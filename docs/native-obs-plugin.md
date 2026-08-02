# ClipXtudio Studio — Diseño del plugin nativo de OBS

**Estado:** Guía de integración, previa a implementación  
**Relacionado:** [architecture.md](architecture.md)

---

## 1. Propósito

Este documento define cómo ClipXtudio Studio se integra de forma nativa con OBS Studio: exports del módulo, lifecycle, Frontend API, Replay Buffer, dock Qt, hotkeys, ownership, threading y compatibilidad.

No define la lógica final de captura/render. Los nombres y snippets son contratos propuestos que se implementarán después de aprobar arquitectura y ADRs.

---

## 2. Naturaleza del artefacto

ClipXtudio Studio es:

- Un módulo nativo compilado y cargado por OBS.
- Basado en `obsproject/obs-plugintemplate`.
- Compilado mediante CMake.
- Enlazado contra `libobs` y OBS Frontend API.
- Integrado con la instancia Qt que ya ejecuta OBS.
- Instalado con binario, recursos, traducciones y dependencias permitidas.

ClipXtudio Studio no es:

- Un script Python/Lua.
- Una app externa que controla OBS.
- Un cliente WebSocket como arquitectura principal.
- Un proceso que duplica el stream completo fuera de OBS.

---

## 3. Exports y lifecycle del módulo

### 3.1 Exports previstos

```cpp
#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("clipcoach-studio", "en-US")

extern "C" bool obs_module_load(void);
extern "C" void obs_module_post_load(void);
extern "C" void obs_module_unload(void);
extern "C" const char* obs_module_name(void);
extern "C" const char* obs_module_description(void);
```

Reglas:

- Los exports C son `noexcept` de hecho: capturan toda excepción.
- `obs_module_load()` realiza trabajo mínimo, construye bootstrap y registra el listener de frontend.
- `obs_module_post_load()` notifica que los módulos terminaron de cargar; no asume que toda UI está lista sin verificar.
- La inicialización de UI es idempotente y se completa al recibir `OBS_FRONTEND_EVENT_FINISHED_LOADING`.
- `obs_module_unload()` ejecuta cleanup defensivo, pero no llama Frontend API si ya ocurrió `OBS_FRONTEND_EVENT_EXIT`.
- El locale se obtiene mediante el mecanismo de módulo; no se hardcodean strings visibles.

### 3.2 Máquina de estados del bootstrap

```text
Constructed
  -> ModuleLoaded
  -> WaitingForFrontend
  -> Running
  -> Stopping
  -> Stopped

Any pre-Running failure
  -> DegradedRunning, si captura/biblioteca segura siguen disponibles
  -> Failed, si no existe modo seguro
```

`start()`, `startUi()` y `shutdown()` deben ser idempotentes.

### 3.3 Orden de inicio

1. Crear logger y crash boundary del plugin.
2. Detectar versión/capacidades OBS.
3. Resolver raíz de configuración del módulo.
4. Cargar settings globales.
5. Abrir SQLite y ejecutar migraciones.
6. Crear secure storage, API/licencia y managers.
7. Registrar frontend event callback.
8. Al finalizar carga del frontend:
   - registrar hotkeys;
   - crear `MainDockUI`;
   - registrar dock;
   - resolver perfil/colección;
   - reconciliar trabajos;
   - publicar estado inicial.
9. Iniciar refresh de licencia en background.

El paso 9 no bloquea pasos de captura local.

### 3.4 Orden de salida

El evento `OBS_FRONTEND_EVENT_EXIT` es la última oportunidad para usar Frontend API. Por tanto:

1. Marcar runtime como stopping.
2. Rechazar nuevas capturas/triggers/exports.
3. Detener preview.
4. Desregistrar hotkeys/dock/callbacks mediante mecanismo validado para la versión objetivo.
5. Cancelar tareas de red.
6. Llevar jobs activos a un estado recuperable.
7. Vaciar escrituras acotadas y cerrar DB.
8. Destruir UI en hilo Qt.
9. Invalidar el adaptador OBS.

Después de retornar del evento de salida, cualquier intento de Frontend API debe fallar localmente con `Unavailable` sin llamar a OBS.

---

## 4. ObsFrontendAdapter

### 4.1 Objetivo

Evitar que `libobs` se propague por el proyecto. Solo estos archivos pueden incluir `obs.h`, `obs-module.h` u `obs-frontend-api.h`:

```text
src/plugin/**
src/adapters/obs/**
include/clipcoach/adapters/obs/**   # si fuera imprescindible
```

Se añadirá una regla CI que falle si esos includes aparecen en `src/core`, `include/clipcoach/core`, `src/ui`, `storage`, `network` o `security`.

### 4.2 Traducción de eventos

Callback C:

