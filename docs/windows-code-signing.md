# Firma de código para Windows

## Motivo

Los instaladores públicos de ClipXtudio deben estar firmados con Authenticode.
Windows Smart App Control puede bloquear un EXE sin firma porque no puede
verificar su editor. Desactivar Smart App Control o usar un certificado
autofirmado no es una solución de distribución.

El certificado debe ser RSA, incluir el uso extendido **Code Signing** y
encadenar a una autoridad del Microsoft Trusted Root Program. El certificado
RSA que firma tokens de licencia de ClipXtudio es independiente y nunca debe
usarse para firmar binarios.

## Credenciales admitidas por el pipeline

El empaquetado Windows admite una de estas alternativas:

1. PFX exportable mediante secretos de GitHub Actions:
   - `WINDOWS_SIGNING_CERT_BASE64`: contenido Base64 del PFX;
   - `WINDOWS_SIGNING_CERT_PASSWORD`: contraseña del PFX.
2. Certificado disponible en el almacén del runner:
   - `WINDOWS_SIGNING_CERT_SHA1`: huella SHA-1 del certificado.

Para convertir un PFX a Base64 sin saltos de línea:

```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes('ClipXtudio-CodeSigning.pfx')) |
  Set-Clipboard
```

La clave privada no se guarda en el repositorio, no se incluye en artefactos y
no debe aparecer en logs.

## Comportamiento del build

Cuando `codesign: true`, el action de empaquetado pasa `-RequireSigning` a
`Package-Windows.ps1`. El build falla si no encuentra credenciales válidas.
No se publicará silenciosamente un instalador Windows sin firma.

El proceso firma, en este orden:

1. `clipxtudio.dll`;
2. el instalador `clipxtudio-<version>-windows-x64-setup.exe`.

Ambos usan SHA-256, timestamp RFC 3161 y se verifican con `signtool` antes de
publicar los artefactos.

## Verificación antes de publicar

```powershell
$installer = 'release\clipxtudio-<version>-windows-x64-setup.exe'
Get-AuthenticodeSignature $installer | Format-List Status,StatusMessage,SignerCertificate
signtool verify /pa /v $installer
```

El estado debe ser `Valid`, el sujeto debe corresponder a la identidad legal de
ClipXtudio/QuatroBytes y la cadena debe ser confiable. También se debe probar el
instalador en una VM limpia con Smart App Control activado.

## Recuperación de la versión 0.5.99

El EXE 0.5.99 existente está sin firma y no se debe seguir distribuyendo. Una
vez configurada una identidad de firma confiable:

1. incrementar la versión;
2. reconstruir DLL e instalador desde fuente limpia;
3. comprobar ambas firmas;
4. ejecutar la matriz de instalación/upgrade/uninstall;
5. reemplazar enlaces de descarga y publicar checksum SHA-256.

No se debe renombrar ni volver a subir el EXE 0.5.99 actual como supuesto
arreglo: el contenido seguiría sin firma.
