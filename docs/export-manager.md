# ExportManager MVP

## Alcance

ExportManager procesa clips sin bloquear el thread de OBS/Qt. El MVP exporta:

- horizontal MP4 H.264/AAC;
- vertical MP4 9:16 mediante scale + center crop;
- horizontal y vertical (`Both`) como dos jobs independientes;
- lotes de clips desde la pestaña Clips;
- un source vertical pre-renderizado cuando `Vertical Canvas` lo proporcione.

## Arquitectura

```text
ClipsTab (Qt main thread)
 ├─ selección, modo, progreso y cancelar
 └─ DesktopClipActionService
      └─ ExportManager (core C++17)
           ├─ cola FIFO + worker std::thread
           ├─ estados/progreso/cancelación
           ├─ publicación temporal atómica
           ├─ callback de metadata
           └─ ExportBackend
                └─ FfmpegExportBackend (QProcess en worker)

ExportManager callback
 └─ ClipLibraryService worker
      ├─ ExportJobRepository
      └─ ClipRepository::setExportStatus
```

`ExportManager` no incluye Qt, libobs, SQLite ni headers FFmpeg. El backend y la
persistencia son adaptadores. Los unit tests usan un backend simulado.

## Estados

```text
Pending → Exporting → Done
                    ├→ Error
                    └→ Cancelled
Pending ─────────────→ Cancelled
```

- `Pending`: aceptado en memoria y publicado al repositorio.
- `Exporting`: FFmpeg está activo.
- `Done`: temporal publicado como MP4 final y progreso 100%.
- `Error`: temporal eliminado y error legible conservado.
- `Cancelled`: proceso terminado, temporal eliminado y original intacto.

## Pipeline FFmpeg

El backend ejecuta FFmpeg sin shell mediante `QProcess`, por lo que las rutas no
se interpretan como comandos.

### Horizontal

- video H.264 `libx264`;
- pixel format `yuv420p`;
- audio AAC;
- `+faststart`;
- conserva la geometría horizontal de entrada.

### Vertical center crop

```text
scale=W:H:force_original_aspect_ratio=increase,
crop=W:H,
setsar=1
```

El crop es centrado. El audio se recodifica a AAC y mantiene el timeline de la
entrada.

### Vertical Canvas

Si `verticalSource=VerticalCanvas`, la solicitud debe incluir
`verticalCanvasPath`, un archivo vertical pre-renderizado por el futuro renderer
libobs. Se escala con `decrease` y se aplica pad 9:16. Una solicitud Canvas sin
source se rechaza; no cae silenciosamente a crop.

En el runtime actual, `DesktopClipActionService` solicita `CenterCrop` porque
VerticalCanvas v1 todavía no produce un stream/archivo renderizado.

## Presets

| Preset | x264 preset | CRF | Audio |
|---|---:|---:|---:|
| Bajo | ultrafast | 30 | 96 kbps |
| Medio | veryfast | 26 | 128 kbps |
| Alto | medium | 21 | 160 kbps |
| Máximo | slow | 17 | 192 kbps |

Los cuatro presets son valores cerrados y se validan antes de encolar.

## Progreso y cancelación

FFmpeg se inicia con:

```text
-progress pipe:1 -stats_period 0.25 -nostdin
```

El backend interpreta `out_time_us` contra la duración conocida. Durante encode
el progreso se limita a 99%; 100% solo se publica después de verificar y renombrar
el temporal.

Cancelar activa un flag atómico. El backend envía terminate, espera hasta un
segundo y aplica kill como fallback. La UI sondea snapshots thread-safe cada
250 ms y nunca espera al worker.

## Rutas y colisiones

- El nombre base elimina caracteres inseguros y compacta separadores.
- Se añade `_horizontal` o `_vertical`.
- El formato final siempre es `.mp4`.
- Si existe un destino o ya está reservado por otro job, se asigna `_2`, `_3`,
  etc.
- FFmpeg recibe `-n`, por lo que tampoco sobrescribe ante una carrera externa.
- FFmpeg escribe `*.part.mp4`; el manager publica el archivo final solamente si
  el encoder terminó correctamente.

## Persistencia SQLite

La migración DB v3 crea `exports`:

| Campo | Uso |
|---|---|
| `id` | ID estable del job |
| `clip_id` | FK al clip original |
| `source_path` | entrada |
| `output_path` | destino reservado |
| `orientation` | horizontal/vertical |
| `preset` | low/medium/high/maximum |
| `state` | estado del job |
| `progress_percent` | 0–100 |
| `error` | último error |
| `created_at`, `updated_at` | auditoría |

Cada transición actualiza también `clips.export_status`. Las escrituras pasan por
el worker existente de `ClipLibraryService`.

## Dependencia y empaquetado

Windows incluye FFmpeg 8.1.2 x64 en
`data/tools/ffmpeg/ffmpeg.exe`. `plugin-main` resuelve la ruta con
`obs_module_file()` y la inyecta a `FfmpegExportBackend`; producción no consulta
`PATH` ni requiere una instalación externa.

El paquete conserva `LICENSE`, `README.txt`, URL de origen y checksums del
archivo. El build Essentials verificado incluye H.264 `libx264` y AAC.

## Limitaciones técnicas del MVP

- Un solo worker: los lotes son secuenciales para limitar CPU/GPU.
- Codec fijo H.264/AAC y contenedor MP4.
- Crop vertical centrado, sin tracking de sujeto.
- Sin aceleración NVENC/QSV/AMF/VideoToolbox.
- Sin estimación previa de espacio libre.
- La duración usada para progreso proviene de metadata, no de `ffprobe`.
- No hay recuperación automática de jobs interrumpidos al reiniciar.
- Vertical Canvas real depende de un source pre-renderizado aún no disponible.
- El entitlement de batch/cuotas no está conectado porque `LicenseManager`
  todavía no está implementado.

## Criterios verificados

1. Encolar devuelve inmediatamente y FFmpeg corre en el worker.
2. Un fixture 640×360 con audio genera MP4 horizontal.
3. El mismo fixture genera MP4 vertical 720×1280 validado con `ffprobe`.
4. `Both` produce dos jobs y dos archivos.
5. Un lote expande y drena todos sus jobs.
6. La UI muestra progreso, resultado y cancelación.
7. Un error del backend llega como `Error` con mensaje concreto.
8. Destinos existentes no se sobrescriben.
9. Los jobs sobreviven en SQLite schema v3.
10. Un runtime ausente solicita reinstalar ClipXtudio sin pedir al usuario que
    instale FFmpeg.
