# Hardening del plugin nativo

## Autoridad y límites

Laravel es la autoridad de pago, licencia, dispositivo y funciones. El plugin
solo concede Pro con un token RS256 válido emitido para su activación. Cambiar
un widget o un valor visual no evita los controles de `FeatureGateService` en
core.

Ningún cliente nativo es imposible de modificar. Estas medidas elevan el costo
del abuso y reducen su duración; no sustituyen refresh, revocación y control de
cuotas en el servidor.

## Token de licencia

El verificador nativo exige algoritmo RS256, `kid`, issuer, audience, subject,
JTI, dispositivo, `license_id`, `device_activation_id`, `token_version`, `iat`
y `exp`. Rechaza:

- firma, algoritmo o clave pública incorrectos;
- payload manipulado o claims obligatorios ausentes;
- activación distinta del `sub`;
- reloj fuera de la tolerancia configurada;
- versión de token cero o inválida.

La clave privada nunca se distribuye. El binario contiene únicamente la clave
pública de verificación.

## Persistencia

- Windows: Credential Manager, credencial `ClipXtudioStudio.License`; los blobs
  temporales se limpian con `SecureZeroMemory`.
- La license key se limpia tras activar y no se serializa.
- Se persisten token firmado, refresh token rotatorio, `install_id` y metadata
  mínima de expiración.
- macOS y Linux fallan cerrados hasta implementar Keychain y Secret Service.
  No existe fallback a texto plano.

## Red, fallback y actualizaciones

- Producción acepta exclusivamente HTTPS y validación CA del sistema.
- HTTP loopback solo existe en builds de desarrollo compilados con
  `CLIPX_ALLOW_INSECURE_LOCAL_API`; está apagado en el build de producción.
- El fallback cloud ocurre solo ante errores de red/servidor. Una key inválida
  o respuesta inválida no se reintenta contra otro entorno.
- Hay timeouts y no se desactiva la verificación TLS.
- No se habilita pinning rígido por defecto porque bloquearía rotación normal
  de certificados. Puede añadirse con pins solapados y canal de recuperación.
- El updater valida tamaño y SHA-256. En Windows, un build de producción además
  exige firma Authenticode válida y cadena confiable antes de ejecutar.

## Releases

El workflow soporta Authenticode con certificado y timestamp. Los tags/releases
fallan si no hay material de firma; un paquete público no debe publicarse sin
firma. La firma se verifica después de generar DLL e instalador.

## Respuesta a fallos

- Token inválido o manipulado: Free inmediato.
- Backend `revoked`, `expired` o subscription terminal: Free y limpieza del
  cache protegido.
- Offline dentro de gracia firmada: Pro con advertencia.
- Offline fuera de gracia: Free.
- Errores de red nunca producen Pro por defecto.

## Validación

`ctest --test-dir build_x64_final -C RelWithDebInfo --output-on-failure`
cubre firma válida/manipulada, token incompleto, gracia offline, revocación,
feature gates, no persistencia de la key y fallback de red.

