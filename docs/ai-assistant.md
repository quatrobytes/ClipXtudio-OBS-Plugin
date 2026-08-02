# ClipXtudio Studio — AI Assistant Pro

## Estado validado el 30 de julio de 2026

El catálogo remoto configurado en `https://ia.snlosrv001.us` respondió por
`/api/tags` y `/v1/models`. Sin embargo, las inferencias reales probadas con
`qwen3:8b` (120 s) y `llama3.2:1b` (45 s) agotaron el tiempo de espera. Por
consiguiente, la conectividad del catálogo está verificada, pero el proveedor
no se considera operativo para producción hasta que responda una inferencia
dentro del timeout configurado.

El plugin 0.5.14 intenta primero el backend local de QA y después
`https://clipxtudio.com`. No activa automáticamente el consentimiento: el
usuario debe habilitar AI Assistant y aceptar el envío del texto de
transcripción.

La generación actual requiere `transcript_path`. Voice Trigger usa Whisper
local para detectar frases, pero ese buffer corto no constituye una
transcripción completa y temporizada del clip. Para subtítulos automáticos de
cualquier clip falta incorporar un pipeline speech-to-text del archivo
guardado; no se simula esa etapa con un LLM.

## Alcance

AI Assistant analiza texto de transcripción para generar:

- hasta diez títulos sugeridos;
- caption;
- hashtags;
- resumen del clip;
- cues o contenido SRT/VTT;
- resumen de sesión.

Es una función Pro, desactivada por defecto y con consentimiento separado. El
plugin no contiene API keys de proveedores.

## Flujo

```text
clip + transcript_path
        |
        v
AiAssistantService -- FeatureGateService / consentimiento
        |
        v
QtAiApi -- HTTPS + bearer de licencia --> Laravel /api/ai/analyze
                                              |
                                      crédito mensual + AiProvider
                                              |
        <-------------------------------------+
        |
SubtitleWriter + ClipLibraryService
        |
SQLite clip/session + .srt/.vtt
```

El procesamiento automático se solicita después de guardar un clip únicamente
si existe una transcripción, AI está activado, hay consentimiento y el gate Pro
está abierto. No se envía el archivo de video o audio.

## Configuración

- `aiAssistantEnabled`: opt-in funcional.
- `aiPrivacyConsent`: consentimiento explícito e independiente.
- `aiLanguage`: `auto`, `es` o `en`.
- Free ve la sección, aviso, controles bloqueados y banner Pro.

Cambiar idioma afecta solicitudes nuevas. No reescribe resultados existentes.

## Contratos C++

- `AiApi`: transporte inyectable y cancelable.
- `AiAssistantService`: validación, gates, respuesta y persistencia.
- `AiAssistantRequest`: scope, clip/session, transcript, idioma y request UUID.
- `AiAssistantResponse`: títulos, caption, hashtags, resumen, cues y SRT/VTT.
- `SubtitleWriter`: serialización local SRT/VTT con publicación temporal.

Límites del cliente:

| Campo | Límite |
|---|---:|
| Transcript | 100.000 bytes |
| Títulos | 10 |
| Longitud por título | 240 bytes |
| Hashtags | 30 |
| Caption | 10.000 bytes |
| Resumen | 20.000 bytes |
| SRT/VTT | 2 MB por archivo |

## Persistencia

SQLite schema v4 guarda en el clip:

- título seleccionado;
- caption;
- lista de títulos mediante blob length-prefixed;
- lista de hashtags mediante blob length-prefixed;
- resumen AI;
- idioma;
- ruta local del subtítulo.

Las sesiones guardan resumen e idioma. Las listas no usan delimitadores
ambiguos, por lo que preservan UTF-8, puntuación y saltos.

## Créditos

Laravel valida el token de licencia y la suscripción antes de procesar. La tabla
`ai_usages` mantiene un contador por usuario y mes. El crédito se incrementa
solo después de una respuesta válida del proveedor, dentro de una transacción.

`AI_MONTHLY_CREDITS` define el límite. Al agotarse se devuelve
`402 AI_USAGE_LIMIT`. El plugin muestra el error sin perder metadata previa.

## Proveedor

`AiProvider` vive exclusivamente en Laravel. `ConfiguredAiProvider` selecciona
OpenAI, Anthropic/Claude o un servidor local OpenAI-compatible usando la
configuración cifrada del panel administrativo. Si AI está apagado o falta una
configuración obligatoria, falla de forma controlada antes de enviar texto.

El resultado incluye `quality_score`, `quality_reason` y `hook_strength`, además
de los datos editoriales existentes. Consulta `docs/backend-ai-providers.md`
para configuración y controles de red. Nunca se distribuye una API key al
plugin.

## Errores

- `AI_DISABLED`
- `AI_CONSENT_REQUIRED`
- `PRO_REQUIRED`
- `AI_INVALID_INPUT`
- `AI_USAGE_LIMIT`
- `AI_NETWORK_ERROR`
- `AI_INVALID_RESPONSE`
- `AI_SUBTITLE_WRITE_FAILED`
- `AI_PERSISTENCE_FAILED`
- `AI_PROVIDER_UNAVAILABLE`

## Criterios verificables

- Free produce cero requests AI.
- Consentimiento ausente produce cero requests AI.
- Pro con mock genera y persiste título/caption/hashtags ligados al clip.
- Cues válidos crean `.srt` y `.vtt`.
- Idiomas fuera de Auto/ES/EN se rechazan antes de red.
- Un proveedor fallido no consume crédito.
- Ni binario ni configuración del plugin contienen API keys de proveedor.

## Recuperación de autorización para Caption — 0.5.44

Al iniciar el plugin, una credencial firmada por el backend tiene prioridad
sobre cualquier entitlement local de QA. Esto evita mostrar Pro mientras el
cliente AI carece del token de autorización necesario.

Si Caption detecta que el token está ausente o caducado y existe una credencial
renovable, el plugin solicita un refresh de licencia y reintenta el análisis una
sola vez. Un fallo de autorización se muestra separado de un fallo de red o de
configuración del proveedor. El backend continúa siendo la autoridad sobre el
plan, las features y la cuota mensual.

## Caption bajo demanda desde Clips — 0.5.30

La acción Caption de la biblioteca usa este orden:

1. reutiliza `transcript_path` si el clip ya tiene una transcripción;
2. si falta, el plugin extrae audio mono de 16 kHz con el FFmpeg incluido;
3. Whisper procesa ese audio localmente en el equipo;
4. `AiAssistantService` envía exclusivamente el texto de transcripción al
   backend configurado;
5. el caption y sus hashtags se guardan ligados al clip en SQLite;
6. la UI muestra el resultado en un diálogo copiable.

El flujo exige licencia Pro, AI activada y consentimiento de privacidad. El
video, el audio, sus rutas locales y las claves del proveedor no se envían al
backend. Un error de extracción, transcripción, red o proveedor se muestra de
forma controlada y no bloquea OBS.

Los subtítulos automáticos permanecen fuera del alcance de 0.5.30. Su acción se
muestra deshabilitada con el texto `Próximamente`; no se crea un archivo SRT/VTT
ficticio ni se presenta una operación incompleta como disponible.
