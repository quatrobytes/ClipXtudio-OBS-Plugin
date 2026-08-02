# Checklist de release — ClipXtudio 0.5.1

## Código y tests

- [x] Release y RelWithDebInfo compilan.
- [x] QA license desactivada.
- [x] HTTP local inseguro desactivado.
- [x] 46/46 tests C++ pasan en ambas configuraciones.
- [x] 89/89 tests Laravel pasan (501 assertions).
- [x] Migraciones desde cero probadas.
- [x] Seeders demo idempotentes y restringidos.
- [x] DLL, modelo Whisper, FFmpeg, locales y clave pública empaquetados.
- [ ] Build final cargado en OBS después de cerrar la instancia actual.
- [ ] Matriz vocal ES/EN sobre hardware real completada.
- [ ] Chat Pulse Twitch/YouTube implementado.

## Backend externo

- [ ] Staging HTTPS desplegado.
- [ ] Stripe test mode real aprobado.
- [ ] Brevo test aprobado.
- [ ] Activate/refresh/revoke desde DLL final aprobado.
- [ ] Backups, workers, scheduler y alertas aprobados.
- [ ] Historial Git auditado para secretos.
- [ ] Cuenta Pro muestra y abre checkout Monthly/Annual desde el backend.
- [ ] Deadline promocional configurado y visible.
- [ ] Responsive landing validado visualmente en desktop/tablet/mobile.

## Distribución

- [x] ZIP portable generado.
- [x] Instalador compilado.
- [x] Checksums SHA-256 generados.
- [ ] Instalador ejecutado con UAC/admin y OBS cerrado.
- [ ] Upgrade desde versión previa validado.
- [ ] Uninstall y preservación de datos validados en máquina limpia.
- [ ] DLL e instalador firmados.
- [ ] Release notes y manifest publicados en producción.

## Decisión

- [x] Beta interna condicionada.
- [ ] Producción pública autorizada.
