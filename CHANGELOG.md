# Changelog

## [0.5.99] - 2026-08-02

- Vertical is now the default clip format for new profiles and the first-run assistant.
- Skipping first-run setup keeps the safe Vertical default instead of falling back to Horizontal.
- Basic 9:16 export remains available to Free users; only dual Horizontal + Vertical output requires Pro.

## [0.5.98] - 2026-08-02

- Fixed Quick Clip Editor exports of vertical library items so the encoded MP4
  uses a real 9:16 frame instead of preserving a 16:9 replay canvas with black
  side bars.
- Completed vertical exports are now registered as independent vertical clips
  in the library, alongside the original horizontal capture.
- The **Both** capture description now states that it creates two selectable
  library variants.

## [0.5.97] - 2026-08-02

- Replaced the contextual Quick Clip Editor delete label with **Delete cut** for
  one selected block and **Delete selected (N)** for multiple selected blocks.
- Added a trash icon to the timeline deletion action.
- Smart Trim no longer renames the deletion action to **Delete silences**; the
  action now describes the user's current selection consistently.

## [0.5.96] - 2026-08-02

- Decoupled timeline block selection from playback range in the Quick Clip
  Editor.
- Play and Space now continue through every retained block from the blue
  playhead, skipping deleted ranges and advancing across ripple boundaries.
- Reworked preview playback as a retained-segment sequence so a decoder process
  ending at one cut can no longer stop the complete edited preview.

## [0.5.95] - 2026-08-02

- Added Ctrl+click multi-selection for non-adjacent Quick Clip Editor blocks.
- Delete and the contextual action now remove all selected blocks in one ripple
  edit while preventing deletion of the complete timeline.
- Smart Trim now selects every detected silence candidate and changes the
  contextual action to Delete silences; users can Ctrl+click to keep any
  candidate before confirming.
- Tightened silence detection to sustained near-zero audio at -50 dB for at
  least 650 ms, reducing false cuts caused by quiet speech or room tone.

## [0.5.94] - 2026-08-02

- Replaced the export-complete message box with a spacious persistent result
  dialog that only closes through its Close action.
- Renamed the preview action to Play file and kept the dialog open while the
  exported video or its containing folder is opened.
- Added a read-only full output path and a dedicated Copy path action in both
  English and Spanish.

## [0.5.93] - 2026-08-02

- Changed Quick Clip Editor to a split-select-delete workflow modeled on common
  desktop timeline editors.
- Added selectable timeline blocks, `Ctrl+B` split and `Delete` removal.
- Added ripple-style visual joining after a block is removed; export continues
  to concatenate only retained source ranges.
- Smart Trim now creates orange silence candidates and never deletes content
  automatically.

## [0.5.92] - 2026-08-02

- Rebuilt the Quick Clip Editor timeline with time ticks, a thumbnail filmstrip,
  audio waveform and a draggable playhead.
- Added non-destructive manual range removal with red cut previews and undo.
- Added local Smart Trim silence detection with a blocking analysis state.
- Added multi-segment FFmpeg export so removed ranges are excluded from the
  final MP4 while keeping audio and video synchronized.
- Added English and Spanish UI strings plus automated duration/export coverage.




















## [0.5.91] - 2026-08-02

- Added blocking caption progress with live audio-analysis percentage, stage updates, error propagation, and a finite timeout in the Quick Clip Editor.

## [0.5.90] - 2026-08-02

- Added an export-complete dialog with exact selection duration, file name, open file, and open location actions.

## [0.5.89] - 2026-08-02

- Improved Quick Clip Editor timeline seeking, Space playback, compact transport controls, and smoother high-resolution 30 FPS previews.

## [0.5.88] - 2026-08-02

- Fixed OBS 32.2 plugin loading by replacing the incompatible Qt Multimedia runtime with the bundled FFmpeg preview pipeline.

## [0.5.87] - 2026-08-02

- Added the Quick Clip Editor with inline playback, precise trimming, captions, quality and FPS controls, and MP4 export.

## [0.5.86] - 2026-08-02

- Clarified onboarding voice commands and configured recognized setup phrases to save clips.

## [0.5.85] - 2026-08-02

- Added a bilingual five-step first-run setup assistant for Free workflows, optional Pro activation, version-aware completion, and manual reopening from Settings.

## [0.5.84] - 2026-08-01

- Added Free and Pro JSON settings profile export/import for complete ClipXtudio workflow preferences, with validation and sensitive credential exclusion.

## [0.5.83] - 2026-08-01

- Moved capture progress and result feedback into a transient header status and cleared stale save states when Replay Buffer is inactive.

## [0.5.82] - 2026-08-01

- Added a single persisted Horizontal, Vertical, or Both default clip format selector in Capture and applied it consistently to manual buttons, standard hotkeys, and voice-trigger saves, with explicit bilingual source guidance.

## [0.5.81] - 2026-08-01

