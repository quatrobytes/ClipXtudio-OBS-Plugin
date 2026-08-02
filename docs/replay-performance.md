# Rendimiento del Replay Buffer

## Diagnóstico verificado en Windows

El perfil OBS `Perfil_MrJimeneX__Stream` está configurado a 3840 × 2160 y
60 FPS. Su Replay Buffer usa `obs_x264`, que codifica en CPU. El mismo perfil
dispone del encoder AMD AMF (`h264_texture_amf`), pero no está seleccionado
para el Replay Buffer. Esa combinación 4K60 + x264 explica el salto cercano al
80 % de CPU al encender el buffer.

ClipXtudio no codifica el Replay Buffer: `ObsReplayManager` delega
start/stop/save a la OBS Frontend API. El dock, SQLite y la metadata no crean
un encoder adicional.

## Control integrado desde 0.5.5

**Vertical → Rendimiento del Replay Buffer** permite elegir un encoder H.264
registrado por OBS y habilitar Replay Buffer sin abrir Ajustes de OBS.
ClipXtudio escribe el perfil activo con guardado seguro, pero no sustituye en
caliente los objetos de salida propiedad de OBS. El cambio entra en vigor
después de reiniciar OBS.

La acción se bloquea si streaming, grabación o Replay Buffer están activos.
Resolución y FPS continúan siendo propiedades generales del perfil y no se
modifican desde este control.

## Configuración recomendada

1. En OBS, abrir **Ajustes > Salida > Grabación**.
2. Seleccionar **AMD HW H.264 (AVC)** / AMF en vez de x264.
3. Si se transmite y se desea reutilizar el mismo encode, usar la opción de
   encoder de transmisión cuando esté disponible.
4. Si todavía hay sobrecarga, bajar la salida de grabación/replay a 1920 ×
   1080 o 1280 × 720 y/o 30 FPS.

El encoder se modifica solamente al pulsar **Aplicar al perfil de OBS**.
Resolución y FPS no se cambian automáticamente porque afectan grabación y
streaming.

## OBS horizontal y Aitum Vertical

Aitum mantiene un Backtrack/Replay vertical separado. Encender simultáneamente
el Replay Buffer horizontal de OBS y el Backtrack vertical de Aitum crea dos
pipelines de render/encode y aumenta el consumo.

La versión revisada de Aitum expone start/stop/save del Backtrack mediante su
API vendor de obs-websocket, pero no publica esos comandos en su API nativa
`proc_handler` de canvas. ClipXtudio no usa WebSocket como base ni se acopla
a métodos Qt internos no soportados. Por tanto, en esta versión:

- ClipX controla de forma nativa el Replay Buffer horizontal de OBS.
- Para menor consumo, se recomienda mantener un solo buffer activo.
- El clip vertical básico se deriva en exportación desde el clip horizontal.
- El Backtrack vertical se administra en el dock de Aitum hasta que exista una
  API nativa pública/estable o se implemente un adaptador opcional aislado.

Esta limitación evita un control frágil que podría romper OBS al actualizar
Aitum.
