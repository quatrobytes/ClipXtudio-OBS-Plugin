# ClipXtudio — Sistema de diseño Qt Widgets

**Versión:** 1.0  
**Estado:** Implementado en el scaffold nativo  
**Referencia:** conceptos visuales de `imagenes-demo/`

---

## 1. Dirección visual

ClipXtudio usa una interfaz oscura integrada con OBS, con profundidad
moderada, superficies compactas y un acento morado reservado para navegación y
acciones principales.

Principios:

- Oscuro, pero no negro plano.
- Jerarquía por contraste, tamaño y spacing; no por exceso de bordes.
- Morado para selección y acción primaria.
- Verde únicamente para estados positivos/activos.
- Cards consistentes con un solo sistema de radios y borders.
- Una acción primaria dominante por bloque.
- Sin gradientes decorativos, sombras pesadas ni chrome de aplicación externa.
- Qt Widgets nativo; no se usa WebView.

---

## 2. Tokens

Fuente única: `include/clipcoach/ui/design-tokens.hpp` y
`src/ui/design-tokens.cpp`.

### 2.1 Color

| Token | Valor | Uso |
|---|---|---|
| `kBackground` | `#0C1016` | Fondo del dock y páginas |
| `kSurface` | `#121821` | Estados vacíos y superficies base |
| `kSurfaceRaised` | `#171E29` | Cards y secciones |
| `kSurfaceHover` | `#202938` | Hover de cards/controles |
| `kBorder` | `#283345` | Divisores y bordes |
| `kAccent` | `#7C3AED` | Acción primaria y selección |
| `kAccentHover` | `#8B5CF6` | Hover/indicador activo |
| `kAccentPressed` | `#6D28D9` | Acción presionada |
| `kSuccess` | `#22C55E` | Estado activo/score positivo |
| `kSuccessSurface` | `#12351F` | Fondo de badge activo |
| `kWarning` | `#F59E0B` | Advertencias futuras |
| `kTextPrimary` | `#F4F7FB` | Títulos y valores |
| `kTextSecondary` | `#A4AEBD` | Descripciones |
| `kTextMuted` | `#6F7B8C` | Estados deshabilitados |

Los colores no se definen en widgets individuales. El QSS central usa estos
valores para todos los componentes.

### 2.2 Spacing

Escala lógica:

| Token | px lógicos |
|---|---:|
| `kSpaceXs` | 4 |
| `kSpaceSm` | 8 |
| `kSpaceMd` | 12 |
| `kSpaceLg` | 16 |
| `kSpaceXl` | 24 |

Reglas:

- Interior de card: 12–16.
- Separación entre cards: 8–12.
- Margen de página: 16.
- Bloques grandes: 24 únicamente cuando la densidad lo permite.

### 2.3 Radios

| Token | px lógicos | Uso |
|---|---:|---|
| `kRadiusSm` | 6 | Badges y thumbnails |
| `kRadiusMd` | 9 | Cards y botones |
| `kRadiusLg` | 12 | Banners/estados vacíos |

### 2.4 Controles e iconos

- Control secundario: mínimo 34 px.
- Control primario: mínimo 42 px.
- Icono estándar: 16×16 px lógicos.
- Ancho mínimo del dock: 360 px.
- Alto mínimo del dock: 480 px.

Qt6 trabaja en píxeles lógicos; no se fijan escalas basadas en DPI físico.

---

## 3. Tipografía

Se conserva la familia tipográfica configurada por OBS/sistema para integrarse
con el host.

Jerarquía:

| Rol | Tamaño | Peso |
|---|---:|---:|
| Marca | 15 px | 700 |
| Título de página | 17 px | 700 |
| Texto base | 13 px | normal |
| Título de sección/card | 12–13 px | 600–700 |
| Label de estado/tab | 10–11 px | 600–800 |

No se usan mayúsculas generadas en código. Cuando una etiqueta se presenta en
mayúsculas, el texto traducido ya define esa forma.

---

## 4. Estructura del dock

```text
MainDock
├─ DockHeader
│  ├─ BrandMark
│  ├─ BrandName
│  └─ ProBadge
├─ Status summary
│  ├─ Replay Buffer
│  ├─ Vertical Canvas
│  └─ Plan
├─ TabBar
└─ QStackedWidget
   ├─ Capturar
   ├─ Vertical
   ├─ Triggers
   ├─ Clips
   └─ Ajustes
```

Las páginas de contenido estático viven dentro de `QScrollArea`. Clips mantiene
cabecera, controles y resumen visibles, y usa un `QScrollArea` interno solo para
la lista. El ancho no genera scroll horizontal.

