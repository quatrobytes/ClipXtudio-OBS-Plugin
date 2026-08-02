# ClipXtudio — actualizaciones

## Flujo del plugin

La versión se obtiene de `buildspec.json` y CMake la inyecta como
`CLIPCOACH_VERSION`. En la versión 0.5.13, el dock inicia una comprobación
asíncrona 1.5 segundos después de construirse.

Los manifiestos se consultan en este orden:

1. `http://127.0.0.1:8000/updates/latest.json`, solo para QA local.
2. `https://clipxtudio.com/updates/latest.json`, producción.

El HTTP sin cifrar solo se permite para `localhost`, `127.0.0.1` o `::1`.
Producción requiere HTTPS y TLS 1.2 o superior.

Cuando existe una versión posterior:

- aparece un punto rojo animado junto al botón del footer;
- el botón cambia a **Descargar actualización**;
- el usuario debe confirmar la instalación;
- la descarga se hace por red de manera asíncrona;
- se validan tamaño y SHA-256 antes de confirmar el archivo;
- el archivo se guarda atómicamente en la carpeta temporal;
- si no hay transmisión, grabación ni Replay Buffer activo, OBS se cierra;
- Windows ejecuta el instalador con elevación y vuelve a abrir OBS.

Un error de la comprobación automática es silencioso. La comprobación manual sí
muestra un error claro.

## Contrato del manifiesto

```json
{
  "version": "0.5.13",
  "download_url": "https://clipxtudio.com/downloads/ClipXtudio-Setup.exe",
  "release_notes_url": "https://clipxtudio.com/releases/0.5.13",
  "sha256": "64-caracteres-hexadecimales",
  "size_bytes": 12345678
}
```

Todos los campos son obligatorios salvo `release_notes_url`. El instalador no
puede superar 512 MB.

## Publicación segura

1. Compilar y probar el DLL y el instalador.
2. Firmar el binario cuando haya certificado de firma.
3. Subir el instalador a una URL HTTPS estable.
4. Calcular el SHA-256 y tamaño exacto del archivo publicado.
5. Publicar las notas de versión.
6. Publicar el manifiesto al final mediante reemplazo atómico.
7. Validar desde una instalación anterior.
# Actualización 0.5.18: HTTPS y estado visible

La instalación de Windows incluye el plugin TLS Schannel de Qt en
`data/qt-plugins/tls/qschannelbackend.dll`. El módulo registra esa carpeta antes
de inicializar licencias, actualizaciones o servicios cloud. Sin este archivo,
Qt informa `No functional TLS backend was found` y ninguna petición HTTPS puede
salir de OBS.

La comprobación ahora tiene estados observables:

- al abrir el dock comienza automáticamente después de 1.5 segundos;
- durante la consulta el botón muestra **Buscando actualizaciones** y el icono
  gira;
- una consulta manual siempre termina con **actualización disponible**, **estás
  al día** o un error con código técnico;
- una actualización disponible mantiene el indicador rojo animado y convierte
  el botón en **Descargar actualización**;
- las comprobaciones duplicadas quedan bloqueadas mientras hay una petición en
  curso.

El manifiesto público fue verificado el 30 de julio de 2026 y devolvió la versión
`0.5.17`. El instalador corregido se versiona como `0.5.18`.
