# Componentes de terceros

ClipXtudio utiliza componentes que conservan sus licencias originales.

## OBS Studio y libobs

- Proyecto: https://github.com/obsproject/obs-studio
- Versión fijada: consulta `buildspec.json`
- Licencia: GNU GPL v2.0 o posterior

## Qt

- Proyecto: https://www.qt.io/
- Distribución: dependencias oficiales fijadas por OBS
- Licencia: según los módulos y paquetes distribuidos por OBS

## whisper.cpp y modelo Whisper

- Runtime: https://github.com/ggml-org/whisper.cpp
- Modelo: https://huggingface.co/ggerganov/whisper.cpp
- Procedencia y checksum: `data/models/README.txt`

## FFmpeg

- Proyecto: https://ffmpeg.org/
- Build distribuido: Gyan.dev Essentials
- Licencia del build incluido: GNU GPL v3
- Fuente, configuración y checksums:
  `data/tools/ffmpeg/README.txt` y
  `data/tools/ffmpeg/CLIPXTUDIO-NOTICE.txt`

Los archivos de licencia incluidos con cada componente no deben eliminarse de
los paquetes redistribuidos. Antes de actualizar una dependencia se debe
repetir la auditoría de licencia y checksum.
