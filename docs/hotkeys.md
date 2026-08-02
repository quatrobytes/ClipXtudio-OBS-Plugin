# Hotkeys nativas

## Objetivo

ClipXtudio Studio registra nueve hotkeys globales mediante la API nativa de OBS.
No dependen del foco del dock, de `QShortcut`, de WebSocket ni de una aplicación
externa. OBS es la fuente de verdad para asignaciones, conflictos y persistencia.

## Acciones registradas

| Nombre interno estable | Acción core |
|---|---|
| `clipcoach.mark_moment` | Capturar la duración predeterminada |
| `clipcoach.save_15_seconds` | Capturar 15 segundos |
| `clipcoach.save_30_seconds` | Capturar 30 segundos |
| `clipcoach.save_60_seconds` | Capturar 60 segundos |
| `clipcoach.save_2_minutes` | Capturar 120 segundos |
| `clipcoach.save_5_minutes` | Capturar 300 segundos |
| `clipcoach.save_vertical` | Solicitar captura vertical |
| `clipcoach.cycle_output_mode` | Horizontal → Vertical → Ambos → Horizontal |
| `clipcoach.toggle_dock` | Abrir o cerrar el dock |

Los nombres internos no deben cambiar: OBS los usa para relacionar cada binding
persistido con su acción. Las descripciones visibles sí se localizan mediante los
archivos `data/locale`.

## Arquitectura

```text
OBS Hotkey System
  └─ ObsHotkeyAdapter (API OBS + persistencia)
       └─ HotkeyManager (registro, dispatch y errores)
            ├─ ClipManager / ReplayManager
            ├─ SettingsManager
            └─ servicio de visibilidad del dock
```

- `HotkeyManager` pertenece a core y no incluye headers de OBS ni Qt.
- `HotkeyRegistrar` es el puerto testeable de registro/desregistro.
- `ObsHotkeyAdapter` implementa ese puerto con
  `obs_hotkey_register_frontend`, `obs_hotkey_unregister`,
  `obs_hotkey_save` y `obs_hotkey_load`.
- Las capturas horizontales pasan por `ClipManager`; nunca por un slot de UI.
- El cambio de output pasa por `SettingsManager` y conserva su validación y
  escritura atómica.
- La visibilidad del dock se publica en el thread Qt con una invocación queued.

## Ciclo de vida y persistencia

1. `obs_module_load` crea el adaptador y registra exactamente nueve acciones.
2. Un segundo `registerAll` es idempotente y no crea registros duplicados.
3. El callback de guardado del Frontend almacena los arrays devueltos por
   `obs_hotkey_save` dentro de `clipcoach_studio_hotkeys`.
4. Durante la carga del Frontend se restauran con `obs_hotkey_load`.
5. En rollback o descarga, `HotkeyManager` desregistra cada ID una sola vez y
   después se destruye el adaptador.
6. En `OBS_FRONTEND_EVENT_EXIT` se evita llamar a callbacks del Frontend una vez
   iniciada su destrucción.

La asignación se realiza en **OBS Studio → Ajustes → Atajos**. La sección
Hotkeys del dock es informativa y mantiene desactivados sus antiguos editores
Qt para evitar dos sistemas de bindings contradictorios.

## Reglas de ejecución y error

- Solo se ejecuta la transición `pressed=true`; liberar la tecla no repite la
  acción.
- Si el Replay Buffer está inactivo, `ClipManager` devuelve un error controlado:
  no se solicita guardado, se mantiene OBS operativo y se registra un warning con
  prefijo `[ClipXtudio Studio]`.
- Un fallo parcial de registro revierte todos los IDs creados en ese intento.
- Callbacks o servicios ausentes producen un resultado fallido, nunca una
  desreferencia nula.
- La captura vertical permanece registrada, pero responde con una limitación
  controlada mientras el pipeline vertical Pro todavía no esté implementado.
  No genera un archivo horizontal etiquetado falsamente como vertical.

## Pruebas

`tests/unit/hotkey-manager-test.cpp` usa un registrador simulado y valida:

- nueve acciones y nombres estables;
- registro idempotente;
- dispatch exacto de 15, 30, 60, 120 y 300 segundos;
- duración predeterminada para Marcar momento;
- callbacks vertical, output y dock;
- ignorar key release;
- error controlado con Replay Buffer inactivo;
- rollback de un registro parcial;
- desregistro completo sin duplicados.

La suite se ejecuta con:

```bash
ctest --test-dir build --output-on-failure
```

## Criterios de aceptación medibles

1. El log de arranque contiene `Registered 9 native frontend hotkeys`.

## Edición directa desde Ajustes (0.5.18)

Seis combinaciones de uso frecuente se pueden editar directamente en
**ClipXtudio > Ajustes > Hotkeys**: marcar momento, guardar 15/30/60 segundos,
guardar vertical y abrir/cerrar el dock. Cada cambio se convierte a
`obs_key_combination_t` y se aplica con `obs_hotkey_load_bindings`, por lo que
sigue siendo una hotkey nativa, global y visible en
**OBS Studio > Ajustes > Atajos**.

Las acciones de 2/5 minutos y alternar modo de salida continúan disponibles en
el panel nativo de OBS. No se usa `QShortcut` para ejecutar acciones.
2. OBS muestra nueve entradas ClipXtudio Studio en Ajustes → Atajos.
3. Activar una hotkey con otro control enfocado ejecuta exactamente una acción.
4. Las duraciones enviadas a core son `15/30/60/120/300`.
5. Reiniciar OBS conserva las combinaciones asignadas.
6. Dos llamadas a `registerAll` mantienen nueve registros, no dieciocho.
7. Replay Buffer inactivo produce warning y cero solicitudes de guardado.
8. Descargar el módulo desregistra los nueve IDs sin crash ni double-free.