---

## 5. Tabs

Orden obligatorio:

1. Capturar
2. Vertical
3. Triggers
4. Clips
5. Ajustes

Comportamiento:

- `TabBar` usa `documentMode`.
- Las cinco tabs se expanden al ancho disponible.
- El texto puede elidirse y conserva tooltip completo.
- Tab inactiva: texto secundario.
- Hover: superficie sutil.
- Tab activa: texto morado claro e indicador inferior de 2 px.
- Iconografía: `QStyle::StandardPixmap`, para nitidez vectorial/temática según Qt.
- `QStackedWidget` mantiene el mismo fondo y márgenes en todas las páginas.

---

## 6. Componentes

Todos los componentes viven en:

```text
include/clipcoach/ui/components/
src/ui/components/
```

### 6.1 StatusCard

Card compacta de resumen.

- Título pequeño secundario.
- Valor principal o `StatusPill`.
- Pill verde solo para estado activo.
- Hover sutil.
- No ejecuta lógica de estado OBS.

### 6.2 PrimaryButton

- Fondo morado.
- Texto blanco y peso 700.
- Alto mínimo 42 px.
- Estados hover, pressed y disabled.
- Propiedad `controlRole=primary` permite cambiar `objectName` para tests sin
  perder styling.

### 6.3 SecondaryButton

- Fondo raised oscuro.
- Border visible.
- Hover con border morado.
- Alto mínimo 34 px.

### 6.4 ToggleRow

- Título, descripción y `QCheckBox` nativo.
- Toda descripción puede ocupar varias líneas.
- Accesible por teclado.
- El título alimenta el nombre accesible del checkbox.

### 6.5 ClipCard

- Thumbnail 104×62 con placeholder y duración si no existe imagen.
- Título, fecha/hora, trigger y `ScoreBadge`.
- Favorito con estado checked.
- Preview, Exportar 9:16, Caption, Subtítulos y Abrir carpeta en dos filas.
- Caption y Subtítulos se deshabilitan si falta el recurso asociado.
- Emite intenciones al controlador; no consulta SQLite ni abre archivos.

### 6.6 ScoreBadge

- Score restringido a 0–100.
- Fondo success oscuro y texto verde.
- Tamaño basado en contenido.

### 6.7 SettingsSection

- Card contenedora con título.
- Expone un `QVBoxLayout` para composición.
- Mantiene padding y spacing uniformes.

### 6.8 TabBar

- Encapsula configuración visual y comportamiento común.
- Tooltips automáticos.
- Icon size centralizado.

### 6.9 EmptyState

- Superficie oscura con border dashed.
- Título centrado y descripción.
- Alto mínimo para evitar una pantalla visualmente colapsada.

### 6.10 ProBadge

- Fondo morado oscuro.
- Texto morado claro.
- Compacto y no interactivo.

### 6.11 UpgradeBanner

- Card de énfasis Pro.
- `ProBadge`, título, descripción y `PrimaryButton`.
- Se usa al final de Capturar y Triggers.
- Nunca reemplaza la acción principal de una página.

---

## 7. Descripción textual de capturas esperadas

### 7.1 Capturar

El dock aparece a la derecha de OBS. Arriba se ve la marca ClipXtudio y

### Checks y formularios de Ajustes

- El indicador de `QCheckBox` es un cuadrado de 18 × 18 px con radio de 4 px.
- El estado desmarcado es neutro; el morado se reserva para el estado marcado.
- Cada campo vive en una fila compacta con fondo propio, padding consistente y
  control alineado a la derecha.
- Las secciones se centran y no superan 920 px para evitar controles dispersos
  en docks anchos.

### Vertical

- La pestaña completa usa scroll vertical `AsNeeded`, sin scroll horizontal.
- Antes de los controles se explica el flujo: modo de salida, resolución,
  plantilla, capas, preview y persistencia automática.
- `Canvas Settings` separa configuración general y capas visibles mediante
  títulos y textos de apoyo localizados.
- El formulario permite envolver filas largas para evitar superposición cuando
  el dock pierde altura o anchura.
un badge PRO compacto. Debajo hay tres cards: Replay Buffer activo en verde,
resolución vertical y plan Free. La tab Capturar está subrayada en morado.

La página muestra “Captura rápida”, cuatro botones compactos de duración, un
botón morado de ancho completo para marcar momento, un botón secundario para
guardar vertical y el banner Pro al pie.

### 7.2 Vertical

