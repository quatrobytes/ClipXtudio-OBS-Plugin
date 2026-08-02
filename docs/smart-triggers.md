# Smart Triggers

## Alcance

`TriggerEngine` convierte señales normalizadas en eventos de momentos
importantes. Es C++ puro, no enlaza Qt, `libobs`, WebSocket ni SQLite. Los
adaptadores obtienen señales de OBS u otras integraciones; la UI configura el
motor y consulta sus eventos recientes.

## Triggers y acceso

| Trigger | Free | Pro | Señal normalizada |
|---|---:|---:|---|
| Manual | Sí | Sí | `manualMarker` |
| Voice Trigger | No | Sí | `voiceConfidence` |
| Audio Spike | No | Sí | `audioIntensity` |
| Chat Pulse | No | Sí | `chatActivity` |
| Scene Trigger | No | Sí | `sceneRelevance` y escena seleccionada |
| Keyword Trigger | No | Sí | `keywordStrength` y keyword configurada |
| Future AI Hook Finder | No | Sí | `aiConfidence`; detector futuro |

El motor aplica el entitlement aunque una UI o archivo intente activar un
trigger Pro. En v1 el plugin inicia con entitlement Free; la activación real se
conectará a `LicenseManager`.

## Flujo

1. Un adaptador construye `TriggerSignal` con timestamp y valores entre 0 y 1.
2. El motor comprueba activación, entitlement, listas y sensibilidad.
3. `evaluateMoment` puede fusionar varias señales del mismo momento.
4. `ScoreEngine` calcula un score entre 0 y 100.
5. Se aplican ventana de duplicados y cooldown.
6. Se crea `TriggerEvent` con acción, score, contribuyentes y ventana de captura.
7. El evento entra en una cola reciente acotada a 100 elementos.

## Acciones

- `SaveClip`: intención de guardar, protegida por deduplicación y cooldown.
- `MarkMoment`: registra el momento sin forzar un nuevo guardado.
- `SuggestClip`: presenta un candidato al usuario.

El core produce la intención mediante un callback desacoplado. El bootstrap
nativo la conecta con `ClipManager`: espera el post-roll en el hilo Qt y solicita
un clip cuya duración es pre-roll + post-roll. El callback nunca se ejecuta bajo
el mutex del motor y un fallo externo no compromete el procesamiento. Los
eventos Manual no vuelven a guardar porque su acción de captura ya fue iniciada
por el botón o la hotkey.

## Deduplicación y cooldown

- Ventana de duplicado por defecto: 2.5 segundos, aplicada a todo evento.
- Cooldown de clip por defecto: 15 segundos, configurable de 0 a 3600.
- El cooldown largo solo se aplica a `SaveClip`.
- La configuración se persiste en settings schema v5.

## Integraciones actuales

- El botón y las hotkeys de captura generan Manual después de que OBS acepta la
  operación.
- `OBS_FRONTEND_EVENT_SCENE_CHANGED` genera Scene con el nombre de la escena.
- Voice Trigger adquiere frames de una fuente de audio OBS con `libobs`,
  resamplea a 16 kHz mono y reconoce localmente con un modelo multilingüe
  `whisper.cpp`. No depende del micrófono predeterminado, SAPI ni idiomas de
  Windows.
- Audio Spike, Chat Pulse y Keyword conservan contratos y lógica core, pero no
  deben considerarse activos sin su adaptador/integración de entrada.
- Future AI Hook es explícitamente un placeholder y no crea análisis en v0.1.

## Criterios de aceptación medibles

- Manual genera exactamente un evento en Free con score de 0 a 100.
- Todo trigger Pro sin entitlement se rechaza.
- Una señal bajo sensibilidad no genera evento.
- Pre/post-roll coinciden al segundo con la configuración.
- Dos señales dentro de la ventana corta producen como máximo un evento.
- Dos `SaveClip` dentro del cooldown producen como máximo un guardado.
- La lista reciente nunca supera 100 eventos.
- El tab muestra siete triggers y refresca eventos cada 500 ms.