- Remote Clipper can now be turned on or off from the header with a localized blocking connection flow validated by the backend.

## [0.5.80] - 2026-08-01

- Remote status no longer flashes red during polling, pulses while online, and localizes the authenticated label.

## [0.5.79] - 2026-08-01

- Remote Clipper header controls now show user and live red-green status icons without a duplicate footer action.

## [0.5.78] - 2026-08-01

- Remote authentication ignores stale revoked-token responses and moves compact controls to the header.

## [0.5.77] - 2026-08-01

- Remote Clipper supports existing signed plugin tokens and reports authorization error codes.

## [0.5.76] - 2026-08-01

- Remote Clipper is now easier to launch from Settings, Capture, and the global footer.

## [0.5.75] - 2026-08-01

- Corrige la autenticación de Remote Clipper para confirmar la conexión solo después de validar heartbeat y lectura de comandos, y muestra los errores reales del backend.

## [0.5.74] - 2026-08-01

- Nueva compilación estable con las mejoras recientes de Remote Clipper y su interfaz.

## [0.5.73] - 2026-08-01

- Corrige los dialogos de Remote Clipper y anade concesiones administrativas de cortesia para el add-on.

## [0.5.72] - 2026-08-01

- Make Remote Clipper authentication a visible blocking workflow with progress, timeout and terminal dialogs.
- Distinguish a stale token from a missing add-on or inactive remote session.
- Add a direct link to the owner Remote Clipper web dashboard for add-on and session setup.

## [0.5.71] - 2026-08-01

- Add an explicit Remote Clipper authentication action that refreshes the current device license token.
- Resume Remote Clipper polling immediately after successful authentication.
- Localize all Remote Clipper header states in Spanish and English.

## [0.5.70] - 2026-08-01

- Stack Remote Clipper status cards vertically when the OBS dock is narrow.
- Restore the three-column status layout automatically when enough width is available.

## [0.5.69] - 2026-08-01

- Reorganized Remote Clipper settings into clear status cards with complete Spanish and English localization.


## [0.5.68] - 2026-08-01

- Added acknowledged remote command processing, claimed-command leases and recovery, and a durable result outbox.














## [0.5.67] - 2026-08-01

- Devuelve el control manual del Replay Buffer, acelera el reconocimiento local de comandos de voz y corrige el espaciado de Eventos recientes.

## [0.5.66] - 2026-08-01

- Estandariza captions sociales en un párrafo de longitud consistente con cinco hashtags separados.

## [0.5.65] - 2026-08-01

- Agrega spinner animado y mensajes inteligentes por etapa y demora durante la generación de captions.

## [0.5.64] - 2026-08-01

- Inicia y mantiene Replay Buffer automáticamente para triggers que guardan clips, conservando reconocimiento local de bajo consumo.

## [0.5.63] - 2026-08-01

- Reduce la latencia del reconocimiento de voz y presenta los eventos recientes en una sección completa y legible.

## [0.5.62] - 2026-08-01

- Agrega configuración inicial de escena y micrófono, prioriza Mic/Aux de OBS y recupera automáticamente la captura de voz.

## [0.5.61] - 2026-08-01

- Mantiene compatibilidad con respuestas antiguas del backend, recorta hashtags a cinco sin rechazar el caption y localiza errores de respuesta inválida.

## [0.5.60] - 2026-08-01

- Mueve los botones de copia debajo de cada caption, renombra YT Shorts y genera captions sociales con al menos dos párrafos orientados a SEO.

## [0.5.59] - 2026-08-01

- Separa captions para TikTok, Instagram y Facebook de YouTube Shorts, limita redes a cinco hashtags y Shorts a 100 caracteres.

## [0.5.58] - 2026-08-01

- Persiste la última escena de composición vertical y restaura ClipXtudio Vertical por defecto tras reiniciar OBS.

## [0.5.57] - 2026-08-01

- Elimina las alertas sonoras de guardado, hace que Trigger respete la ventana completa del clip y retira de Ajustes la captura por defecto duplicada.
- Conserva el estado pendiente desde que se detecta la frase y espera el post-roll completo antes de guardar.
- Sincroniza `RecRBTime` de OBS con la ventana configurada y desactiva el límite `RecRBSize` que podía truncar replays con alta tasa de bits.
- Rechaza de forma explícita una captura cuando el Replay Buffer aún no contiene el historial solicitado.
- Mantiene las confirmaciones dentro de OBS y elimina `QApplication::beep` y los avisos de bandeja de Windows.

## [0.5.56] - 2026-08-01

- Corrige la puntuación de clips con evaluación AI persistida, puntúa capturas manuales, acelera Voice Trigger y muestra claramente el procesamiento pendiente.
- Consume y persiste `quality_score` y `hook_strength` devueltos por el backend, y refresca la biblioteca al terminar el análisis.
- Reduce el cierre por silencio de Voice Trigger a 180 ms y elimina la segunda espera de post-roll tras reconocer una frase.
- Distingue una captura original ya guardada de una exportación realmente pendiente y repara los estados históricos incorrectos mediante la migración SQLite 6.
- Añade indicadores rojos intermitentes durante una captura aceptada y en las tarjetas con exportación pendiente o activa.