La tab Vertical queda activa con indicador morado. La página presenta cards
para resolución, plantilla y fuente. El botón “Abrir diseñador vertical” es la
acción dominante; guardar 60 segundos es secundario. El canvas real no se
simula hasta que exista backend.

### 7.3 Triggers

Una `SettingsSection` agrupa trigger de voz, pico de audio y escena. Cada fila
tiene título, explicación y toggle alineado a la derecha. Solo el trigger de
audio aparece activo en el estado inicial conceptual. El banner Pro cierra la
página.

### 7.4 Clips

La cabecera mantiene los mismos márgenes. Tres cards resumen clips, score y
duración. Debajo aparecen filtros, búsqueda y orden; los resultados usan
`ClipCard` completas. Cuando SQLite no devuelve datos se muestra `EmptyState`.
El panel inferior presenta el resumen de sesión y el listado se crea por lotes.

### 7.5 Ajustes

Dos cards separan General y Rendimiento. Las opciones usan `ToggleRow`; no hay
formularios sin implementar. El fondo, radios, border y tipografía coinciden
con las demás tabs.

---

## 8. Alto DPI y adaptabilidad

- Qt6 habilita high-DPI por defecto.
- Solo se usan tamaños lógicos y `QStyle` icons.
- No hay imágenes bitmap en la UI base.
- Layouts usan stretch y size policies, no posiciones absolutas.
- Cada página permite scroll vertical.
- Los textos descriptivos usan word wrap.
- Tabs usan elide y tooltip.
- El dock puede flotar o acoplarse sin cambiar su sistema visual.

---

## 9. Localización

Fuente:

```text
include/clipcoach/ui/ui-strings.hpp
data/locale/en-US.ini
data/locale/es-ES.ini
```

Reglas:

- `MainDock` recibe `TranslationFunction`.
- El plugin inyecta `obs_module_text`.
- Los tests inyectan un traductor determinista.
- No se hardcodean textos principales dentro de los builders de página.
- Inglés y español contienen todas las claves de
  `kRequiredTranslationKeys`.
- El test `ui.localization-keys` falla si falta una clave en cualquiera de los
  dos idiomas.

Valores técnicos como `1080×1920` no se traducen.

---

## 10. Accesibilidad

- Botones y tabs admiten foco de teclado.
- Controles conservan semántica Qt nativa.
- Los toggles tienen accessible name.
- Los estados no dependen solo del color: incluyen texto.
- Contraste de texto principal/secundario diseñado sobre fondos oscuros.
- La jerarquía usa peso y tamaño además de color.

---

## 11. Reglas de contribución

- No añadir colores inline a componentes.
- No crear botones genéricos cuando corresponde `PrimaryButton` o
  `SecondaryButton`.
- No crear una sexta tab sin decisión de producto.
- No guardar lógica de OBS, storage o network dentro de widgets.
- Todo texto nuevo requiere constante y valores `en-US`/`es-ES`.
- Todo componente nuevo requiere test básico de construcción.
- Todo estado interactivo requiere keyboard focus y disabled state.
- No usar WebView para construir la UI principal.

---

## 12. Extensión implementada: biblioteca de clips

La pestaña Clips usa los mismos tokens, radios y jerarquía que Capturar.

- Tres `StatusCard` muestran clips totales, mejor score y duración de sesión.
- Los filtros son chips checkables con acento morado en selección.
- `QLineEdit` y `QComboBox` comparten superficie, borde y foco morado.
- `ClipCard` usa thumbnail 104×62, duración, título, fecha/hora, trigger,
  `ScoreBadge`, favorito y acciones compactas en dos filas.
- Caption y Subtítulos presentan estado disabled cuando falta el recurso.
- El panel inferior de resumen usa una superficie base para distinguirse de las
  cards interactivas.
- A 360 px no existe scroll horizontal; a 520 px las acciones conservan
  legibilidad.
- El listado se renderiza en lotes de 20 para devolver control al event loop.

Captura esperada: sobre el fondo `#0C1016`, la métrica superior forma una fila
de tres cards; debajo aparecen cuatro filtros, búsqueda y orden. Cada clip usa
una card elevada con thumbnail a la izquierda, contenido al centro, favorito y
score a la derecha, y acciones debajo. El resumen queda al final del layout.

---

## 13. Extensión implementada: Ajustes

- La página usa un `QScrollArea` único; versión, actualización y créditos ya no
  ocupan una sección desplazable.
- Cada sección conserva superficie raised, borde de 1 px, radio de 9 px y
  padding de 12–16 px.
- Los formularios combinan label secundario con control nativo alineado a la
  derecha.
