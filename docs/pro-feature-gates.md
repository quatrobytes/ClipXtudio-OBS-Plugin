# ClipXtudio Studio — Pro Feature Gates

## Fuente de verdad

`LicenseManager` es la fuente de estado firmado y `FeatureGateService` es la
única política local de autorización. La UI no desbloquea funciones por texto,
badge, checkbox ni respuesta JSON sin verificar.

```text
Laravel -> HTTPS -> QtLicenseApi
                    |
                    v
             Rs256TokenVerifier
                    |
                    v
              LicenseManager
                    |
           FeatureGateService
             /      |       \
     ClipManager ExportManager UI
```

| Estado | Pro | Significado |
|---|---:|---|
| `Free` | No | Sin token confiable, revocado o gracia vencida |
| `ProActive` | Sí | Token firmado vigente |
| `ProGrace` | Sí, temporal | Sin red y dentro de gracia acotada |

## Matriz

La matriz normativa completa vive en [free-vs-pro.md](free-vs-pro.md).

`LicenseManager::isFeatureAllowed` y `FeatureGateService::isAllowed` aplican
`PlanPolicy`: las capacidades Free siguen permitidas en cualquier downgrade;
las Pro solo en `ProActive` o `ProGrace`.

## Integración

- `TriggerEngine::setProUnlocked` cambia con la licencia.
- `VerticalCanvasManager::setProUnlocked` aplica el mismo snapshot.
- Header y Cuenta Pro observan estado, pero no lo controlan.
- `SUBSCRIPTION_INACTIVE` borra credenciales y pasa a Free.
- Token nuevo con firma, issuer, audience o dispositivo inválido se ignora.
- Ante error de red se revalida el último token firmado.

## Offline y reloj

- Drift permitido: cinco minutos.
- `iat` demasiado futuro se rechaza.
- Activo hasta `exp + drift`.
- Gracia máxima: 72 horas desde `exp`, acotada por `grace_until`.
- Al vencer, el cache protegido se elimina y los gates cierran.
- Refresh al iniciar con cache y cada seis horas.

## Regla para nuevas funciones

1. Agregar el valor a `Feature`.
2. Documentar Free/Pro/grace en esta matriz.
3. Consultar el gate en core, no solo deshabilitar un botón.
4. Crear tests Free, ProActive, ProGrace y revocación.
5. Mantener banner o alternativa Free visible.

## Criterios verificables

- Modificar un widget no cambia Pro.
- Subscription inactiva cierra gates en la misma respuesta.
- Reiniciar OBS reconstruye estado desde `SecureStorage`.
- Cambiar token, fingerprint o `install_id` no abre gates.
- Sin red, Pro nunca continúa después de la gracia.