## [0.5.55] - 2026-08-01

- Muestra progreso real por etapas al validar la licencia, extraer audio, transcribir con Whisper y solicitar el caption al backend.
- Añade porcentaje, estado actual y tiempo restante estimado con cuenta regresiva.
- Amplía el cuadro de progreso y reserva altura para que los textos no queden cortados.

## [0.5.54] - 2026-08-01

- Bloquea toda la interfaz del plugin con un indicador de progreso mientras valida la licencia, transcribe y genera un caption.
- Distingue las licencias rechazadas por el backend y muestra una alerta clara de licencia no válida.
- Elimina el aviso de error residual cuando un caption sí fue generado o ya estaba almacenado.

## [0.5.53] - 2026-08-01

- Escala las fuentes de cámara al 100 % del alto del lienzo vertical y las
  centra horizontalmente sin deformar su relación de aspecto.
- Alinea la fuente `MontillaRX Kick` al ancho en la zona inferior y garantiza
  que quede por delante de la cámara en `ClipXtudio Vertical`.
- Aplica el encuadre al crear la escena o al pulsar nuevamente Crear escena
  vertical sobre una escena administrada existente, sin modificar otras
  escenas de OBS.

## [0.5.52] - 2026-08-01

- Conserva el error real del backend de IA cuando no existe otro endpoint
  seguro disponible, en lugar de reemplazarlo por un falso error de
  configuración del plugin.
- Alinea la espera del cliente con el límite de 120 segundos del proveedor
  configurado para no cancelar inferencias válidas a los 20 segundos.
- Renueva la licencia y reintenta una vez cuando el backend devuelve los
  códigos reales `LICENSE_TOKEN_*` de autorización vencida o revocada.
- Presenta mensajes localizados y accionables para fallos de conexión y de
  configuración del proveedor de IA.

## [0.5.51] - 2026-08-01

- Incluye `ClipXtudio Vertical` y cualquier otra escena real de la colección
  actual de OBS en el selector de Composición del plugin.
- Conserva la escena vertical seleccionada y su zoom y posición guardados en
  lugar de reemplazarlos automáticamente por otra escena.

## [0.5.50] - 2026-08-01

- Valida la licencia guardada contra ClipXtudio Hub antes de solicitar captions
  y deja de presentar una sincronización pendiente como si fuera un plan
  completamente disponible.
- Añade `Cambiar licencia` a Cuenta Pro y exige una activación válida del
  backend antes de sustituir la licencia del dispositivo.
- Limita la espera de la transcripción local y muestra errores recuperables en
  lugar de dejar indefinidamente el caption en análisis de audio.
- Restaura la vista previa vertical de la escena real de OBS, elimina el bloque
  de plantillas y capas que interfería con el encuadre y evita modificar
  automáticamente la escena administrada.
- Reorganiza Composición debajo de la vista previa y agrupa Lienzo y Posición
  en la misma fila, conservando el control para ampliar la vista previa.


## [0.5.49] - 2026-07-31

- Sincroniza automáticamente la licencia guardada con ClipXtudio Hub al abrir
  OBS y conserva la autorización local únicamente como contingencia sin red.
- Serializa las solicitudes de refresh concurrentes para que el arranque y la
  generación de captions no roten o invaliden la misma credencial entre sí.
- Recupera credenciales de refresh antiguas para licencias estándar, de
  cortesía y perpetuas cuando el token firmado continúa vinculado al mismo
  dispositivo, instalación y versión de licencia.
- Prioriza `https://clipxtudio.com` sobre el backend local y muestra progreso o
  un error claro mientras se solicita un caption.


## [0.5.48] - 2026-07-31

- Integra Plantilla encima de Capas visibles en una sola tarjeta compacta del
  lienzo vertical, con ayuda breve dentro del mismo bloque.
- Elimina la tarjeta informativa redundante y el botón de vista previa porque
  los cambios se reflejan inmediatamente en la composición vertical.
- Reutiliza la escena administrada `ClipXtudio Vertical` cuando ya existe y
  aplica de verdad la plantilla, visibilidad, orden y transformación de capas.


## [0.5.47] - 2026-07-31

- Corrige el flujo de captions para que una licencia Pro de cortesía sincronice
  primero con el backend y no reutilice un token vencido durante la gracia
  offline.
- Distingue una autorización pendiente de un fallo de infraestructura de firma
  y muestra mensajes claros en español e inglés en lugar de errores técnicos.
- Añade un preflight de despliegue que valida el par de claves de firma antes de
  habilitar activación, refresh y funciones de IA.


## [0.5.46] - 2026-07-31