```text
OBS enum + raw handles
  -> validate bootstrap alive
  -> capture bounded snapshot
  -> copy strings/path
  -> release OBS-owned allocation/ref
  -> enqueue FrontendEvent value
  -> return immediately
```

El core nunca recibe:

- `obs_source_t*`
- `obs_output_t*`
- `obs_data_t*`
- `config_t*`
- enums de OBS
- `calldata_t`

### 4.3 Ownership de handles

Cada tipo refcounted que deba sobrevivir una llamada se envuelve en RAII:

```cpp
template <typename T, void (*Release)(T*)>
class ObsRef final {
public:
  explicit ObsRef(T* value = nullptr) noexcept;
  ~ObsRef();
  ObsRef(ObsRef&&) noexcept;
  ObsRef& operator=(ObsRef&&) noexcept;
  ObsRef(const ObsRef&) = delete;
  ObsRef& operator=(const ObsRef&) = delete;
  T* get() const noexcept;
private:
  T* value_;
};
```

No se asume que todas las listas retornadas usan la misma regla de release. Cada llamada OBS se documentará según su contrato; por ejemplo, una source list de frontend se libera con su función específica, mientras rutas retornadas por determinadas APIs pueden requerir `bfree`.

### 4.4 Capability map

El adaptador calcula capacidades una vez y al cambiar contexto:

```cpp
struct ObsCapabilities {
  SemanticVersion runtimeVersion;
  bool certified;
  bool supportsDockById;
  bool supportsLastReplayPath;
  bool supportsReplayFrontendEvents;
  bool supportsRequiredRenderPath;
};
```

No se dispersan `#if OBS_VERSION...` en managers. Las divergencias se resuelven en el adaptador o en implementaciones por versión.

---

## 5. Integración de Replay Buffer

### 5.1 APIs y eventos

El adaptador utilizará únicamente APIs frontend soportadas por la matriz:

- Consultar si Replay Buffer está activo.
- Solicitar guardar replay.
- Escuchar eventos de starting/started/stopping/stopped/saved.
- Obtener la ruta del último replay guardado cuando la versión certificada lo soporte.

La llamada de “save” solo solicita trabajo; no equivale a éxito.

### 5.2 Correlación

OBS notifica un replay guardado, pero ClipXtudio debe correlacionar la notificación con una solicitud lógica:

```text
CaptureRequest:
  request_id
  requested_at_monotonic
  requested_at_utc
  origin
  trigger_ids
  profile/collection/scene snapshot
  state
```

Política v1:

- Mantener una cola ordenada de solicitudes aceptadas.
- Serializar guardados si la versión/behavior de OBS no permite correlación inequívoca.
- Al evento saved, obtener/copiar la ruta final y completar la solicitud más antigua compatible.
- Verificar que la ruta no fue indexada.
- Si no hay solicitud pendiente, tratarlo como replay externo: ignorar o indexar según setting explícito, nunca atribuirlo falsamente.
- Timeout produce `Failed/Timeout`, pero una notificación tardía se reconcilia por ruta y timestamp.

### 5.3 Estado

```text
Unavailable  version/capability absent
Stopped      configured but not active
Starting     frontend event
Active       ready to accept request
SaveRequested logical request accepted
Finalizing   saved event/path stabilization
Error        recoverable reason
```

`ReplayManager` contiene esta máquina. `ObsFrontendAdapter` solo reporta hechos.

### 5.4 Política de control

- MVP no inicia/detiene Replay Buffer silenciosamente.
- Si está detenido, UI muestra la acción requerida o una acción explícita aprobada por producto.
- Nunca se inicia/detiene streaming o recording como efecto lateral.
- Los cambios de perfil/colección invalidan snapshots y fuerzan reevaluación.

---

## 6. Dock Qt nativo

### 6.1 Registro

Para versiones que lo soporten, usar un ID estable:

```text
Dock ID: com.clipcoach.studio.main
Title key: Dock.Title
Widget: MainDockUI (QWidget)
```

La API oficial permite registrar un widget como dock y exponerlo en el menú de docks. La matriz mínima debe incluir la versión en que el método elegido existe.

### 6.2 Ownership

- `PluginBootstrap` posee el controller/runtime.
- Qt/OBS poseen la integración visual según el contrato del método de registro.
- El ownership real del `QWidget` se prueba y documenta con la versión fijada.
- Se usa `QPointer<MainDockUI>` para observar destrucción sin mantener un raw pointer colgante.
- Se elimina el dock antes de destruir dependencias que sus slots puedan usar.
- Ningún singleton Qt contiene managers.

### 6.3 MainDockUI

```text
MainDockUI
├─ CaptureTab + CaptureViewModel
├─ VerticalTab + VerticalViewModel
├─ TriggersTab + TriggersViewModel
├─ ClipsTab + ClipsViewModel
├─ SettingsTab + SettingsViewModel
└─ ProAccountTab + AccountViewModel
```

