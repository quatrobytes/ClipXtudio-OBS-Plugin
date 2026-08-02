# Voice Trigger Pro

## Implementación nativa

Voice Trigger no usa SAPI, el micrófono predeterminado de Windows ni paquetes de
idioma del sistema operativo. El flujo de producción es:

1. enumera fuentes de audio creadas o restauradas por OBS;
2. permite elegir una fuente concreta o **Automático (preferir micrófono OBS)**;
3. recibe sus frames con `obs_source_add_audio_capture_callback`;
4. convierte el formato global de OBS a PCM16 mono de 16 kHz con el resampler de
   `libobs`;
5. segmenta voz en memoria mediante actividad y silencio;
6. transcribe localmente con el modelo multilingüe `whisper.cpp`;
7. normaliza y compara español/inglés con las frases configuradas;
8. envía el evento aceptado a `TriggerEngine`.

El callback de OBS solo convierte, segmenta y encola audio. La inferencia se
ejecuta en un worker para no bloquear el hilo de audio ni la interfaz.

## Modelo local e idiomas

La distribución incluye `data/models/ggml-tiny-q5_1.bin`. El selector admite:

- `auto`: detección automática entre español e inglés;
- `es`: fuerza transcripción en español;
- `en`: fuerza transcripción en inglés.

No se instala ni descarga ningún idioma de Windows. Si el modelo no está
empaquetado, la UI muestra un error y no finge estar escuchando.

## Selección de audio OBS

El campo **Fuente de audio OBS** lista las fuentes con audio visibles en la
colección actual. El modo automático prefiere una fuente de entrada/micrófono y
usa otra fuente de audio solo como fallback.

OBS restaura las fuentes después de cargar los módulos. Por eso el controlador
reintenta el enlace al recibir `OBS_FRONTEND_EVENT_FINISHED_LOADING`. El estado
visible distingue modelo cargando/listo, fuente ausente y recepción de audio.

## Frases iniciales

- `clip`
- `saca clip`
- `guarda eso`
- `eso va pa TikTok`
- `modo grinch`
- `no ombe no`
- `eso fue duro`
- `pa que aprendan`
- `save clip`
- `save that`

El usuario puede agregar hasta 50 frases. La sensibilidad controla confianza de
transcripción y similitud; el cooldown evita clips repetidos.

## Privacidad

En modo local:

- el PCM viene exclusivamente de la fuente OBS seleccionada;
- audio y transcripción temporal permanecen en memoria;
- no se escriben grabaciones ni transcripciones;
- no se envía audio, texto o frases al backend;
- los logs registran estado, no contenido hablado.

El modo Cloud permanece inactivo hasta existir consentimiento y contrato de API.
El plugin no contiene claves privadas de proveedores.

## Estados y errores

- desactivado o bloqueado por plan;
- iniciando/cargando modelo;
- escuchando la fuente OBS seleccionada;
- fuente OBS no disponible;
- modelo local ausente o inválido;
- error de resample/inferencia;
- último texto reconocido, visible solo durante la sesión.

Para `Guardar clip`, Replay Buffer debe estar activo. `Marcar momento` y
`Sugerir clip` generan eventos sin guardar archivo.

## Ajuste de detección 0.4.4

- La actividad de voz se calcula por RMS de cada bloque recibido desde la
  fuente de audio OBS, evitando que picos aislados abran o mantengan segmentos.
- El umbral se deriva de Sensibilidad y conserva un mínimo contra ruido.
- Una frase exacta configurada recibe evidencia completa para TriggerEngine;
  la confianza del modelo sigue bloqueando audio de baja calidad antes del
  matching.
- Voice Trigger necesita Replay Buffer activo cuando la acción es Guardar clip.

## Latencia 0.4.5

- El cierre de frase se produce tras 300 ms de silencio.
- La inferencia empieza inmediatamente después de cerrar la frase y continúa en
  el worker, sin bloquear audio ni UI.
- Si la acción es Guardar clip, el video no se solicita a OBS hasta terminar el
  Post-roll. Esa espera es contenido futuro del clip, no latencia del
  reconocimiento.
- La frase configurada que coincidió se conserva como `trigger_label` en la
  metadata y SQLite.

## Validación

- unidad: segmentación, descarte de ruido corto y límite de duración;
- unidad: matching español con acentos, inglés, sensibilidad y cooldown;
- unidad: gates Free/Pro y paso por `TriggerEngine`;
- UI: selector de fuente, idiomas, frases y estados;
- integración OBS: carga posterior de fuentes, modelo local listo y frames de
  audio recibidos;
- regresión: 46 pruebas CTest completas.
