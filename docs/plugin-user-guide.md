# Guía del plugin ClipXtudio

## Abrir el dock

En OBS use `Docks > ClipXtudio`. El header muestra versión, estado, plan,
Replay Buffer y resolución vertical.

## Capturar

1. Configure Replay Buffer y un encoder hardware en OBS.
2. Pulse Start Replay Buffer.
3. Elija 15 s, 30 s, 60 s, 2 min o 5 min.
4. Use Mark Moment o Save Clip.
5. Detenga el buffer cuando no lo necesite.

Si el buffer está apagado, el plugin muestra error y no intenta guardar.
La duración real nunca puede superar el material acumulado por OBS.

## Vertical

Seleccione Horizontal, Vertical o Both, resolución y plantilla. La vista previa
usa una escena/fuente real de OBS. Puede activar gameplay, cámara, subtítulos,
título, logo y chat. El export MVP crea MP4 H.264 9:16; todavía no reproduce
todos los elementos avanzados del preview en el archivo final.

## Triggers

- Pre-roll: tiempo anterior a la señal.
- Post-roll: tiempo posterior a la señal.
- Sensitivity: nivel necesario para aceptar la señal.
- Cooldown: evita clips duplicados.
- Action: guardar, marcar, guardar vertical/ambos o recomendar.

Voice Trigger toma PCM de la fuente seleccionada en OBS y usa el modelo Whisper
incluido; no depende del idioma de Windows. Debe verse `model=ready`,
`audio=receiving` y una licencia Pro activa.

## Clips

La biblioteca permite filtros, orden, preview, favoritos, carpeta, captions,
subtítulos y export 9:16. `Pending` significa que una copia/proceso todavía no
terminó.

## Ajustes y Pro

Las rutas, nombres, notificaciones, exportación, idioma e integraciones se
persisten localmente. Las funciones Pro se validan también en core. La versión
pública se comunica con `https://clipxtudio.com`; el token se guarda mediante el
almacenamiento seguro del sistema y se refresca según la política backend.
Para desarrollo, el mismo código puede configurarse contra
`http://127.0.0.1:8000` únicamente habilitando la excepción loopback al generar
el proyecto.

## Rendimiento

No ejecute dos buffers simultáneos. Use NVENC, Quick Sync o AMF cuando esté
disponible. En la máquina QA, el encoder CPU elevó el uso a ~70%.

## Compra Pro: limitación actual

Cuenta Pro permite pegar una licencia, activarla y gestionar una membresía
existente. La versión auditada todavía no consulta el catálogo público del
backend ni muestra cards Monthly/Annual, promoción o botones de compra.

Hasta implementar ese cliente, compre desde el landing web y pegue la licencia
recibida. No presentar esta limitación como checkout integrado en el plugin.

Cuando el backend devuelve Free/expired, el plugin usa el mensaje:

`Your Pro subscription expired. ClipXtudio has returned to Free.`
# My account

In **Settings > Pro account**, **Open my account** opens the customer portal for the configured ClipXtudio service. The portal shows the current membership, renewal or courtesy expiration, masked licenses, registered devices, Stripe billing access and clip activity metadata. A founder/owner license is displayed as never expiring with unlimited use.