Los view models:

- Reciben interfaces de casos de uso.
- Emiten modelos inmutables de pantalla.
- Transforman errores a claves de mensaje.
- Se prueban con fakes.
- No ejecutan SQL, HTTP ni Frontend API.

### 6.4 Eventos Qt

- `showEvent`: reanudar refresh/preview si está habilitado.
- Evento de cierre específico del dock: pausar preview y tareas visuales.
- `changeEvent`/evento de theme: recargar tokens visuales.
- Destrucción: cancelar suscripciones RAII.

Una pestaña oculta no mantiene preview a FPS completo.

### 6.5 Comunicación con workers

```text
Qt slot -> command DTO -> application executor
application event -> queued Qt invocation -> view model -> widget
```

No se captura un `QWidget*` dentro de una tarea larga. Se captura un weak subscription token o se publica por event bus y se verifica vida.

---

## 7. Hotkeys

- Registrar una hotkey frontend estable: `clipcoach.save_clip`.
- Persistencia mediante mecanismo de OBS cuando corresponda; no duplicar binding en settings propios.
- Callback de hotkey solo encola `CaptureCommand`.
- Ignorar el evento release si el contrato entrega press/release y producto solo actúa en press.
- Desregistrar durante shutdown.
- El comando manual no se bloquea por cuota de automatización.

Hotkeys futuras:

- `clipcoach.manual_marker`
- `clipcoach.toggle_vertical_preview`
- `clipcoach.favorite_last_clip`

No se registran hasta estar en alcance y tener conflicto/UX definido.

---

## 8. Escenas, fuentes y canvas vertical

### 8.1 Lectura sin mutación

`ObsSceneCatalog` crea descriptors:

```cpp
struct SourceDescriptor {
  std::string stableId;
  std::string displayName;
  SourceKind kind;
  Size nativeSize;
  bool available;
};
```

El core guarda `stableId` y metadatos de reconciliación, nunca raw pointers.

### 8.2 Regla de aislamiento

El canvas vertical no modifica:

- resolución base/output de OBS;
- transform de scene items existentes;
- visibilidad de fuentes del programa;
- escena current/program;
- configuración de streaming/recording.

El adaptador crea recursos privados/off-screen o usa postproceso, según ADR-002.

### 8.3 Spike obligatorio

Antes de elegir backend:

1. Prototipo A: composición/render off-screen con recursos OBS.
2. Prototipo B: postproceso desde replay guardado.
3. Opcional C: híbrido para preview nativo + export posterior.
4. Medir CPU/GPU, rendering lag, audio sync, encoders ocupados y shutdown.
5. Seleccionar por gates del PRD, no por facilidad inicial.

La arquitectura ya permite sustituir el backend mediante `IVerticalRenderBackend`.

### 8.4 Recursos gráficos

- Crear/destruir recursos en el contexto/hilo requerido por libobs.
- Nunca bloquear render con disco/red.
- Callbacks de draw no acceden SQLite ni locks de aplicación.
- Estado de render consumido como snapshot inmutable.
- Preview con FPS acotado y desactivación al ocultar dock.

---

## 9. Eventos OBS y mapeo

| Evento OBS conceptual | Evento interno | Consumidor |
|---|---|---|
| Finished loading | `FrontendReady` | Bootstrap |
| Exit | `FrontendExiting` | Bootstrap |
| Replay starting/started/stopped | `ReplayStateChanged` | ReplayManager |
| Replay saved | `ReplayFileSaved` | ReplayManager/ClipManager |
| Scene changed | `ProgramSceneChanged` | TriggerEngine/Vertical |
| Scene collection cleanup/change | `SceneCollectionChanging/Changed` | Settings/Vertical |
| Profile changed | `ProfileChanged` | Settings/Replay |
| Recording/streaming state | `ProductionStateChanged` | Sessions/Triggers/UI |
| Theme changed | `ThemeChanged` | UI |

El mapeo exacto se cubre con tests del adaptador contra la versión OBS fijada.

---

## 10. Integración de configuración OBS

### 10.1 Config propia

La ruta propia se resuelve mediante `obs_module_config_path`. El buffer retornado por OBS se copia a `std::filesystem::path` y se libera según la API.

Se guardan fuera de la config de OBS:

- settings versionados;
- DB;
- cache;
- recibo firmado;
- temporales propios.

### 10.2 Config de perfil

El plugin puede leer valores necesarios mediante APIs soportadas, pero:

- No retiene `config_t*` fuera de la operación acotada.
- No escribe settings centrales de OBS sin acción explícita.
- No asume claves internas no documentadas como contrato estable.
- El onboarding dirige al usuario a configuración cuando no exista API segura.

