# Score Engine

## Objetivo

`ScoreEngine` ordena candidatos con un resultado determinista de 0 a 100. No
autoriza triggers ni ejecuta capturas.

## Entradas

- tipo de trigger;
- intensidad de audio;
- actividad de chat;
- fuerza de keyword;
- confianza de voz;
- marcador manual;
- relevancia de escena;
- confianza del futuro hook de IA;
- calidad de duración;
- cantidad de tipos de señal independientes.

Los valores externos se normalizan internamente a `[0, 1]`.

## Modelo v1

Cada tipo aporta una base. Las señales agregan: audio 14, chat 12, keyword 15,
voz 10, escena 8, IA 12, marcador manual 25 y calidad de duración 8. Para un
momento combinado se toma la señal más fuerte, se suma el 16 % de las señales
de soporte y un bonus de seis puntos por tipo adicional, limitado a 18. El
resultado se redondea y limita a `[0, 100]`.

La calidad de duración favorece clips de aproximadamente 30 a 60 segundos,
penaliza clips demasiado cortos y degrada gradualmente los demasiado largos.

## Propiedades y límites

- Determinista y explicable; no usa un modelo remoto.
- Valores fuera de rango no rompen el límite 0–100.
- Los pesos son heurísticos y aún no se personalizan por contenido.
- `FutureAiHook` consume una confianza generada por un adaptador futuro.
- El score no actualiza SQLite hasta que un orquestador acepte el candidato.

## Criterios de aceptación medibles

- Ningún cálculo retorna menos de 0 ni más de 100.
- Un marcador manual válido obtiene al menos 70 con duración óptima.
- Mayor intensidad produce mayor score con el resto constante.
- Tres señales fuertes combinadas superan a la señal aislada.
- Una duración de 30 segundos obtiene calidad 1.0.

