# ClipXtudio Studio — Flujo de captura manual

**Estado:** Implementado para Capturar Free  
**Fecha:** 2026-07-28  
**Base técnica:** C++17, OBS Frontend API, libobs y Qt Widgets

## 1. Alcance

Este flujo permite iniciar y detener el Replay Buffer de OBS, solicitar un
guardado manual, correlacionar la respuesta asíncrona, crear metadata y mostrar
el clip dentro de la sesión actual.

No usa Python, Lua, WebSocket, WebView ni una aplicación externa.

## 2. Separación de responsabilidades

```text
MainDock (Qt)
  └─ usa ClipManager
       ├─ valida duración y estado
       ├─ controla una solicitud pendiente
       ├─ crea metadata trigger=manual
       ├─ genera/asegura nombre legible
       └─ mantiene clips de sesión
            └─ usa ReplayManager (interfaz C++ pura)
                 └─ ObsReplayManager
                      ├─ obs_frontend_replay_buffer_start
                      ├─ obs_frontend_replay_buffer_stop
                      ├─ obs_frontend_replay_buffer_save
                      ├─ obs_frontend_replay_buffer_active
                      └─ OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED

SettingsManager
  └─ lee/persiste quick_durations_seconds
```

La UI no incluye headers de OBS ni llama funciones de libobs. `ClipManager`
tampoco depende de OBS y se prueba con un ReplayManager simulado.

## 3. Estados del Replay Buffer

| Estado | UI | Iniciar | Detener | Marcar momento |
|---|---|---:|---:|---:|
| Inactivo | Texto neutral | Sí | No | No |
| Iniciando | Badge ámbar | No | No | No |
| Activo | Badge verde | No | Sí | Sí |
| Deteniendo | Badge ámbar | No | No | No |
| Error | Badge rojo | Sí | Sí | No |

Un inicio que vuelve a `STOPPED` sin haber alcanzado `STARTED` se interpreta
como error recuperable.

## 4. Secuencia de guardado

```text
Usuario        MainDock        ClipManager        ObsReplayManager       OBS
  |               |                 |                    |                |
  | Marcar        |                 |                    |                |
  |-------------->| captureManual   |                    |                |
  |               |---------------->| valida estado      |                |
  |               |                 | save               |                |
  |               |                 |------------------->| save replay    |
  |               |                 |                    |--------------->|
  |               | Guardando…      | pending=manual     |                |
  |               |<----------------|                    |                |
  |               |                 |                    | SAVED          |
  |               |                 | path               |<---------------|
  |               |                 |<-------------------|                |
  |               |                 | verifica archivo                    |
  |               |                 | renombra sin sobrescribir            |
  |               |                 | crea metadata                        |
  |               | clip + contador |                    |                |
  |               |<----------------|                    |                |
```

Sólo se admite una solicitud pendiente. Esto evita asignar el evento `SAVED`
de OBS a una duración o trigger incorrectos.

## 5. Duraciones rápidas

Defaults Free:

| Etiqueta | Valor |
|---|---:|
| 15s | 15 |
| 30s | 30 |
| 60s | 60 |
| 2 min | 120 |
| 5 min | 300 |

`SettingsManager` persiste exactamente cinco valores válidos entre 5 y 300
segundos en `quick_durations_seconds`.

La duración se conserva como intención y metadata. OBS guarda la ventana
configurada en su propio Replay Buffer. El Frontend API no ofrece recorte por
solicitud; por eso este incremento no promete que seleccionar 15s recorte un
buffer configurado a 60s. ClipXtudio tampoco cambia silenciosamente el perfil
de salida del usuario.

## 6. Metadata y archivo

Cada clip manual contiene:

- ID de sesión.
- Ruta final.
- Nombre visible.
- Duración solicitada.
- Timestamp UTC.
- `trigger=manual`.

Formato:

```text
ClipXtudio_YYYY-MM-DD_HH-mm-ss_manual.ext
```

Si el destino existe, se prueba `_2`, `_3`, etc. hasta obtener una ruta libre.
Nunca se sobrescribe un clip. Si el rename falla, se conserva el archivo
original, se incorpora a sesión y se muestra una advertencia.

## 7. Errores y recuperación

| Caso | Comportamiento |
|---|---|
| Replay inactivo | No llama a guardar; muestra acción correctiva |
| Duración inválida | Rechaza antes de tocar OBS |
| Guardado pendiente | Rechaza duplicado |
| OBS no devuelve ruta | Limpia pending y muestra error |
| Archivo inexistente | No crea metadata de éxito |
| Rename falla | Conserva original, registra clip y muestra advertencia |
| Inicio falla | Estado Error; se puede reintentar |

Los logs usan el prefijo `[ClipXtudio Studio]` y no imprimen la ruta completa
del usuario.

## 8. UI Free

Capturar muestra:

- Estado global del Replay Buffer.
- Botones Iniciar y Detener.
- Cinco duraciones seleccionables.
- Marcar momento.
- Guardar clip vertical visible pero deshabilitado con explicación Pro.
- Último clip.
- Contador y cards de la sesión.
- Notificación interna de progreso, éxito o error.

## 9. Criterios de aceptación medibles

- Con estado activo, un clic ejecuta exactamente una solicitud de guardado.
- Con estado inactivo, se ejecutan cero solicitudes y se informa el error.
- Metadata sólo aparece después del evento `SAVED` y de verificar el archivo.
- Todo clip manual contiene `trigger=manual`.
- El nombre cumple el formato y no sobrescribe archivos existentes.
- El contador aumenta exactamente una unidad por clip correlacionado.
- La notificación aparece en el siguiente ciclo del event loop de Qt.
- Construcción y flujo se prueban sin abrir OBS mediante mocks.
- El módulo carga y descarga sin crash en el smoke test certificado.

## 10. Pendiente posterior

- Persistencia SQLite entre sesiones.
- Hotkey configurable.
- Estabilización adicional del archivo para contenedores/almacenamiento lento.
- Recorte exacto mediante ExportManager.
- Miniaturas y reproducción.
- Guardado vertical real y entitlement Pro.
