# ClipXtudio Studio — Checklist QA manual Windows/OBS

**Versión plugin:** ______  
**Commit/SHA:** ______  
**Tester/fecha:** ______  
**Windows/OBS:** ______  
**CPU/GPU/driver:** ______  
**Escala DPI:** ______  
**Instalador SHA-256:** ______

Marcar cada caso `PASS`, `FAIL` o `N/A` con evidencia. Un `N/A` P0 requiere
aprobación del QA Lead.

## Preparación

- [ ] QA-WIN-00 — Crear snapshot de VM y respaldar perfil OBS.
- [ ] Confirmar OBS cerrado e instalar artifact firmado x64.
- [ ] Confirmar que el instalador no solicita borrar datos de usuario.
- [ ] Preparar escenas Gameplay/Cámara, audio, Replay Buffer y carpeta escribible.
- [ ] Preparar una key nueva y otra consumida en Stripe sandbox.

## Lifecycle y UI

- [ ] QA-WIN-01 — Abrir OBS; el log muestra carga de ClipXtudio y no hay crash.
- [ ] QA-WIN-02 — El dock aparece, se mueve, flota, acopla, redimensiona y reabre.
- [ ] QA-WIN-03 — Recorrer Capturar/Vertical/Triggers/Clips/Ajustes 20 veces.
- [ ] QA-WIN-04 — Validar 100%, 125%, 150% y 200% DPI; sin texto cortado.
- [ ] QA-WIN-05 — Cambiar locale ES/EN y reiniciar; textos principales correctos.

## Captura, hotkeys y biblioteca

- [ ] QA-WIN-06 — Probar cada hotkey con dock enfocado, sin foco y minimizado.
- [ ] QA-WIN-07 — Con Replay apagado guardar 30 s: error claro, OBS sigue usable.
- [ ] Encender Replay desde el dock y guardar 15/30/60 s.
- [ ] Verificar archivo legible, duración y contador de sesión.
- [ ] Verificar clip en biblioteca sin reiniciar.
- [ ] Probar búsqueda, favoritos, verticales, pendientes, score y fecha.
- [ ] Reiniciar OBS y comprobar reconstrucción SQLite.

## Ajustes y rutas

- [ ] QA-WIN-08 — Cambiar ajustes de cada sección, reiniciar y comparar valores.
- [ ] QA-WIN-09 — Probar carpeta inexistente, sin permiso y ruta demasiado larga.
- [ ] Confirmar error visible y que el último ajuste válido permanece.
- [ ] Verificar cambio de rutas sin reiniciar cuando corresponda.

## Pro y red

- [ ] QA-WIN-10 — Activar key nueva: Plan Pro, renovación y dispositivo visibles.
- [ ] Activar la misma key en otra VM: error `LICENSE_KEY_ALREADY_USED`.
- [ ] QA-WIN-11 — Cancelar suscripción sandbox, procesar webhook y refrescar.
- [ ] Confirmar gates Pro cerrados y Free todavía funcional.
- [ ] QA-WIN-12 — Cortar red dentro de grace: Pro permitido con estado offline.
- [ ] Avanzar reloj/fixture fuera de grace: vuelve a Free.
- [ ] QA-WIN-13 — Forzar token expirado: refresh correcto, sin duplicar activación.
- [ ] Corromper token cache: no habilita Pro y no crashea.

## Vertical, export y triggers

- [ ] QA-WIN-14 — Exportar 1080x1920 y 720x1280; validar con ffprobe.
- [ ] Exportar Horizontal, Vertical y Ambos; probar cancelar y colisión de nombre.
- [ ] Confirmar progreso y que OBS sigue respondiendo.
- [ ] QA-WIN-15 — Enviar señales repetidas dentro de cooldown; un solo evento/clip.
- [ ] Probar Voice, Audio Spike, Chat Pulse y Scene con licencia válida.
- [ ] Free muestra función bloqueada, pero visible y con explicación.

## Fault injection

- [ ] QA-WIN-16 — Con backup, reemplazar DB por bytes inválidos y abrir OBS.
- [ ] Confirmar `.corrupt-<timestamp>`, DB nueva funcional y aviso en log.
- [ ] QA-WIN-17 — Bloquear DNS/API durante activate, refresh y AI.
- [ ] Confirmar timeout entendible, UI usable y ausencia de crash.
- [ ] QA-WIN-18 — Quitar permisos a carpeta de clips/exports.
- [ ] Confirmar que no se pierde configuración válida ni se sobrescriben archivos.
- [ ] Llenar disco de prueba: error controlado y clip original intacto.

## Packaging, upgrade y uninstall

- [ ] QA-WIN-19 — Instalar en VM limpia; binario, locales y dock presentes.
- [ ] QA-WIN-20 — Actualizar desde la versión anterior; DB/settings migran.
- [ ] Crear clips y registrar sus hashes antes de desinstalar.
- [ ] QA-WIN-21 — Desinstalar; clips/exports/thumbnails/DB conservan hashes.
- [ ] Confirmar que binarios y entradas propiedad del plugin sí se eliminan.
- [ ] Reinstalar y comprobar que la biblioteca conservada se reconstruye.

## Estabilidad y cierre

- [ ] QA-WIN-22 — Stream/record de 30 min con captura, tabs y exports simultáneos.
- [ ] QA-WIN-23 — Soak 2 h: CPU idle <1% y crecimiento memoria <25 MB.
- [ ] QA-WIN-24 — Ejecutar 20 ciclos abrir/cerrar OBS; cero crash/hang.
- [ ] Revisar logs: sin tokens, keys, transcript completo ni rutas sensibles.

## Resultado

- P0 PASS: ____ / ____
- P1 PASS: ____ / ____
- Bugs creados: ____________________
- Evidencia adjunta: ____________________
- Decisión QA: `APPROVE / REJECT`
- Firma QA Lead: ____________________
