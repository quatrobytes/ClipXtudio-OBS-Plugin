# Contribuir a ClipXtudio

Gracias por ayudar a mejorar el plugin nativo de ClipXtudio para OBS Studio.

## Flujo

1. Abre un issue para errores reproducibles o propuestas concretas.
2. Crea una rama desde `main`.
3. Mantén los cambios limitados al plugin público.
4. Añade o actualiza pruebas.
5. Ejecuta formato, build y CTest antes del pull request.
6. Explica impacto funcional, riesgos y validación realizada.

## Requisitos

- C++17 y convenciones existentes del proyecto.
- Ningún secreto, token, licencia de usuario o dato personal.
- Ninguna clave de Stripe, proveedor de IA o firma digital.
- No desactivar TLS, autorización, validación de firmas ni feature gates.
- No incluir implementaciones copiadas del backend privado.
- Las nuevas dependencias necesitan justificación y auditoría de licencia.

## Pruebas locales

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo --parallel
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File tests/packaging/Test-ReleaseMetadata.ps1
```

## Licencia de contribuciones

Al enviar una contribución declaras que tienes derecho a aportarla y aceptas
que se distribuya bajo GNU GPL v2.0 o posterior. No transfieres derechos sobre
las marcas ClipXtudio o QuatroBytes.