- Unifica los botones de ayuda contextual de Ajustes con un icono `i` claro.
- Permite cerrar la ayuda al volver a pulsar el icono, al pulsar fuera o con
  Escape.
- Amplía el selector del encoder para mostrar los nombres completos sin
  truncarlos.


## [0.5.45] - 2026-07-31

- Corrige el fallback de captions de Ollama a OpenAI y presenta errores AI seguros.

## [0.5.44] - 2026-07-31

- Fixed caption generation when the dock restored a cached Pro entitlement
  without restoring the signed backend authorization credential.
- Signed backend credentials now take priority over the local QA/perpetual
  fallback, preserving AI authorization across plugin updates.
- Caption generation now refreshes an eligible license and retries once when
  authorization is missing or expired.
- Split missing authorization from backend transport errors and added
  actionable localized messages.
- Added regression coverage for signed credential restoration when a QA cache
  also exists.

## [0.5.43] - 2026-07-31

- Reworked the Vertical tab for narrow OBS docks so the live 9:16 preview is
  the first and dominant element instead of being pushed below full controls.
- Added compact icon-only actions for Replay Buffer, saving a clip and creating
  the vertical scene, with localized tooltips and accessible names.
- Added a preview-only mode that hides the global header, status cards, tabs,
  Vertical controls and layers while preserving the live preview and footer.
- Preserved the complete desktop layout and all existing callbacks, settings
  and Free/Pro gates at wider dock sizes.
- Added focused UI tests for responsive layout, preview-only mode and restoration.

## [0.5.42] - 2026-07-31

- Added a responsive footer presentation for narrow OBS docks.
- Compact mode now shows `Replay`, `Clip`, and an update icon without clipped
  labels, while wide mode preserves the complete localized action text.
- Added distinct replay and recording icons and hid developer credits before
  they can become truncated.
- Added UI regression coverage for compact and expanded footer layouts.

## [0.5.40] - 2026-07-31

- Added an explicit localized label to every clip score badge: `Puntuación: N`
  in Spanish and `Score: N` in English.
- Preserved the existing default order by most recent capture; users can still
  choose best score from the sort selector.
- Added UI regression coverage for the localized score badge text.

## [0.5.39] - 2026-07-31

- Compacted the update status dialog so its information icon and message remain
  grouped and left-aligned instead of reserving a wide empty icon column.
- Reduced the up-to-date dialog text width, button height and button width while
  preserving selectable version details and window-modal behavior.
- Applied the same left-aligned message treatment to available-update and
  update-error dialogs for a consistent update flow.
- Added UI regression coverage for the compact icon and text columns.

## [0.5.38] - 2026-07-31

- Reduced the Clips library global action buttons to a compact 32-pixel height.
- Added balanced top and bottom spacing to the action toolbar so Create copies,
  Cancel and Delete selected no longer touch or clip against its lower edge.
- Vertically centered the three actions without changing selection, export or
  deletion behavior.
- Added UI regression coverage for the shared height and visible toolbar margins.

## [0.5.37] - 2026-07-31

- Removed the non-functional cloud-audio consent control from Voice phrases.
- Limited voice processing to the working local OBS-audio pipeline so the UI
  no longer suggests that audio can be uploaded when no cloud transport exists.
- Added settings migration schema 11 to reset legacy cloud mode and consent
  values safely to local processing without deleting configured phrases.
- Added settings and UI regressions for the single local-processing option.

## [0.5.36] - 2026-07-31

- Moved the Voice phrases editor out of the compressed Triggers column and
  made it span the complete workspace width.
- Reorganized the phrase list/editor and recognition controls into balanced
  cards that stack automatically in narrow OBS docks.
- Standardized the phrase input, Add and Remove actions at 44 pixels and
  added consistent internal spacing to the local-listening status.
- Added UI regression coverage for full-width layout, responsive stacking,
  control alignment and phrase persistence.

## [0.5.35] - 2026-07-31

- Unified the Vertical header actions and active-state badge at a shared
  44-pixel height.
- Vertically centered all four controls on the same baseline in the wide
  Vertical workspace while preserving the responsive stacked layout.
- Added a UI regression that verifies the exact height and top coordinate of
  the Replay Buffer, Save clip, Create scene and Vertical active controls.

## [0.5.34] - 2026-07-31

- Replaced persistent instructional copy under controls with compact contextual
  information buttons in Capture, Triggers and Settings.
- Added a shared accessible help component that shows localized guidance on
  hover, keyboard focus and click without changing the underlying action.
- Kept dynamic status, error, privacy and runtime feedback visible so critical
  information is never hidden behind a tooltip.
- Added UI regression coverage for contextual help content and button wiring.

## [0.5.33] - 2026-07-31

- Moved Replay Buffer performance and encoder configuration out of the
  Vertical workspace into a dedicated Settings card.
