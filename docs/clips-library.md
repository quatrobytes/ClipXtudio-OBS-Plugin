# Biblioteca de clips

**Estado:** implementada  
**Alcance:** biblioteca local de la sesión activa dentro del dock nativo de OBS

## Objetivo

La pestaña Clips permite descubrir, filtrar y accionar los clips persistidos sin
consultar SQLite desde la UI ni bloquear el hilo principal de OBS.

## Arquitectura

```text
ClipsTab (Qt Widgets)
  |
  v
ClipLibraryController
  |-----------------------> ClipLibraryViewModel (C++ puro)
  |                              |
  |                              +-- filtros, búsqueda, orden y resumen
  |
  +--> ClipLibraryService ------> worker FIFO
  |                              |
  |                              +-- ClipRepository / SessionRepository
  |                                  |
  |                                  +-- SQLite
  |
  +--> ClipActionService
         |
         +-- DesktopClipActionService
               |
               +-- ExportManager -> FFmpeg worker
```

Reglas de dependencia:

- `ClipsTab` no incluye ni instancia repositorios.
- `ClipLibraryController` es el único coordinador de estado de la pestaña.
- `ClipLibraryViewModel` no depende de Qt, OBS ni SQLite.
- `ClipLibraryService` ejecuta todas las consultas y escrituras en un worker.
- Preview, caption, subtítulos, carpeta y exportación pasan por
  `ClipActionService`.
- La exportación individual y por selección se encola en `ExportManager`; el
  tab únicamente presenta progreso y cancelación.

## Modelo de vista

| Filtro | Regla |
|---|---|
| Todos | No restringe por categoría |
| Favoritos | `is_favorite = true` |
| Verticales | orientación `vertical` o `both` |
| Pendientes | `export_status = pending` |

La búsqueda ignora mayúsculas sobre título y nombre de archivo. El orden
predeterminado es fecha descendente; el usuario puede elegir score descendente.
Los empates se resuelven por fecha e ID.

## Resumen

Las cards superiores muestran clips totales, score máximo y duración de sesión.
Para una sesión cerrada se usa `ended_at - started_at`; para una sesión activa,
`now - started_at`. El panel inferior añade favoritos, verticales, pendientes y
duración acumulada del material capturado.

## Estados y acciones

- **Carga:** indicador visible; consulta fuera del hilo UI.
- **Vacío:** `EmptyState` tras una consulta sin resultados.
- **Error:** notificación interna; OBS continúa operativo.
- **Favorito:** actualización optimista; un fallo SQLite revierte el estado.
- **Preview:** abre el archivo mediante el adaptador de escritorio.
- **Caption:** abre transcript o presenta el caption persistido.
- **Subtítulos:** abre `subtitle_path`; se deshabilita si no existe metadata.
- **Abrir carpeta:** abre el directorio padre del clip.
- **Crear copia 9:16:** el original capturado ya existe; esta acción crea una
  variante adicional mediante center crop real.
- **Ya está en 9:16:** un clip vertical deshabilita una conversión redundante.
- **Crear copias de selección:** orientación horizontal, vertical o ambas.
- **Progreso/cancelación:** estado del job activo sin bloquear Qt.

## Actualización tras captura

`ClipManager` encola primero la persistencia y después notifica a `MainDock`.
`MainDock::handleClipSaved` recarga `ClipsTab`. La cola FIFO de
`ClipLibraryService` garantiza que la lectura quede detrás del insert.

Desde 0.5.6, el motor FFmpeg incluido extrae un frame en segundo plano después
del guardado. `ClipLibraryService::updateThumbnail` persiste `thumbnail_path` y
solicita otra recarga de la biblioteca. La captura no espera la miniatura: si
el proceso falla, el original permanece intacto y la tarjeta usa placeholder.

La composición de cada `ClipCard` sigue este orden:

```text
[selección] [thumbnail 16:9 + duración] [título / fecha / trigger] ... [acciones] [favorito] [score]
```

`trigger_label` tiene prioridad visual sobre el tipo genérico. Esto permite
mostrar la frase de voz (`saca clip`), escena o señal concreta; `Manual` solo
se usa cuando la captura realmente fue manual.

## Rendimiento

- SQLite y FFmpeg trabajan en threads dedicados independientes.
- Clips y sesión se obtienen en una sola tarea.
- La carga global se limita a 1.000 entradas.
- La búsqueda tiene debounce de 180 ms.
- Las cards se crean en lotes de 20 por turno del event loop.
- Respuestas y renders obsoletos se descartan mediante generaciones.

Esto mantiene receptivo el event loop, pero no equivale a virtualización total.
Si el uso real supera habitualmente 1.000 clips por sesión, v2 debe adoptar
paginación o `QAbstractItemModel`.

## Criterios de aceptación medibles

1. Existen tres cards, cuatro filtros, búsqueda, orden, lista y resumen.
2. Favoritos devuelve exclusivamente clips favoritos.
3. Verticales incluye `vertical` y `both`.
4. La búsqueda encuentra títulos sin distinguir mayúsculas.
5. Score y fecha ordenan de forma descendente y estable.
6. El resumen calcula total, mejor score y duración exactos.
7. Cada card presenta thumbnail/placeholder, duración, título, fecha, trigger,
   score, favorito y cinco acciones de contenido.
8. Favorito persiste en SQLite; ante fallo se revierte la UI.
9. Un clip guardado aparece después de la persistencia sin reabrir el dock.
10. Ninguna consulta SQLite se ejecuta en el hilo UI.
11. Se crean como máximo 20 cards por turno del event loop.
12. Los tests unitarios y smoke Qt pasan en modo offscreen.

## Limitaciones actuales

- Las acciones de sistema requieren rutas existentes y asociaciones del SO.
- Vertical Canvas real requiere que el renderer futuro entregue un source
  pre-renderizado; el runtime actual usa center crop.
- FFmpeg 8.1.2 x64 está empaquetado por ZIP e instalador dentro del plugin.
- Windows/Visual Studio 2022 aún requiere validación en un host Windows con OBS.
## Eliminación de clips (0.5.17)

- La papelera roja de cada tarjeta elimina un solo clip.
- Las casillas conservan la selección y habilitan **Eliminar seleccionados** para
  operaciones por lote.
- Ambas acciones requieren confirmación explícita; **Cancelar** es la respuesta
  predeterminada.
- Se eliminan permanentemente el video original, thumbnail, transcripción,
  subtítulos y el registro SQLite. No se eliminan carpetas ni copias exportadas
  independientes.
- La operación de archivos y SQLite se ejecuta fuera del hilo de UI. El dock
  muestra éxito o un error controlado sin bloquear ni cerrar OBS.

## Clip card y alcance del historial (0.5.3)

- Cada tarjeta agrupa a la izquierda selección, duración/thumbnail, título,
  fecha y trigger. Las acciones permanecen en una sola fila a la derecha.
- Preview, copia 9:16, caption, subtítulos, carpeta y favorito usan iconos
  vectoriales nativos con tooltip y nombre accesible.
- La biblioteca abre en `Sesión actual`. Esto evita mezclar de forma
  inesperada clips persistidos durante ejecuciones anteriores de OBS.
- Pro puede seleccionar `Todo el historial guardado`; ese alcance consulta la
  SQLite local sin `session_id` y muestra explícitamente su procedencia.
- Cambiar el alcance vuelve a consultar de forma asíncrona y no bloquea OBS.
