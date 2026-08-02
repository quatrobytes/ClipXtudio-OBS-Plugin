# Instalar ClipXtudio Studio en Windows

## Instalador recomendado

1. Descarga el archivo `clipcoach-studio-<versión>-windows-x64-setup.exe`.
2. Compara su SHA-256 con el archivo publicado de checksums.
3. Cierra OBS Studio.
4. Ejecuta el instalador. Windows puede solicitar permisos de administrador.
5. Confirma la ruta de OBS detectada. Puedes elegir la raíz, `bin` o
   `bin\64bit`; el instalador las normaliza a la raíz. También admite una
   instalación portable.
6. Abre OBS.
7. Ve a **Docks/Panels** y confirma que aparece **ClipXtudio Studio**.

La ruta seleccionada debe contener:

```text
bin\64bit\obs64.exe
```

El instalador coloca el DLL y sus datos en las carpetas estándar de OBS.

## ZIP portable

1. Cierra OBS.
2. Abre la carpeta raíz de OBS.
3. Extrae el contenido del ZIP manteniendo las carpetas `obs-plugins` y `data`.
4. Permite combinar carpetas, pero revisa antes de sobrescribir una versión más
   nueva.
5. Abre OBS y verifica el dock.

## Actualización

Cierra OBS y ejecuta el instalador nuevo sobre la misma ruta. La actualización
conserva configuración, biblioteca y clips.

## Desinstalación

Usa **Configuración de Windows → Aplicaciones → ClipXtudio Studio →
Desinstalar**. El desinstalador elimina el plugin, pero conserva deliberadamente
clips, exports, thumbnails, configuración y SQLite.

Si quieres borrar esos datos, haz primero un backup y sigue la eliminación
manual documentada en troubleshooting. No borres carpetas compartidas de OBS.

## Verificación rápida

- OBS abre sin advertencias del módulo.
- El dock ClipXtudio Studio está disponible.
- Replay Buffer puede iniciarse desde el dock.
- Un clip de prueba aparece en la biblioteca.

Si el dock no aparece, consulta [troubleshooting.md](troubleshooting.md).
