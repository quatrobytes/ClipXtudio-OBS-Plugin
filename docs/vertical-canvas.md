# Vertical Canvas v1

## Designer layout 0.5.4

- La composición OBS 9:16 ocupa la columna principal izquierda y se escala
  dinámicamente entre 270×480 y 360×640, siempre con proporción exacta.
- `Capas visibles` vive en una sección independiente debajo del preview y usa
  una cuadrícula de dos columnas.
- Explicación, selección de escena/fuente, zoom, posición, modo, resolución y
  plantilla permanecen en la columna derecha.
- Por debajo de 720 px el orden responsive es preview, capas visibles y
  configuración. El contenido mantiene scroll vertical y nunca habilita scroll
  horizontal.

## Preview nativo de escenas OBS (v0.5.0)

La pestaña Vertical integra un `obs_display` dentro de un widget Qt nativo. El
usuario selecciona una escena de la colección activa y puede mostrar la escena
completa o una fuente directa, incluida una cámara. libobs renderiza esa fuente
en vivo con recorte 9:16.

El encuadre admite zoom (100–300 %) y desplazamiento horizontal/vertical
(-100–100 %). La escena, fuente y geometría se persisten en el archivo de
ajustes. `Crear escena vertical de ClipXtudio` genera una escena OBS real
`ClipXtudio Vertical` a partir de la selección actual.

La capa UI recibe `VerticalObsBridge`; no enlaza directamente con libobs. La
implementación OBS vive en `src/plugin/obs-vertical-preview.cpp`, manteniendo
los tests Qt ejecutables sin abrir OBS.

La nota histórica de v1 que aparece más abajo ya no describe el preview de
v0.5.0: el preview actual sí es un render libobs real. El plugin no inicia
silenciosamente un segundo encoder, porque duplicaría el consumo; la
exportación 9:16 continúa procesando el clip guardado con el motor multimedia
incluido.

## Encoder del Replay Buffer (0.5.5)

La sección de rendimiento consulta el perfil activo mediante la Frontend API y
enumera los encoders H.264 de video que libobs tiene registrados. Permite:

- elegir un encoder hardware disponible o x264;
- habilitar Replay Buffer en el perfil activo;
- conservar un solo buffer nativo para horizontal y vertical;
- aplicar el cambio únicamente con todas las salidas detenidas;
- guardar con archivo temporal y backup.

OBS crea sus encoders al cargar el perfil. Por seguridad, el cambio requiere
reiniciar OBS y la UI lo comunica inmediatamente. La funcionalidad no controla
ni inicia buffers de plugins externos.

## Alcance entregado

Vertical Canvas v1 permite configurar y persistir una composición 9:16 desde el
dock nativo de OBS. Incluye preview Qt, modos de salida, resoluciones, plantillas
y seis elementos activables. Esta versión modela la composición y deja contratos
estables para conectarla posteriormente a un renderer libobs/off-screen.

No crea todavía una segunda salida de video ni reescala archivos. Mostrar el
preview no se considera render vertical real.

## Modos de salida

| Modo | Comportamiento configurado |
|---|---|
| Horizontal 16:9 | Mantiene el flujo de captura horizontal |
| Vertical Canvas 9:16 | Selecciona el flujo vertical para el futuro renderer |
| Ambos | Solicita horizontal y vertical |

El valor se comparte con Ajustes y con la hotkey de alternar output.

## Resoluciones

- 720p vertical: 720×1280.
- 1080p vertical: 1080×1920.
- 2K vertical: 1440×2560.
- 4K vertical: 2160×3840.
- 8K vertical: 4320×7680.
- Custom, siempre con relación exacta 9:16.

La validación usa enteros (`width * 16 == height * 9`) para evitar errores de
redondeo. Dimensiones cero, negativas o con otra relación se rechazan antes de
modificar el archivo activo. Seleccionar una resolución configura el canvas y
la resolución objetivo del proceso vertical; la capacidad de codificar 4K u 8K
depende del encoder, la GPU, la memoria y los límites del codec elegidos.

## Plantillas

| Plantilla | Plan | Composición inicial |
|---|---|---|
| Gaming Vertical | Free | gameplay central, cámara y subtítulos |
| Talking Head | Pro | cámara dominante, título y subtítulos |
| Tech Review | Pro | gameplay/producto, cámara, título y logo |
| Product Review | Pro | producto central, cámara, título y logo |

En Free, las plantillas Pro permanecen visibles pero deshabilitadas. El manager
también valida el entitlement; la restricción no depende únicamente de la UI.

## Elementos

`VerticalElement` representa Gameplay, Cámara, Subtítulos, Título, Logo o Chat.
Cada elemento contiene:

- estado activado;
- geometría normalizada `x/y/width/height`;
- orden Z.

La geometría no se expone todavía en el editor, pero permite evolucionar el
modelo sin reemplazar los settings cuando se integre el renderer real.

## Arquitectura

```text
VerticalTab (Qt)
 ├─ controles y mensajes localizados
 └─ VerticalPreviewWidget (paintEvent, preview 9:16)
          │
          ▼
VerticalCanvasManager (core, sin Qt/libobs)
 ├─ validación de modo/resolución/entitlement
 ├─ catálogo VerticalLayoutTemplate
 └─ SettingsManager
          │
          ▼
settings.json schema v3
```

`VerticalCanvasManager` es la única entrada de escritura del tab. La UI no
serializa JSON ni conoce rutas. La hotkey de modo output también llama al manager.

## Persistencia

Schema local v3:

- `output_mode`;
- `vertical_resolution`;
- `vertical_width` y `vertical_height`;
- `vertical_template`;
- seis flags `vertical_element_*`.

Los archivos v1 migran a defaults v3. Los archivos v2 conservan sus dimensiones:
1080×1920 se convierte al preset 1080; otras dimensiones 9:16 anteriores se
conservan como Custom.

La escritura continúa siendo temporal + publicación atómica mediante
`SettingsManager`.

## Free y Pro

- Free puede configurar el preview, usar Gaming Vertical y preparar el contrato
  de export vertical básico.
- Pro habilita plantillas avanzadas y será el único plan con canvas vertical
  libobs real.
- La fuente de verdad futura del entitlement será `LicenseManager`; v1 inyecta
  explícitamente `proUnlocked=false` porque la activación de licencia aún no está
  conectada.

El export/re-render de media y la salida simultánea real no forman parte de este
incremento. Deben implementarse mediante `ExportManager`/renderer, nunca mediante
una etiqueta falsa sobre un clip horizontal.

## Criterios de aceptación

1. El combo de output contiene Horizontal, Vertical Canvas y Ambos.
2. El combo de resolución contiene 720p, 1080p, 2K, 4K, 8K y Custom.
3. Custom rechaza cualquier dimensión que no sea exactamente 9:16.
4. Se muestran cuatro plantillas; Gaming es seleccionable en Free.
5. Gameplay, Cámara, Subtítulos, Título, Logo y Chat pueden alternarse.
6. El preview mantiene orientación 9:16 y refleja elementos activos.
7. Modo, resolución, plantilla y elementos sobreviven a una nueva instancia.
8. Una plantilla Pro no puede activarse con entitlement Free.
9. Los presets 2K, 4K y 8K persisten sus dimensiones exactas 1440×2560,
   2160×3840 y 4320×7680.