- Preserved the active OBS profile integration, hardware encoder detection,
  Replay Buffer enablement, apply action, restart confirmation, persistence
  and status feedback.
- Kept Vertical focused exclusively on the 9:16 canvas, composition, layers,
  positioning and templates.
- Added English and Spanish help copy plus UI regression coverage for the new
  Settings location.

## [0.5.32] - 2026-07-31

- La rueda del mouse solo controla el zoom de la vista previa vertical después
  de activarla intencionalmente con un clic.
- Mientras la vista previa está inactiva, la rueda continúa desplazando la
  pantalla Vertical sin alterar el encuadre.
- La edición se desactiva al salir de la vista previa para evitar cambios
  accidentales al volver a recorrer la pantalla.
- Mantiene el arrastre para posición y el zoom intencional sin modificar la
  composición, el preview de OBS ni la persistencia existentes.
- Añade una prueba de regresión para el estado de activación de la interacción.

## [0.5.31] - 2026-07-31

- Evita que la rueda del mouse cambie accidentalmente sliders, campos numéricos
  o selecciones mientras el usuario desplaza una pantalla del dock.
- Los valores siguen siendo editables mediante clic, arrastre, flechas del
  control y teclado.
- Centraliza el comportamiento en controles Qt reutilizables y lo aplica a
  Vertical, Triggers, Clips y Ajustes.
- Añade una prueba UI que valida que la rueda no altera valores y permanece
  disponible para el contenedor desplazable.

## [0.5.30] - 2026-07-31

- Activa la generación de captions sugeridos desde cada clip de la biblioteca.
- Reutiliza la transcripción existente o transcribe localmente el audio con
  FFmpeg y Whisper antes de consultar el asistente AI configurado.
- Muestra el caption en un diálogo legible con una acción para copiarlo.
- Mantiene el audio y el video en el equipo: solo se envía el texto de la
  transcripción cuando AI y su consentimiento están habilitados.
- Marca Subtítulos como `Próximamente` y evita presentar una acción sin
  implementación.
- Añade cobertura UI para generación, diálogo copiable y estado de subtítulos.

## [0.5.29] - 2026-07-30

- Vertical ahora ocupa el primer lugar de la navegación y continúa siendo la
  pantalla inicial.
- Se eliminó la etiqueta redundante `Principal` del tab Vertical.
- Se actualizaron la navegación interna, las pruebas y las capturas
  automatizadas para el nuevo orden.

## [0.5.28] - 2026-07-30

- Añade `Mejorar plan` dentro de la tarjeta de Plan cuando la licencia está en
  Free/Gratis.
- El CTA abre la sección pública de precios Pro de ClipXtudio y desaparece
  automáticamente cuando la licencia activa Pro.
- Incorpora textos equivalentes en español e inglés y cobertura UI para los
  estados Free y Pro.

## [0.5.27] - 2026-07-30

- Aumenta la tipografía de títulos, subtítulos, labels y descripciones de
  Ajustes para mejorar la lectura dentro del dock.
- Reorganiza Almacenamiento y carpetas en dos niveles: nombre y explicación
  arriba; ruta, Cambiar y Abrir carpeta debajo.
- Evita que las descripciones de rutas se compriman o se corten en tarjetas
  estrechas sin modificar la persistencia ni las acciones existentes.
- Añade cobertura UI para verificar las tres rutas y su estructura vertical.

## [0.5.26] - 2026-07-30

- Corrige la activación de licencias cuando producción usa rutas de claves
  distintas a las del entorno de desarrollo.
- Resuelve y valida el par RSA de firma en tiempo de ejecución desde el
  almacenamiento privado estándar del backend.
- Añade el diagnóstico `licenses:signing-keys:check` para impedir despliegues
  con claves ausentes o incompatibles con el plugin.
- Evita consumir una licencia si el backend no puede firmar el token y muestra
  al cliente un error localizado y recuperable.

## [0.5.25] - 2026-07-30

- Añade padding horizontal consistente a las tarjetas de señales de Triggers.
- Evita que títulos, descripciones y toggles queden pegados a los bordes en docks estrechos.
- Agrega una prueba de regresión para conservar el margen interno de las tarjetas.

## [0.5.24] - 2026-07-30

- Reemplaza el mensaje recortado del header por alertas legibles de actualización.
- Muestra la versión instalada y la versión disponible antes de descargar.
- Permite confirmar “Descargar e instalar” o posponer una actualización.
- Mantiene el header en estado compacto mientras el botón muestra el progreso de la consulta.

## [0.5.23] - 2026-07-30

- Ajusta el encabezado de Vertical para replicar la referencia: identidad a la
  izquierda, acciones compactas a la derecha y estado verde sin expandir los
  botones.
- AÃ±ade la segunda lÃ­nea de estado con “Guardado automÃ¡tico” y “Ahora mismo”.
- Simplifica “Crear escena vertical de ClipXtudio” a “Crear escena vertical”
  para mantener la proporciÃ³n y jerarquÃ­a visual del diseÃ±o.

