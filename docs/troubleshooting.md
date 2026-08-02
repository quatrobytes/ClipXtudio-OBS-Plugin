# ClipXtudio Studio — Troubleshooting y recolección de logs

## Voice Trigger no escucha

1. Confirma Pro, toggle Voice y modo Local.
2. Selecciona explícitamente una fuente con audio en OBS.
3. El estado debe indicar modelo listo y audio recibido.
4. `data\models\ggml-tiny-q5_1.bin` debe medir más de 30 MB; un archivo
   pequeño es un placeholder/truncado y requiere reinstalar.
5. No instales paquetes de voz de Windows: el recognizer incluido es local,
   multilingüe y consume PCM de OBS.

## Export 9:16 falla o queda en 0 %

`data\tools\ffmpeg\ffmpeg.exe` debe existir y medir más de 50 MB. El instalador
oficial lo incluye. “Media engine is missing” indica instalación incompleta.
Verifica permisos y espacio de la carpeta de export.

## Pro difiere entre header y Cuenta Pro

Ambos observan `LicenseManager`. Una respuesta de suscripción inactiva, token
inválido o dispositivo distinto cierra todos los gates. Conserva el log sin
copiar la key si la discrepancia continúa.

## El dock no aparece

1. Confirma que usas OBS x64.
2. Abre **Docks/Panels** y busca ClipXtudio Studio.
3. Verifica:

```text
<OBS>\obs-plugins\64bit\clipcoach-studio.dll
<OBS>\data\obs-plugins\clipcoach-studio\locale\en-US.ini
```

4. Reinstala seleccionando la carpeta que contiene `bin\64bit\obs64.exe`.
5. Revisa el log por líneas con `[ClipXtudio Studio]`.

Si aparece “Select a valid OBS Studio folder”, selecciona normalmente:

```text
C:\Program Files\obs-studio
```

Las versiones actuales también aceptan `...\obs-studio\bin`,
`...\obs-studio\bin\64bit` y la ruta `obs64.exe` registrada por OBS. Una carpeta
que no contenga una instalación OBS real seguirá siendo rechazada.

## Error de carga

Las causas más frecuentes son:

- OBS abierto durante instalación;
- mezcla de OBS x86/x64;
- DLL bloqueado por antivirus;
- versión OBS no soportada;
- dependencia runtime ausente;
- ZIP extraído un nivel demasiado profundo.

No descargues DLLs sueltos desde sitios externos. Reinstala Visual C++ Runtime
x64 desde Microsoft y usa el artefacto oficial.

## Replay Buffer apagado

Actívalo desde Capturar o en **Settings → Output → Replay Buffer**. El plugin
debe mostrar un error controlado; no es necesario reiniciar OBS.

## Pro no activa

- Confirma conexión HTTPS y fecha/hora de Windows.
- No compartas ni pegues la key en un reporte.
- Una key consumida en otro dispositivo devuelve
  `LICENSE_KEY_ALREADY_USED`.
- Si falta `license-public.pem`, la build no puede validar tokens y no es una
  distribución Pro válida.

## DB corrupta

ClipXtudio conserva el archivo como:

```text
clipcoach.db.corrupt-<timestamp>
```

y abre una biblioteca limpia. No borres el backup. Adjunta solamente el log al
reporte inicial; la DB puede contener rutas o metadata privada.

## Obtener logs de OBS

En OBS:

1. **Help → Log Files → Upload Current Log File**, o
2. **Help → Log Files → View Current Log** y guarda una copia.

Filtra por `[ClipXtudio Studio]`. Antes de compartir:

- elimina tokens, keys y URLs con parámetros;
- revisa nombres de usuario y rutas locales;
- no adjuntes transcripts/chat sin consentimiento.

## Crash reports

Después de reiniciar OBS:

1. abre **Help → Crash Reports**;
2. conserva el crash log más reciente;
3. incluye pasos exactos y hora del crash;
4. indica Windows, OBS, ClipXtudio, CPU/GPU/driver;
5. indica si ocurre con otros plugins deshabilitados.

No envíes dumps completos públicamente si pueden contener memoria sensible.
Usa el canal privado indicado por el equipo beta.

## Diagnóstico mínimo para un bug

- versión y SHA del instalador;
- OBS y Windows;
- log completo sanitizado;
- pasos y resultado esperado/real;
- reproducibilidad;
- archivo de export de ejemplo solo si no contiene contenido privado.

Todo bug confirmado debe convertirse en un test de regresión.
