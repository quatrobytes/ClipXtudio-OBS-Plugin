# ClipXtudio Studio — Free vs Pro

## Política de producto

Free conserva el flujo principal completo: iniciar Replay Buffer, guardar clips
manuales, usar hotkeys básicas, revisar la sesión actual y producir exports
horizontal o vertical limitado sin watermark. Pro vende automatización, escala,
canvas vertical real y enriquecimiento; no degrada archivos ya creados.

## Matriz v1

| Feature (`Feature`) | Free | Pro |
|---|---:|---:|
| `ManualCapture` | Sí | Sí |
| `BasicHotkeys` | Sí | Sí |
| `ThreeQuickDurations` | 15s, 30s y 60s | Sí |
| `CurrentSessionHistory` | Sí | Sí |
| `BasicHorizontalExport` | Sí | Sí |
| `LimitedVerticalExport` | Sí, crop básico | Sí |
| `BasicVerticalTemplate` | Gaming Vertical | Sí |
| `UnlimitedDurations` | No | Sí |
| `VerticalCanvas` | No | Sí |
| `HorizontalAndVertical` | No | Sí |
| `VoiceTrigger` | No | Sí |
| `AudioSpike` | No | Sí |
| `ChatPulse` | No | Sí |
| `SceneTrigger` | No | Sí |
| `AdvancedClipScore` | No | Sí |
| `AiTitles` | No | Sí |
| `AiCaptions` | No | Sí |
| `AutoSubtitles` | No | Sí |
| `BatchExport` | No | Sí |
| `FullHistory` | No | Sí |
| `PremiumVerticalTemplates` | No | Sí |
| `SessionRecap` | No | Sí |
| `AiHookFinder` | No | Sí |

Esta matriz expresa autorización. Las capacidades que todavía pertenezcan al
roadmap, como generación AI, no se consideran implementadas por el mero hecho de
tener un gate.

## Arquitectura

```text
signed LicenseSnapshot
          |
          v
 FeatureGateService ----> UI (estado, ProBadge, banner)
          |
          +-------------> ClipManager
          +-------------> ExportManager
          +-------------> TriggerEngine
          +-------------> VerticalCanvasManager
```

- `Feature` es el catálogo estable de capacidades.
- `PlanPolicy` contiene exclusivamente la matriz Free/Pro.
- `FeatureGateService` convierte el estado de entitlement en plan efectivo y
  devuelve una decisión con código y mensaje.
- `LicenseManager` verifica la licencia; la UI no puede crear estado Pro.
- `ProActive` y `ProOfflineGrace` usan política Pro.
- `Free`, `Expired` y `Revoked` usan política Free.

## Reglas de enforcement

1. Toda acción que tenga implementación real valida el gate en core antes de
   invocar OBS, FFmpeg, almacenamiento o red.
2. La UI repite la decisión para orientar, pero no reemplaza la validación core.
3. Un control bloqueado permanece visible con `ProBadge`, tooltip o
   `UpgradeBanner`.
4. El error estable de producto es `PRO_REQUIRED`.
5. Al expirar o revocarse Pro, nuevas acciones avanzadas se bloquean
   inmediatamente. Una operación ya iniciada puede terminar de forma segura.
6. Nunca se borran clips, presets o metadata por downgrade.

## Enforcement implementado

- `ClipManager`: 2min, 5min y otras duraciones fuera de 15/30/60 requieren
  `UnlimitedDurations`.
- `ExportManager`: batch requiere `BatchExport`; Vertical Canvas requiere
  `VerticalCanvas`; orientación Both requiere `HorizontalAndVertical`.
- `VerticalCanvasManager`: modo Vertical real, Both y plantillas premium se
  rechazan en Free.
- `TriggerEngine`, Voice y Chat: rechazan triggers Pro aun sin dock abierto.
- Clips UI: batch y Session Recap se muestran con badge y estado bloqueado.
- Capturar UI: duraciones extendidas siguen visibles con badge y banner
  “Mejorar a Pro”.

## Criterios de aceptación medibles

- Modificar `enabled=true` en un widget no permite una acción Pro en core.
- Free guarda clips manuales de 15, 30 y 60 segundos.
- Free recibe `PRO_REQUIRED` para 120/300 segundos sin llamar a ReplayManager.
- Free puede ejecutar export horizontal y vertical limitado individual.
- Free recibe `PRO_REQUIRED` para batch, Both y Vertical Canvas antes de crear
  trabajos.
- ProActive y ProOfflineGrace permiten todas las capacidades de la matriz.
- Expired y Revoked bloquean todas las capacidades Pro y mantienen las siete
  capacidades Free.
- Cada superficie bloqueada probada contiene un `ProBadge` y texto de upgrade.

