# ClipXtudio plugin UI

## Vertical — centro de producto

La pestaña **Vertical** se rediseñó tomando como referencia
`Imagenes Demo/vertical.png`, sin sustituir la integración existente con
libobs ni la persistencia de `VerticalCanvasManager`.

### Jerarquía y controles

- El encabezado reúne **Iniciar Replay Buffer**, **Guardar clip**,
  **Crear escena vertical** y el estado real del buffer.
- La vista previa 9:16 vive dentro de una tarjeta propia, conserva el render
  nativo de OBS y mantiene la interacción directa de arrastre y rueda.
- **Capas visibles** permanece debajo de la vista previa y conserva los seis
  toggles existentes.
- Los controles se agrupan visualmente por propósito:
  **Composición**, **Lienzo**, **Posición** y **Plantilla**.
- La acción **Ver vista previa** refresca y enfoca el preview real; no genera
  contenido ficticio.
- La configuración avanzada del encoder del Replay Buffer se mantiene como
  bloque independiente debajo del diseñador.

### Responsive y estados

- En docks amplios, preview y controles se muestran lado a lado.
- En docks estrechos, acciones, preview, capas y controles se apilan sin
  scroll horizontal.
- Los botones de captura se habilitan con el estado real del Replay Buffer y
  reutilizan los callbacks de `ClipManager`.
- Los estados Free/Pro, plantillas bloqueadas y límites existentes no fueron
  modificados.

### Inicio y validación

- **Vertical continúa siendo la pestaña inicial** (`mainTabBar` índice 1).
- El smoke test comprueba el header, las cuatro agrupaciones, la tarjeta de
  preview, una sola acción de crear escena y los callbacks reales de captura.
- Se mantienen las pruebas de resolución, persistencia, preview nativo,
  encuadre, reinicio de OBS y adaptación a 460 px / 1400 px. En anchos
  intermedios el diseñador se apila para que ningún control quede recortado.
- Validación del rediseño: las pruebas `ui.vertical-tab`, `ui.main-dock` y
  `ui.main-dock-render` pasan. La suite general pasa 47 de 48 casos; el único
  caso pendiente es `packaging.rundir-layout` porque OBS mantiene cargado el
  DLL de desarrollo y el enlazador no puede reemplazarlo hasta cerrar OBS.