## [0.5.22] - 2026-07-30

- Recompone Vertical para seguir la referencia visual: encabezado abierto,
  acciones superiores, preview 9:16 con capas debajo y cuatro tarjetas de
  controles por propósito.
- Añade sliders sincronizados para zoom y posición sin reemplazar los bindings
  existentes con OBS ni la edición directa de la vista previa.
- Mantiene Vertical como pestaña inicial, conserva los gates Free/Pro y
  uniforma el estado activo, inputs y espaciado con el resto del plugin.

## [0.5.21] - 2026-07-30

- Unifica spacing, tarjetas, botones, controles y jerarquía visual en las cinco
  pantallas principales del plugin.
- Mejora la barra superior con tabs más altos, estado activo tipo tarjeta y
  badge Principal/Primary para Vertical.
- Mantiene Vertical como pestaña inicial y conserva bindings, eventos y gates
  Free/Pro existentes.

## [0.5.20] - 2026-07-30

- Rediseñó Capturar como dashboard de inicio con acciones rápidas, estado del
  buffer, duraciones, atajos, último clip y flujo recomendado.
- Añadió comportamiento responsive para docks estrechos sin ocultar acciones.
- Cambió la pantalla inicial del dock a Vertical conservando Capturar como tab.
- Conservó los servicios, eventos y feature gates existentes de captura.

## [0.5.19] - 2026-07-30

- Corrige la pérdida de IDs seleccionados antes de ejecutar el borrado
  asíncrono individual o múltiple.
- Permite completar el borrado aunque un thumbnail, transcript o subtítulo
  opcional ya no exista.
- Valida que la confirmación elimina el archivo local y la metadata de SQLite.

## [0.5.18] - 2026-07-30

- Conecta todos los banners **Mejorar a Pro** con la sección pública de planes.
- Permite editar seis atajos desde Ajustes y aplica las combinaciones al sistema
  de hotkeys nativo de OBS sin depender del foco del dock.
- Activa sonido, toast del sistema y niveles compacto/estándar/detallado al
  guardar clips.
- Añade explicaciones bilingües por sección y por campo en Ajustes.

## [0.5.17] - 2026-07-30

- Añade eliminación individual mediante una acción roja en cada clip.
- Añade selección múltiple y eliminación por lote desde la biblioteca.
- Solicita confirmación explícita antes de eliminar permanentemente el video y
  sus archivos asociados del equipo.
- Corrige la selección de clips, que antes se desmarcaba al actualizar las
  acciones disponibles.

## [0.5.16] - 2026-07-30

- La guía remota se muestra para usuarios Free y Pro mediante configuración
  pública limitada y con rate limit.
- El plan Free se presenta como `Gratis` cuando el plugin está en español.
- Las credenciales Pro firmadas sobreviven actualizaciones y reinicios mediante
  el almacén seguro del sistema.
- Si el access token venció mientras OBS estaba cerrado, el plugin conserva el
  refresh token, vuelve temporalmente a Free y renueva al iniciar sin pedir de
  nuevo la license key de un solo uso.

## [0.5.15] - 2026-07-30

- Simplifica el crédito del footer en español e inglés.

## [0.5.14] - 2026-07-30

- Añade video demo/guía administrable desde el backend y visible en el header del dock.
- Scene Trigger permite seleccionar múltiples escenas de OBS.
- El cliente de IA usa backend local de QA y cae de forma segura a `clipxtudio.com`.

## [0.5.13] - 2026-07-29

- Checks the local QA endpoint and the production release manifest
  automatically after the ClipXtudio dock starts.
- Shows a blinking red update indicator and turns the footer action into
  **Download update** when a newer release is available.
- Downloads the Windows installer asynchronously, validates its declared size
  and SHA-256 checksum, and writes it atomically.
- After explicit confirmation, safely closes OBS, runs the verified installer
  elevated and restarts OBS. Active streaming, recording or Replay Buffer
  blocks installation.

## [0.5.12] - 2026-07-29

- Added automatic license API selection between the local development backend
  at `http://127.0.0.1:8000` and production at `https://clipxtudio.com`.
- Fixed local license requests incorrectly initializing TLS for a plain HTTP
  loopback connection.
- Added safe endpoint fallback without permitting unencrypted remote hosts.

## [0.5.11] - 2026-07-29

- Migrates settings, the stable install ID and the SQLite clip library from the
  legacy `clipcoach-studio` module configuration directory to `clipxtudio`.
- Backs up any files created by 0.5.10 before replacing them with the legacy
  user data.
- Preserves the existing device-bound license identity after the module rename.

## [0.5.10] - 2026-07-29

- Renamed the native module to `clipxtudio.dll` so its filename matches the
  `clipxtudio` OBS package directory and OBS discovers it correctly.
