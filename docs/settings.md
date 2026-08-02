# Ajustes de ClipXtudio

## Cambios funcionales 0.5.18

- Cada sección y cada campo muestra ayuda bilingüe contextual.
- Sonido al guardar ejecuta la señal de notificación de Qt.
- Toast del sistema usa la bandeja de Windows cuando está disponible.
- El nivel de detalle cambia entre confirmación compacta, nombre del clip y
  detalle con duración/carpeta.
- Los seis editores de hotkeys aplican combinaciones nativas de OBS al instante.

**Estado:** esquema local v3 implementado  
**Persistencia:** JSON atómico en la carpeta de configuración del módulo OBS

## Arquitectura

```text
SettingsTab (Qt Widgets)
  |
  v
SettingsController
  |
  v
SettingsManager
  |-- validación central
  |-- escritura settings.json.tmp
  |-- publicación atómica + rollback
  `-- migración schema v1/v2/v3/v4 -> v5
```

`SettingsTab` no serializa JSON. Cada control entrega una intención al
controlador; el controlador crea una copia, valida y guarda. Solo después de una
escritura correcta se publica el nuevo estado. Si falla, el control recupera el
último valor válido y aparece una notificación dentro del dock.

## Secciones

### General

- Iniciar con OBS Studio.
- Abrir dock al iniciar.
- Iniciar automáticamente Replay Buffer.

El plugin nativo se carga con OBS por diseño. `start_with_obs` queda persistido
para que el instalador/gestor de módulos lo use cuando exista ese flujo.
`open_dock_at_startup` queda persistido; la visibilidad final también depende del
estado de docks que OBS conserva.

### Captura

- Duración predeterminada: 5–300 segundos.
- Pre-roll y post-roll: 0–120 segundos.
- Salida: horizontal, vertical o ambos.
- Límite Free: 20 clips por sesión, visible y no editable.
- Confirmación antes de borrar.

La duración predeterminada se aplica inmediatamente a la siguiente captura.
Pre/post-roll y modo de salida quedan listos para el pipeline inteligente y
vertical; no alteran de forma ficticia el Replay Buffer actual.

### Rutas

Clips, exports y thumbnails aceptan:

- vacío para usar la ubicación automática;
- ruta absoluta a un directorio existente;
- ruta absoluta todavía no creada.

Se rechazan rutas relativas y rutas que apuntan a archivos. “Cambiar” usa
`QFileDialog`; “Abrir carpeta” usa la integración de escritorio de Qt.

### Nombres de archivo

Flags persistentes: fecha, score, trigger y orientación.

Tokens permitidos:

```text
{date} {time} {title} {score} {trigger} {orientation} {session}
```

La plantilla admite hasta 160 caracteres. Se rechazan tokens desconocidos,
llaves desbalanceadas, separadores de ruta y caracteres inválidos de archivo.

### Notificaciones

- Notificación dentro de OBS.
- Sonido al guardar.
- Toast del sistema.
- Detalle compacto, estándar o detallado.

La notificación interna se aplica en caliente. Sonido y toast se persisten para
sus adaptadores de plataforma; no se simulan mientras esos servicios no existan.

### Hotkeys

Las acciones operativas se registran con la API nativa de OBS y se configuran en
**OBS Studio → Ajustes → Atajos**. OBS persiste las combinaciones mediante el
callback de guardado del Frontend; los antiguos campos `QKeySequence` quedan
desactivados y se conservan únicamente para compatibilidad de schema. Véase
`docs/hotkeys.md`.

### Exportación

- Contenedor: MP4 o MOV.
- Codec: H.264, HEVC o AV1.
- FPS: 30 o 60.
- Calidad: baja, media, alta o máxima.
- Vertical: 720×1280, 1080×1920, 2K 1440×2560, 4K 2160×3840,
  8K 4320×7680 o Custom 9:16.

Preset y dimensiones se validan como una unidad. La resolución del encabezado
del dock cambia inmediatamente; el pipeline ExportManager consume la resolución
persistida. 4K y 8K requieren hardware y encoder capaces de sostener esa carga.

### Integraciones

Twitch y YouTube muestran estado tipado y acciones Conectar/Desconectar cuando
se inyecta un transporte Pro. Kick muestra `PRÓXIMAMENTE`. Los tokens se
guardan únicamente en `SecureStorage`, nunca en `settings.json`.

### Cuenta Pro

Muestra plan Free, activación, membresía, uso mensual y reset de dispositivo.
Las acciones están visibles pero deshabilitadas con explicación hasta conectar
`LicenseManager`, `SecureStorage` y la API Laravel. No se simulan activaciones,
uso ni solicitudes.

## Campos persistidos

El esquema v5 persiste todos los valores de General, Captura, Rutas, Nombres,
Notificaciones, Hotkeys y Exportación. Estados de integraciones, licencia y uso
son datos externos y deliberadamente no se almacenan como preferencias.

La primera lectura de un archivo v1 conserva idioma, notificaciones, resolución
y duraciones rápidas; completa los campos nuevos con defaults y publica v5.

## Aplicación sin reinicio

| Ajuste | Aplicación |
|---|---|
| Duración predeterminada | inmediata |
| Notificación dentro de OBS | inmediata |
| Auto-start Replay Buffer | inmediata al activarlo y durante el próximo arranque |
| Resolución vertical mostrada | inmediata |
| Rutas, plantilla, exportación | persistidas para el servicio consumidor |
| Abrir dock / iniciar con OBS | siguiente ciclo y adaptador de lifecycle |
| Hotkeys | inmediata mediante el sistema nativo de OBS |
| Sonido/toast | requiere adaptador de plataforma |
| Integraciones | inmediata al cambiar conexión; polling en worker |
| Cuenta Pro | requiere backend de licencia |

## Criterios de aceptación

1. Las nueve secciones existen y usan Qt Widgets nativo.
2. Todos los campos locales sobreviven a una nueva instancia de
   `SettingsManager`.
3. Un archivo v1, v2, v3 o v4 migra automáticamente a v5.
4. Rutas relativas, plantillas desconocidas, timings fuera de rango, FPS no
   soportados y presets incoherentes se rechazan.
5. Un guardado fallido no modifica el estado activo y revierte el control.
6. Duración y notificación interna afectan la siguiente captura sin reinicio.
7. Los textos visibles existen en español e inglés.
8. No se persisten tokens sociales, licencias ni fingerprints.
## Plugin language

General includes `Plugin language` with three persisted values:

- `system`: follow the language selected by OBS.
- `en-US`: force English.
- `es-ES`: force Spanish.

The setting is saved immediately. Because the native dock and every tab are
constructed with localized text, the explicit override is applied the next
time OBS starts.
