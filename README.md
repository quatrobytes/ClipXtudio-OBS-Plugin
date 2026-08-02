# ClipXtudio OBS Plugin

ClipXtudio es un plugin nativo C++17 para OBS Studio. Añade un dock Qt para
capturar, organizar, recortar y exportar clips horizontales y verticales sin
exponer OBS WebSocket a Internet.

> Este repositorio contiene únicamente el cliente nativo para OBS. ClipXtudio
> Hub, el backend Laravel, facturación, licencias, Remote Clipper, proveedores
> de IA e infraestructura de producción son servicios privados separados y no
> forman parte de este repositorio.

## Funciones principales

- captura manual mediante Replay Buffer y hotkeys nativas;
- reconocimiento de frases de voz ejecutado localmente;
- biblioteca local SQLite;
- composición y exportación vertical 9:16;
- editor rápido de clips;
- triggers y puntuación local;
- perfiles de configuración importables y exportables;
- integración autenticada con ClipXtudio Hub para funciones Pro;
- recepción segura de solicitudes de Remote Clipper mediante HTTPS saliente.

## Privacidad

Los clips, audio, rutas locales y archivos multimedia no se cargan
automáticamente al backend. Las operaciones en línea envían únicamente los
datos mínimos descritos en [la política de privacidad del plugin](docs/privacy.md).
El cliente no contiene claves privadas de firma, credenciales de Stripe ni
claves de proveedores de IA.

## Compilar

Requisitos y pasos completos: [Entorno de desarrollo](docs/development-setup.md).

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo --parallel
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
```

Las versiones de OBS, Qt y dependencias se fijan en `buildspec.json`. No se
aceptan cambios que introduzcan credenciales, desactiven validaciones TLS o
trasladen reglas de autorización del servidor al cliente.

## Instalación

Los instaladores oficiales se publican en GitHub Releases y deben estar
firmados por QuatroBytes. Comprueba la firma y el checksum antes de instalar.
Consulta [Instalación](docs/user-installation.md) y
[Solución de problemas](docs/troubleshooting.md).

## Contribuir

Consulta [CONTRIBUTING.md](CONTRIBUTING.md). Los reportes de vulnerabilidades no
deben abrirse como issues públicos; sigue [SECURITY.md](SECURITY.md).

## Builds oficiales y marca

La licencia del código no concede derechos sobre las marcas ClipXtudio,
QuatroBytes, sus logotipos ni la apariencia de un build oficial. Las versiones
modificadas deben evitar confundir a los usuarios y cumplir
[TRADEMARKS.md](TRADEMARKS.md).

## Licencia

El código del plugin se distribuye bajo **GNU GPL v2.0 o posterior** para ser
compatible con OBS Studio. Consulta [LICENSE](LICENSE) y [NOTICE](NOTICE).

Los componentes de terceros conservan sus propias licencias; consulta
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Copyright © 2026 QuatroBytes / MrJimeneX.

Sitio oficial: [clipxtudio.com](https://clipxtudio.com)