- `QSpinBox`, `QComboBox`, `QLineEdit` y `QKeySequenceEdit` comparten fondo,
  borde y foco morado.
- Los toggles conservan el patrón visual del resto del dock.
- Rutas usan campo read-only más botones compactos Cambiar/Abrir.
- Integraciones usan badges ámbar para estados no conectados.
- Cuenta Pro usa badge morado y controles disabled explícitos.
- Un error de persistencia aparece dentro de la página, nunca como diálogo
  modal durante una modificación ordinaria.

Captura esperada: General y Captura aparecen primero; las secciones restantes
continúan verticalmente con scroll. No hay navegación lateral que reduzca el
ancho útil del dock. A 520 px los labels y controles mantienen una columna
estable; a 360 px los campos pueden comprimirse sin scroll horizontal.

### Footer global fijo

- Debajo del stack de tabs existe `GlobalFooter`; nunca se desplaza con el
  contenido.
- Usa una sola fila horizontal sin wrapping. Las acciones `Replay Buffer`,
  `Guardar clip` y `Buscar actualizaciones` permanecen agrupadas a la izquierda;
  estado, versión y crédito ocupan el espacio restante hacia la derecha.
- Muestra `ClipXtudio v<semver>` usando la versión compilada.
- `Buscar actualizaciones` muestra estados consultando, actualizado,
  actualización disponible y error sin bloquear la interfaz.
- `Descargar actualización` permanece oculto hasta recibir una versión
  posterior y una URL HTTPS válida.
- Incluye acciones globales para iniciar/detener Replay Buffer y sacar un clip
  con la duración seleccionada.
- El crédito visible y enlazable es
  `Desarrollado por QuatroBytes.com · by MrJimeneX`.
- La comprobación es manual y nunca instala ni reemplaza el DLL de forma
  silenciosa.
# Header y footer global — 0.4.2

- Header fijo: icono, `ClipXtudio - v<SemVer>`, badge de estado y badge PRO.
- El badge de estado usa superficie verde para operaciones correctas y roja
  para errores; el mensaje completo permanece en tooltip.
- Footer fijo de una sola línea: Replay Buffer, Sacar clip, Buscar
  actualizaciones y crédito de desarrollo.
- Nombre y versión no se duplican en el footer.
- Todos los botones visibles del footer usan `kControlHeight = 36`.

## Confirmación transitoria de clip — 0.4.3

- Al terminar de guardar, la confirmación aparece como una píldora verde
  compacta alineada a la derecha del contenido de Capturar.
- La altura máxima es 24 px y el texto termina con un check `✓`.
- La confirmación se oculta automáticamente después de 3.5 segundos.
- El badge del header confirma brevemente el guardado y vuelve a `Ready/Listo`
  en el mismo intervalo.
- Los estados en curso y los errores no se auto-ocultan: permanecen visibles
  hasta que exista un resultado o una nueva acción.

## Biblioteca simplificada — 0.4.4

- Las tres métricas de sesión ocupan el lado izquierdo del resumen superior.
- Todos, Favoritos, Verticales y Pendientes forman una matriz compacta 2x2 a la
  derecha. Pendientes explica mediante tooltip que el export aún no terminó.
- Se eliminó la búsqueda por título para reducir ruido visual.
- Ordenar por fecha o mejor score permanece inmediatamente encima de la lista.
## ClipCard compacta 0.4.5

Cada ClipCard ocupa una sola fila: selección, miniatura, título/fecha/trigger,
cinco acciones compactas, favorito y score. Preview, copia 9:16, Caption,
Subtítulos y Abrir carpeta mantienen la misma altura y usan iconos o
abreviaturas con tooltip y nombre accesible completo.
## Responsive dock behavior (0.5.1)

- Clips keeps the three session indicators in one horizontal row.
- At 720 px or less, the four library filters wrap into a 2 x 2 grid and the
  export controls use three rows. Controls must never overlap or render as
  collapsed lines.
- Above 720 px, filters and export actions return to a single horizontal row.
- Vertical uses a side-by-side preview/editor at wide widths. Below 720 px the
  9:16 preview stacks above the complete editor inside the existing scroll area.
- Settings stacks each label above its control below 620 px and displays a
  localized hint recommending that the user widen or undock ClipXtudio.
## Compact footer and visible metrics (0.5.2)

- Clip Library summary cards have a 58 px minimum height and always use one
  horizontal row. They may share the overview row with filters at wide widths,
  but they must never collapse into decorative lines.
- Fixed-footer command buttons use the same 32 px height, 10 px vertical
  container margins and explicit vertical-center alignment.