- Updated the locale module identifier and Windows package validation.
- The installer removes the broken 0.5.9 `clipcoach-studio.dll` left inside the
  new package directory during upgrade.

## [0.5.9] - 2026-07-29

- Renamed the Windows installation and portable package root from
  `clipcoach-studio` to `clipxtudio`.
- Added an upgrade migration that removes the legacy packaged plugin directory
  without touching user clips, exports, settings or the local library.
- Kept `clipcoach-studio.dll` as the stable OBS module filename for binary and
  data compatibility.

## [0.5.8] - 2026-07-29

- Fixed Restart OBS on Windows when Qt reports `obs64.exe` with the extended
  `\\?\` path prefix.
- Replaced the fragile `cmd.exe start` relaunch command with a hidden detached
  helper that passes the validated executable path without shell quoting.
- Refuses to close OBS when the executable or restart helper cannot be
  validated and asks the user to restart manually instead.

## [0.5.7] - 2026-07-29

- Added a native Restart OBS confirmation after changing the Replay Buffer
  encoder, with safe output-active checks and automatic relaunch.
- Kept the current OBS process in control of shutdown so canceled close prompts
  never launch a duplicate OBS instance.
- Synchronized the header's Vertical Canvas resolution immediately with
  changes made in the Vertical tab.
- Added a versioned Windows release build command that increments the semantic
  patch version before each new release build and rolls metadata back if the
  build fails.

## [0.5.6] - 2026-07-29

- Grouped ClipCard selection, 16:9 thumbnail and metadata on the left.
- Added duration overlay to clip thumbnails.
- Generates a real thumbnail asynchronously from each saved clip using the
  bundled FFmpeg executable.
- Persists generated thumbnail paths in SQLite and refreshes the library when
  generation completes.
- Backfills up to 100 recent historical clips whose original video still
  exists, staggered to avoid a CPU spike.
- Preserves the actual trigger phrase/source label in each ClipCard.

## [0.5.5] - 2026-07-29

- Added native OBS Replay Buffer profile controls to the Vertical tab.
- Lists only H.264 encoders actually registered by the running OBS instance.
- Identifies CPU and hardware encoders and prioritizes GPU options.
- Saves encoder and Replay Buffer enablement directly to the active OBS
  profile, with output-active safety checks and safe backup writes.
- Keeps one native OBS Replay Buffer for horizontal and vertical workflows;
  no second vertical encoder is started.

## [0.5.4] - 2026-07-29

- Enlarged the native 9:16 OBS preview to use the available designer space.
- Moved Visible Layers into a dedicated section directly below the preview.
- Preserved the explanatory copy and scene/framing controls in the right-hand
  column, with preview, layers and controls stacking cleanly in narrow docks.

## [0.5.3] - 2026-07-29

- Rebuilt Clip Library cards so duration, title, date and trigger remain
  grouped on the left.
- Replaced ambiguous text glyphs with five purpose-built action icons and a
  native vector favorite icon.
- Made the current OBS session the default library scope and exposed prior
  persisted clips through an explicit Pro history selector.
- Applied English/Spanish interface changes immediately by safely rebuilding
  the native dock while preserving the selected tab.

## [0.5.2] - 2026-07-29

- Kept the three Clip Library indicators visible in one horizontal row and
  prevented cards from collapsing into thin lines.
- Added responsive filter wrapping for narrow OBS docks.
- Added a persistent plugin-language selector for OBS language, English and
  Spanish; explicit language overrides apply at the next OBS start.
- Reduced all fixed-footer actions to the same 32 px height and centered the
  command row vertically.

## [0.5.1] - 2026-07-29

- Unified the header, account panel and core Feature Gates around the same
  `LicenseManager` snapshot.
- Hid the Pro badge and upgrade messaging when the effective plan does not
  match; QA-device activation now survives reinstall/restart in secure storage.
- Made all fixed-footer actions the same height.
- Added direct drag positioning and mouse-wheel zoom to the native 9:16 preview.
- Reorganized Clip Library metrics, filters, sorting and compact card actions.

## [0.5.0] - 2026-07-29

- Replaced the placeholder vertical diagram with a live libobs-rendered preview.
- Added selectors for real OBS scenes and their direct sources.
- Added persistent 9:16 zoom and horizontal/vertical framing controls.
- Added creation of a `ClipXtudio Vertical` OBS scene from the selected source.
- Migrated local settings to schema 10 for the vertical OBS composition.

## [0.4.5] - 2026-07-29

- Trigger captures preserve their real type, matched phrase/scene and score
  through ClipManager and SQLite schema v5.
- Clip cards place all five actions in one compact row beside clip metadata.
- Voice command segmentation closes after 300 ms of silence.
- Trigger timing, sensitivity, cooldown and actions now include bilingual
  explanations and a live duration summary.
- Scene Trigger uses a selector populated from the current OBS scene
  collection.

## [0.4.4] - 2026-07-29

- Active Pro licenses now unlock the Capture tab's vertical clip action.
- Saving a vertical clip captures the OBS replay and queues its 9:16 copy.
- Voice activity detection now uses block RMS, tolerates isolated noise spikes
  and accepts exact bilingual commands from the bundled quantized model.
- Clips removes title search, places summary cards beside compact 2x2 filters,
  and keeps date/score sorting immediately above the list.

## [0.4.3] - 2026-07-29

- Saved-clip confirmation is now a compact right-aligned toast with a trailing
  check mark.
- The toast auto-dismisses after 3.5 seconds.
- Header save confirmation also shows a check and automatically returns to
  Ready, preventing stale saving state.
- Fixed an OBS shutdown crash by detaching Voice Trigger from its libobs audio
  source during the frontend exit event.

## [0.4.2] - 2026-07-29

- Moved the installed version from the footer to
  `ClipXtudio - v<version>` in the fixed header.
- Moved runtime status into a colored badge beside PRO.
- Removed duplicate product/version metadata from the footer.
- Normalized Replay Buffer, Save Clip and Check for Updates to the same 36px
  control height.

## [0.4.1] - 2026-07-29

- Bundled FFmpeg 8.1.2 x64 so users no longer install or configure it.
- The export backend now resolves only the media engine shipped with the OBS
  plugin and reports an actionable reinstall error if files are missing.
- Renamed `Export 9:16` to `Create 9:16 copy`; the UI now explains that the
  original replay clip is already saved.
- Clips already stored as vertical disable redundant 9:16 conversion.
- Completed copies display their destination path.

## [0.4.0] - 2026-07-29

- Voice Trigger now captures the selected OBS audio source through `libobs`.
- Added bundled local multilingual Spanish/English recognition with
  `whisper.cpp`; Windows Speech language packs are no longer required.
- Added OBS source selection, post-load reconnection, voice activity
  segmentation and truthful model/audio runtime states.
- Migrated legacy speech-language settings to bilingual automatic detection.
- Calibrated quantized-model confidence for exact configured commands.

All notable changes to ClipXtudio are documented here. The project uses
[Semantic Versioning](https://semver.org/).

## [0.3.0] - 2026-07-29

### Changed

- Product brand standardized as `ClipXtudio` across the dock, hotkeys, logs,
  installer and documentation.
- Fixed global footer is now a single horizontal command bar: Replay Buffer,
  Save Clip and Check for Updates stay on the left; status, version and
  developer credit stay on the right.
- Header now renders the packaged ClipXtudio icon instead of a styled purple
  placeholder.
- Developer credit now links to `QuatroBytes.com`.
- Licensing, plan management and update configuration now target
  `clipxtudio.com` and `api.clipxtudio.com`.

### Compatibility

- OBS module filenames, data directories, configuration keys and secure-storage
  namespace remain unchanged so upgrades preserve user data and activations.

### Known limitations

- `clipxtudio.com` and `api.clipxtudio.com` must publish DNS, TLS, API and
  update-manifest infrastructure before online services can respond.

## [0.2.0] - 2026-07-29

### Added

- Fixed global footer with version, update check, developer credit, Replay
  Buffer toggle and Save Clip action on every tab.
- Native Windows Speech/SAPI microphone runtime for Voice Trigger.
- Visible Voice runtime status, installed-language detection and bilingual
  command defaults including `saca clip`, `save clip` and `save that`.

### Changed

- Trigger explanations now describe inputs, behavior, dependencies and whether
  each signal is operational or reserved.
- Product, licensing and update endpoints now target `clipstudio.com`.
- Developer credit links to `4bytes.com`.
- Settings schema upgraded to v7 with automatic phrase migration.

### Known limitations

- `clipstudio.com` must publish DNS, TLS, API and update manifest before online
  services can respond.
- Spanish recognition requires the Windows Spanish Speech component.
- Cloud speech remains disabled until the private ClipX backend transport is
  deployed.

## [0.1.0] - 2026-07-29

### Added

- Native OBS Studio plugin and movable Qt dock.
- Manual Replay Buffer capture, hotkeys and session clip library.
- SQLite metadata, migrations and non-destructive corruption fallback.
- Vertical canvas configuration and asynchronous MP4 export.
- Smart trigger, scoring, Voice Trigger and Chat Pulse foundations.
- Pro licensing, feature gates and Laravel/Stripe backend.
- AI Assistant backend contract and local result persistence.
- Windows installer, uninstaller and portable ZIP packaging.
- Automated native, UI, backend, security, contract, performance and packaging tests.

### Security

- Provider secrets remain backend-only.
- License tokens are signed and device-bound.
- OAuth and license caches use platform secure storage.
- Uninstall preserves user clips, exports, settings and library data.

### Known beta limitations

- Real vertical output and integrations depend on the configured OBS/profile environment.
- Production Pro activation requires the release public key to match the backend signing key.
- Production AI requires a backend `AiProvider` implementation.
