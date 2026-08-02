# Privacy

## Principios

Voice Trigger está desactivado por defecto y es una función Pro opt-in. Free no
procesa audio. La selección de modo, idioma, frases y consentimiento se almacena
localmente; el consentimiento cloud nunca se infiere.

## Modo local

- La fuente se selecciona entre las fuentes de audio de OBS.
- El PCM permanece en memoria.
- No se persiste audio ni transcripción.
- El segmento se libera después del reconocimiento.
- El modelo y runtime local no deben incluir telemetría oculta.
- No se requieren servicios, permisos de idioma o motores Speech de Windows.

## Modo cloud

- Requiere consentimiento explícito y reversible.
- Solo se envía el segmento necesario, máximo ocho segundos por defecto.
- No se envía el replay completo, video, chat, escenas ni biblioteca de clips.
- Las frases configuradas no se envían salvo que el contrato futuro lo requiera
  y se documente por separado.
- El plugin no tiene actualmente transporte cloud activo; por tanto, esta build
  no transmite audio.

## Datos derivados

El evento guarda tipo Voice, score, timestamp y frase coincidente para el flujo
del clip. No guarda la transcripción completa ni PCM. La UI de eventos recientes
vive en memoria y está limitada a 100 elementos.

## Identidad de instalación y licencias

Para ligar una activación Pro a un dispositivo se genera un `install_id` UUID
aleatorio. Se combina localmente con un identificador estable del sistema cuando
está disponible y se aplica SHA-256 con separación de dominio.

- No se lee ni almacena una MAC address.
- El backend recibe el hash, nunca el identificador estable crudo.
- El identificador crudo no se incluye en logs, SQLite, analíticas ni metadata
  de clips.
- El `install_id` permanece en la carpeta de configuración del usuario para
  mantener la activación entre reinicios.
- Borrar esa identidad puede requerir una nueva activación o un reset de
  dispositivo gestionado por soporte.
- License y refresh tokens se conservan en el almacén seguro del sistema; no hay
  fallback a archivos de texto.

El backend utiliza el hash solo para aplicar el límite de dispositivos, prevenir
reutilización de keys y auditar activaciones. La política pública debe definir
retención y proceso de eliminación antes de producción.

## AI Assistant

AI Assistant está desactivado por defecto y requiere dos condiciones separadas:
licencia Pro y consentimiento de privacidad. Activar Pro no implica
consentimiento.

Datos enviados al backend propio:

- texto de la transcripción, máximo 100.000 bytes;
- identificador interno de clip y sesión;
- idioma solicitado (`auto`, `es`, `en`);
- request ID y token de licencia.

Datos que no se envían:

- video o audio;
- thumbnails;
- ruta local o nombre del archivo;
- escenas o fuentes OBS;
- API keys del proveedor;
- biblioteca completa, salvo texto agregado elegido para un resumen de sesión.

Los títulos, captions, hashtags, resumen y subtítulos devueltos se almacenan
localmente. Laravel conserva el contador mensual de uso por usuario; no necesita
persistir el transcript para controlar créditos. El administrador puede
seleccionar OpenAI, Anthropic/Claude o infraestructura local/privada. La
pantalla muestra el proveedor activo, pero nunca revela la clave guardada. Para
proveedores cloud se deben revisar retención, región y subprocesadores antes de
habilitarlos en producción. En modo local el transcript se envía únicamente a
la URL configurada por el administrador.

Desactivar AI impide solicitudes nuevas y no borra automáticamente resultados
locales. El usuario conserva control sobre sus archivos y puede retirar el
consentimiento sin perder clips.

## Controles del usuario

- Activar/desactivar Voice Trigger.
- Elegir la fuente de audio OBS.
- Elegir Local o Cloud.
- Elegir idioma.
- Agregar/quitar frases.
- Ajustar sensibilidad y cooldown.
- Revocar consentimiento cloud.

## Retención y borrado

Settings persiste preferencias hasta que el usuario las cambie o elimine la
configuración del plugin. El backend cloud futuro debe declarar región,
subprocesadores y retención; ClipXtudio exigirá cero retención para audio y
transcripciones siempre que el proveedor lo permita.
# Customer account activity

When a Pro customer uses a signed-in license, ClipXtudio can synchronize limited clip metadata for the private web account: date, duration, orientation, trigger category, score, session identifier and application version. The clip media, audio, thumbnail, caption files, file name and local filesystem path remain on the customer device and are not sent by the activity endpoint.