### 10.3 Save callbacks

Si se usa un save callback de frontend, solo guarda referencias pequeñas de integración con la colección. El catálogo y secretos no se serializan dentro de scene collection.

---

## 11. Logging en OBS

Se crea `ObsLogSink` sobre el logger propio:

```text
Debug    -> LOG_DEBUG (solo builds/categorías habilitadas)
Info     -> LOG_INFO
Warning  -> LOG_WARNING
Error    -> LOG_ERROR
Critical -> LOG_ERROR + health state
```

Prefijo:

```text
[ClipXtudio Studio] [category] [event] [correlation_id]
```

Nunca se loguean:

- tokens o headers Authorization;
- recibos firmados completos;
- machine fingerprint crudo;
- nombres/notas de clips;
- rutas completas por defecto;
- payloads Laravel sin sanitizar.

---

## 12. Compatibilidad y versionado

### 12.1 Política

- Fijar tag/commit de `obs-plugintemplate`.
- Fijar versión mínima/máxima certificada de OBS.
- Fijar Qt y toolchain compatibles con ese OBS.
- Detectar versión runtime y mostrar “No certificada” fuera de matriz.
- No asumir compatibilidad ABI en una versión mayor.

### 12.2 Feature gates

```cpp
if (!capabilities.supportsLastReplayPath) {
  disable(Capability::ReliableReplayCorrelation,
          ErrorCode::IncompatibleVersion);
}
```

Los feature gates se resuelven al iniciar y se exponen como health report. No se prueban llamadas a ciegas.

### 12.3 Safe mode

La compatibilidad con el safe mode de OBS se decidirá explícitamente:

- Solo declararse safe si el plugin soporta el contrato y pasa pruebas.
- Nunca usar esa marca para omitir validación.
- Si OBS bloquea el módulo, el instalador/documentación debe explicarlo sin intentar evasión.

---

## 13. Packaging nativo

Contenido lógico:

```text
obs-plugins/<arch>/clipcoach-studio.<dll|dylib|so>
data/obs-plugins/clipcoach-studio/
  locale/
  icons/
  migrations/
  metadata/
```

Reglas:

- Dependencias runtime incluidas solo si licencia y packaging lo permiten.
- No sobrescribir DLLs/frameworks de OBS.
- Windows: binario e instalador firmados.
- macOS: firma, hardened runtime y notarización.
- Linux: paquetes por canal soportado; Flatpak separado.
- Desinstalar conserva datos/medios salvo selección explícita.
- Upgrade soporta migraciones hacia adelante y rollback del instalador; no downgrade destructivo de DB.

---

## 14. Pruebas específicas de integración OBS

### 14.1 Smoke

- Módulo carga y descarga.
- Dock aparece y persiste.
- Hotkey registra y ejecuta una sola solicitud.
- Replay inactive/active/saved se refleja correctamente.
- Cambio de perfil/colección no deja handles colgantes.
- Exit no produce llamada tardía a Frontend API.

### 14.2 Stress

- 100 ciclos start/exit.
- 1,000 abrir/cerrar dock.
- 500 guardados.
- Ráfagas de hotkey/trigger.
- Cambios de escena durante preview.
- Cambio de colección con fuentes verticales faltantes.
- Cierre forzado durante index/export.

### 14.3 Instrumentación

- AddressSanitizer/UndefinedBehaviorSanitizer donde toolchain/OBS lo permita.
- Windows Application Verifier o herramientas equivalentes en builds diagnósticos.
- Leak checks propios y observación de referencias OBS.
- Thread assertions en adaptadores y widgets.

---

## 15. Checklist antes de implementar

- [ ] ADR-001 fija OBS, Qt, compiler y template.
- [ ] Contrato de ownership del método de dock verificado.
- [ ] Eventos y ruta de replay verificados en la versión objetivo.
- [ ] Política de solicitudes simultáneas probada.
- [ ] Spike vertical comparado con métricas.
- [ ] Shutdown probado sin Frontend API tardía.
- [ ] Lista exacta de APIs OBS confinada al adapter.
- [ ] CMake targets impiden dependencia core → OBS/Qt.
- [ ] Installer layout validado en instalación limpia.

---

## 16. Referencias oficiales

- [OBS Module API](https://docs.obsproject.com/reference-modules)
- [OBS Studio Frontend API](https://docs.obsproject.com/reference-frontend-api)
- [OBS core/frontend lifecycle](https://docs.obsproject.com/frontends)
- [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate)

La Frontend API oficial documenta el registro/remoción de docks, callbacks y funciones de Replay Buffer. La Module API define `obs_module_load`, `obs_module_post_load`, `obs_module_unload` y las macros del módulo.
