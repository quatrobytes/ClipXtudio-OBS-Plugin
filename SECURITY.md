# Seguridad

## Reportar una vulnerabilidad

No publiques vulnerabilidades, tokens, bypasses de licencia ni datos de
usuarios en un issue público. Repórtalos mediante el canal privado de soporte
en [clipxtudio.com](https://clipxtudio.com) indicando:

- versión afectada;
- sistema operativo y versión de OBS;
- pasos mínimos para reproducir;
- impacto observado;
- logs redactados, sin tokens, rutas personales ni licencias.

## Alcance público

Este repositorio contiene el cliente nativo. El backend, facturación y la
infraestructura se gestionan y reportan por separado. Una respuesta del
servidor no debe considerarse confiable sin las validaciones existentes y el
cliente nunca debe contener secretos de proveedor.

## Versiones compatibles

Solo la versión estable más reciente recibe correcciones de seguridad. Los
builds oficiales se identifican mediante firma digital y release publicada por
QuatroBytes.

## Clave pública de licencia

`data/license-public.pem` se publica intencionalmente. Es una clave de
verificación: no puede emitir licencias ni firmar tokens. La clave privada
correspondiente nunca forma parte de este repositorio ni del instalador.
