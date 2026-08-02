# Instalación de ClipXtudio 0.5.1

## Requisitos

- Windows x64.
- OBS Studio 64-bit.
- Permisos de administrador para el instalador.
- OBS cerrado durante instalación/actualización.

## Instalador

1. Cierre OBS únicamente si no está grabando ni transmitiendo.
2. Ejecute `ClipXtudio-Setup.exe` como administrador.
3. Inicie OBS.
4. Abra `Docks > ClipXtudio`.

El instalador coloca el plugin en:

`C:\ProgramData\obs-studio\plugins\clipxtudio`

Código de salida 2 en instalación silenciosa indica que no hubo elevación UAC.

## ZIP portable

Extraiga `clipcoach-studio` dentro de:

`C:\ProgramData\obs-studio\plugins\`

La DLL debe quedar en:

`C:\ProgramData\obs-studio\plugins\clipxtudio\bin\64bit\clipxtudio.dll`

## Actualización

OBS mantiene la DLL bloqueada mientras está abierto. Cierre OBS de forma segura,
instale la nueva versión y vuelva a abrirlo. Verifique el log más reciente en
`%APPDATA%\obs-studio\logs`.

## Desinstalación

Use Aplicaciones instaladas o elimine solo la carpeta del plugin. El
desinstalador preserva clips, exports, thumbnails, settings y la base local.
