# Manual de usuario — ClipXtudio para OBS

## Inicio

Abre OBS y selecciona **Docks/Paneles → ClipXtudio**. El header muestra
versión, estado y plan efectivo. Header, Cuenta Pro y gates usan la misma
fuente de licencia.

## Capturar

Inicia Replay Buffer, elige 15 s, 30 s, 60 s, 2 min o 5 min y pulsa **Save
Clip/Marcar momento**. OBS escribe primero su replay. Las copias procesadas
toman exactamente los últimos segundos seleccionados. Si Replay está apagado,
el dock muestra un error sin cerrar OBS.

**Save Vertical Clip** usa el gate mostrado y crea una copia 9:16 sin borrar el
original. La notificación verde desaparece automáticamente.

## Vertical

- Horizontal conserva 16:9.
- Vertical Canvas habilita composición 9:16.
- Ambos conserva original y crea copia vertical.

Selecciona 1080×1920, 720×1280 o custom 9:16. Elige escena/fuente OBS; el
preview recibe video de OBS. Zoom, pan y drag ajustan el encuadre y persisten.
**Create vertical scene** crea una escena administrada por ClipXtudio sin
depender de Aitum.

Gaming Vertical, Talking Head, Tech Review y Product Review son las plantillas.
Gameplay, Camera, Subtitles, Title, Logo y Chat se activan por separado.

## Triggers

- Pre-roll: segundos anteriores a la señal.
- Post-roll: segundos posteriores; se espera este tiempo antes de guardar.
- Sensitivity: umbral mínimo de señal.
- Cooldown: tiempo mínimo entre clips automáticos.
- Action: marcar, horizontal, vertical, ambos o recomendados.

Voice Trigger escucha la fuente elegida en OBS. El modo local usa el modelo
multilingüe incluido; no depende de idiomas de Windows ni sube audio. Scene
Trigger usa el selector OBS. Keyword requiere transcripción. Chat Pulse
necesita Twitch/YouTube conectado; Kick es “Próximamente”.

## Clips

Busca por título/nombre, filtra Todos, Favoritos, Verticales y Pendientes, y
ordena por fecha o score. “Pendiente” significa que no existe un export final
registrado. Cada fila ofrece favorito, preview, export 9:16, caption,
subtítulos y abrir carpeta. Free consulta la sesión actual; Pro el historial
persistido.

## Ajustes y Pro

Los cambios válidos se guardan localmente. Las rutas deben ser escribibles. Las
hotkeys nativas aparecen en Settings → Hotkeys de OBS.

Para Pro, pega la key una sola vez. Token y refresh se guardan en
SecureStorage; la key no se conserva. Sin red, el último token confiable puede
mantener Pro durante el grace configurado. Revocación o suscripción inactiva
cierra todos los gates.

