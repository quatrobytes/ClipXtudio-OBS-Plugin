# ClipXtudio Studio — Release y empaquetado Windows

> Actualización 0.4.1: el layout vigente es
> `clipxtudio/bin/64bit/clipxtudio.dll` y
> `clipxtudio/data/{locale,assets,models,tools}`. `tools/ffmpeg` contiene
> FFmpeg x64, licencia, README, origen y checksums. ZIP e instalador deben
> incluirlo; el usuario no instala FFmpeg ni modifica `PATH`.

## Artefactos

Una release Windows x64 produce:

- `clipcoach-studio-<version>-windows-x64-setup.exe`;
- `clipcoach-studio-<version>-windows-x64.zip`;
- `clipcoach-studio-<version>-windows-x64-SHA256SUMS.txt`.

El instalador usa Inno Setup 6. El ZIP portable contiene directamente:

```text
obs-plugins/
  64bit/
    clipxtudio.dll
data/
  obs-plugins/
    clipcoach-studio/
      license-public.pem
      locale/
      assets/                 # cuando existan
```

## Versionado

`buildspec.json` es la fuente única de versión y debe usar
`MAJOR.MINOR.PATCH`. La misma versión debe existir en:

- `CHANGELOG.md`;
- `docs/release-notes/<version>.md`;
- recursos VERSIONINFO del DLL;
- nombre y metadata del instalador.

Para una nueva compilación Release de Windows se debe usar el punto de entrada
versionado:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File build-aux/build-versioned-release.ps1 `
  -Configuration Release `
  -ReleaseSummary "Resumen corto de los cambios"
```

El comando incrementa automáticamente `PATCH` antes de configurar CMake,
sincroniza `buildspec.json`, Inno Setup, changelog y notas de release. Si CMake
o MSBuild fallan, restaura todos esos metadatos para no publicar una versión
que nunca compiló. `-NoIncrement` se reserva para reconstruir exactamente la
versión ya declarada.

Validación:

```powershell
pwsh tests/packaging/Test-ReleaseMetadata.ps1
```

Incrementos:

- PATCH: bugfix compatible;
- MINOR: capacidad compatible;
- MAJOR: cambio incompatible de DB, API o requisitos.

## Build Release local

Requisitos:

- Windows x64;
- Visual Studio 2022;
- CMake 3.28+;
- PowerShell 7.2+;
- Inno Setup 6;
- dependencias de `obs-plugintemplate`.

El archivo `data/license-public.pem` debe ser la clave pública del mismo par RSA
que firma tokens en Laravel. Nunca se empaqueta la clave privada.

```powershell
$env:CI = 'true'
cmake --preset windows-ci-x64
cmake --build --preset windows-x64 --config Release --parallel
ctest --test-dir build_x64 -C Release --output-on-failure
cmake --install build_x64 --prefix release/Release --config Release
pwsh .github/scripts/Package-Windows.ps1 -Configuration Release
```

El requisito histórico de `$env:CI` aplica al script de build del template; el
script de packaging puede ejecutarse localmente.

También se puede abrir `installers/windows/clipcoach-studio.iss` y pulsar
**Build → Compile**. El archivo usa por defecto:

- source: `release/Release`;
- output: `release`;
- versión: la versión SemVer validada contra `buildspec.json`.

Primero debe existir el resultado de `cmake --install` y debe contener el DLL,
locales, assets y `license-public.pem`. El pipeline continúa pasando estas
variables explícitamente para builds reproducibles.

## Detección de OBS

El instalador busca `InstallLocation` en:

- HKLM 64-bit, uninstall key `OBS Studio`;
- HKCU, uninstall key `OBS Studio`;
- `Program Files\obs-studio`.

La pantalla de directorio permanece disponible. Tanto UI como instalación
silenciosa rechazan una ruta que no contenga `bin\64bit\obs64.exe`.

La instalación interactiva acepta y normaliza automáticamente cualquiera de
estas selecciones:

- raíz de OBS;
- carpeta `bin`;
- carpeta `bin\64bit`;
- ruta completa a `obs64.exe` devuelta por el registro.

Para `/VERYSILENT /DIR=...`, el test automatizado confirma la normalización de
`bin` hacia la raíz y evita instalar el plugin bajo una carpeta incorrecta.

## Firma obligatoria para distribución pública

El empaquetado soporta:

- `WINDOWS_SIGNING_CERT_BASE64` y `WINDOWS_SIGNING_CERT_PASSWORD`; o
- `WINDOWS_SIGNING_CERT_SHA1` para un certificado ya instalado.

Se firma primero el DLL y luego el instalador usando SHA-256 y timestamp RFC
3161. `signtool verify /pa /v` debe pasar. Cuando CI solicita `codesign`, la
ausencia de credenciales detiene el build: nunca se publica silenciosamente un
instalador Windows sin firma. Un build local interno puede omitir la firma, pero
no es distribuible a usuarios.

El certificado debe ser RSA y encadenar a una autoridad confiable de Windows.
Un certificado autofirmado no evita el bloqueo de Smart App Control. Consulta
`docs/windows-code-signing.md` para configurar los secretos y verificar el EXE.

La clave RSA de licencia es diferente del certificado Authenticode.

## CI/CD

El workflow de build:

1. valida formato y quality gates;
2. inyecta `LICENSE_SIGNING_PUBLIC_KEY_B64` como `data/license-public.pem`;
3. compila configuración `Release`;
4. ejecuta CTest;
5. instala al staging CMake;
6. crea EXE y ZIP;
7. ejecuta instalación, upgrade, uninstall y validación portable;
8. firma si hay certificado;
9. genera SHA-256;
10. publica artifacts;
11. en tags SemVer, adjunta EXE/ZIP al draft de GitHub Release.

Secrets CI:

| Secret | Obligatorio |
|---|---|
| `LICENSE_SIGNING_PUBLIC_KEY_B64` | sí para paquete Pro |
| `WINDOWS_SIGNING_CERT_BASE64` | requerido para distribución pública, salvo certificate store |
| `WINDOWS_SIGNING_CERT_PASSWORD` | cuando el PFX tiene password |
| `WINDOWS_SIGNING_CERT_SHA1` | alternativa con certificate store confiable |

## Upgrade y rollback

- El `AppId` de Inno Setup es estable entre versiones.
- Upgrade reemplaza binario y recursos propiedad del paquete.
- Settings, DB, clips y exports viven fuera del directorio de instalación.
- No se soporta downgrade si la DB tiene schema superior al binario anterior.
- Para rollback, preservar DB, instalar la versión previa compatible y restaurar
  únicamente un backup con schema compatible.

## Política de desinstalación

El desinstalador elimina solo archivos registrados por Inno Setup. No contiene
`[UninstallDelete]` recursivo y no toca:

- clips;
- exports;
- thumbnails/transcripts/subtitles;
- `%APPDATA%\obs-studio\plugin_config\clipcoach-studio`;
- DB o settings del usuario.

La eliminación de datos es una operación manual separada. No se ofrece una
casilla ambigua durante uninstall.

## Release gate

- 100% CI verde;
- installer QA en VM Windows limpia;
- OBS carga el DLL y muestra el dock;
- firma válida para beta pública;
- checksum publicado y verificado;
- upgrade desde la beta anterior;
- uninstall preserva hashes de clips;
- checklist `docs/qa-windows-checklist.md` aprobado;
- cero S0/S1/S2 abiertos.
