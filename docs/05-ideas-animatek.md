# Ideas para el firmware de Animatek

Las ideas de Javier del 2026-11-06, cada una con lo que hay, lo que cuesta y los límites.
Todo **sin probar**. Límites de la máquina en [01-como-funciona.md](01-como-funciona.md).

## Los límites que mandan

- **CPU:** 240 MHz, uno o dos núcleos (el segundo puede fallar en algunas unidades: hay que
  poder vivir con uno). Por dar escala: en Felucca, 8 voces de un motor caro se comen el 54 %
  de la CPU, y uno de analógico virtual, el 17 %.
- **Memoria:** sin RAM externa. Las líneas de retardo y las reverbs largas cuestan; Felucca
  usa retardos de 16 bits y recortes.
- **Flash:** ~572 KB para el firmware; los samples comparten el resto con los ajustes (FM1
  Open: **unos 19,5 s** de samples nuevos en total).
- **Panel:** 27 teclas, 14 botones, 7 encoders y una pantalla de 240 × 240. Todo tiene que
  caber en páginas de 4 knobs.
- **Un G1 entero no cabe**: no hay un DSP56303 ni su RAM. Lo que sí cabe es **reinterpretar**.

## Las ideas

### 1. Port del ATEK303 (303 + secuenciador) — viable, el más rápido
- **Hay:** nuestro ATEK303 (GPL-3.0, `../VCV-Rack/ATEK303`), con el motor ajustado en VCV y
  el secuenciador de 64 pasos. **BB1 y X0X ya llevan un 303 derivado de Open303** en el FM-1,
  con sobremuestreo y en tiempo real: prueba que cabe.
- **Cuesta:** pasar el motor de C++ a C, con funciones rápidas sin libm, y diseñar el
  secuenciador para 27 teclas y 4 knobs.
- **Ojo:** X0X ya es «una groovebox con 303». Lo nuestro tiene que distinguirse: **el
  sonido del ATEK303 afinado y su secuenciador**, no otra 909.

### 2. Mini modular que lee los `.pch` del Nord Modular — viable, y nadie lo tiene
- **Hay:** **Kitten Rack** (GPL-3.0) ya es un modular de 50 módulos con cables dentro del
  FM-1, con editor web. Y en casa tenemos el lector de `.pch`: NME (`nmedit`) y **g1-taller**
  (conversor y validador, 34.038 patches).
- **Cómo:** el `.pch` se convierte **en el ordenador** (en el editor web o con un script de
  g1-taller) a un formato pequeño: cada módulo del G1 se traduce al módulo nuestro más
  parecido (OscA → VCO, FilterE → filtro de 4 polos, ADSR, LFO, mezclador, VCA, secuenciador…)
  y los cables tal cual. El FM-1 lo carga por USB. No suena como el G1: **lo reinterpreta**.
- **Cuesta:** la tabla de traducción de módulos (el G1 tiene unos 128 tipos; empezar por los
  20 más usados, que se sacan de las 34.038 patches de g1-taller) y el motor de módulos.
  Basarse en Kitten Rack (y por tanto GPL) o escribir uno propio más pequeño sobre fm1-base.
- **Límite:** patches de pocas voces y pocos módulos. Hay que avisar al convertir qué se queda fuera.
- **Para el canal:** es el puente perfecto con G1-Emu y NME, y nadie más puede hacerlo.

### 3. Generativo Animatek (Nord Modular + VCV) — viable
- **Hay:** AMB-1 enseña cómo (loops primos, euclidianos, acorde que deriva, cola de comandos).
  Y nuestro UZZ de VCV es un secuenciador generativo.
- **Idea:** **UZZ dentro del FM-1 con un FM pequeño**: la lógica del UZZ portada a C,
  tocando dos o cuatro operadores FM, con escalas y euclidianos, y las teclas para meter y
  quitar notas como en AMB-1.
- **Ojo:** el generativo ambient ya está muy servido (AMB-1, Bunker Evolved, Tessera…). Lo
  nuestro tiene que sonar a Animatek: más techno o modular que ambient de aeropuerto.

### 4. West Coast con un loop pequeño — viable y barato en CPU
- Oscilador complejo (FM y AM entre dos osciladores), **wavefolder** y **low pass gate**
  (Kitten Rack ya tiene una LPG estilo Buchla), más un loop corto o un secuenciador de 5 pasos
  estilo Buchla. Muy pocas operaciones por muestra: caben varias voces.

### 5. Sampler y caja de ritmos — viable con límites
- Pocos segundos de sample (**~20 s en total**). Pide síntesis de percusión más que samples
  largos, o samples muy cortos y de 8 o 12 bits.
- *Por aclarar:* ¿«el Massive de Reaktor» es el sampler y caja de ritmos de NI que tenías en
  mente? Dime el nombre exacto y miro cómo era para acercarnos.

### 6. Una plataforma «tipo VCV» para el FM-1 — viable, y medio hecha ya
La idea de Javier (06/11): patchear en el ordenador, como en VCV, y mandar el patch al FM-1,
que lo carga y lo toca. Como el **MetaModule de 4ms**, que carga en hardware patches hechos
en VCV Rack con módulos portados, pero en un aparato de 70 €.

**Lo importante: Kitten Rack ya funciona así.** Su web, **Matryoshka** (<https://matryoshka.lol>,
el editor de Kitten Rack), es un modular en el navegador, y el firmware es su mismo motor
portado a C en coma fija (`docs/ARCHITECTURE.md` de `terceros/kitten-rack`):

- El motor de referencia está en TypeScript (`web/src/engine.ts`, `modules.ts`). Los módulos
  son **datos** (`ModuleDef`), compartidos por el editor y el firmware; el firmware es el port
  en coma fija (`firmware/src/engine.c`), y unos tests comparan que los dos suenen igual.
- **El envío al FM-1 es SysEx por USB-MIDI**: HELLO, el patch en trozos de 384 bytes con su
  confirmación, COMMIT con CRC-16, STORE para guardarlo, y PARAM para mover parámetros en
  directo. **64 huecos** de patches en la flash; program change los cambia.
- Coste medido en el FM-1: un patch de 303 con 8 módulos y un delay usa el **11 % de la CPU**.
- Los patches se comparten por enlace (`#p=…`).

**Cómo encaja lo nuestro:**
- **Módulos nuevos que entienden los `.pch`** (la idea 2): en vez de un motor desde cero,
  añadimos a ese motor los módulos que faltan para traducir los del G1, y el conversor de
  `.pch` (con g1-taller) genera el patch en su formato. Primero los módulos de utilidad:
  **sample & hold, track & hold, LFO, secuenciadores, euclidiano, cuantizador, lógica,
  contadores** (los del G1 y los de G1-Mods).
- **«Dile qué quieres y lo manda»**, como con g1-taller: escribes «un sinte generativo
  euclidiano en Re dórico» y un modelo genera el patch, que se envía al FM-1. g1-taller ya
  hace esto para el G1 con un modelo local: sería la misma idea con otro formato de salida.
- **Por dónde mandarlo:** USB-MIDI es lo seguro. Bluetooth MIDI existe (fm1-base lo da, y
  Hortator tiene versión Bluetooth), pero es más lento para mandar patches.
- **Licencia:** Kitten Rack es GPL-3.0. Si partimos de él, lo nuestro también lo será, y
  conviene **hablar con Black Kitten Devices**: contribuir módulos a su proyecto o hacer un
  fork con permiso. Todos ganan más que con dos plataformas que compiten.
- **Límite:** no caben «todos los módulos de VCV»: cada módulo hay que portarlo a mano, a coma
  fija, y medir su CPU. Unas decenas de módulos bien elegidos, sí.

### 7. Caja de ritmos acid: el ATEK303 con más voces — sin probar
La idea de Javier (06/11, con el ATEK303 ya sonando en el FM-1 real): una página **ALGO** al tocar
el encoder ALGORITHM, con los dos osciladores del 303 (SAW, SQR) y los modelos de **Plaits**
(el plan aparcado está en `atek303/README.md`), y con más motores llegar a **4 pistas**:
percusión con los bombos, cajas y charles de Plaits y otras voces, como una caja de ritmos.

- **Ya existe mucho:** X0X (909, 808 y dos 303), SLOOP (4 pistas), Skiff (6 pistas, 14 motores),
  Bubba Machine (los 808 y 909 de Plaits). Otra groovebox más no destaca por sí sola.
- **Lo que sería nuestro:** el **generador acid** del ATEK303 en la pista del 303 y patrones
  **generativos** en las de percusión (euclidiano, mutaciones con LOCK, como en G1-Mods). Pulsas
  y suena una línea entera que se mueve.
- **CPU, por medir:** la voz del 303 usa el 42 % en el emulador (a 240 MHz; el FM-1 va a 360).
  Con tres pistas más puede no caber sin rebajar el sobremuestreo o los efectos. Lo primero
  sería medir los modelos de percusión de Plaits en el emulador.
- **Decidido (06/11):** el ATEK303 se queda como está, como nuestro firmware base. La caja de ritmos,
  si se hace, será un firmware aparte que parta de su código: la pista acid igual y tres de percusión.

## Recomendación

1. **Primero, el port del ATEK303 sobre fm1-base.** Es el más corto, el motor es nuestro y ya
   sabemos que cabe. Sirve para aprender la plataforma (pantalla, teclas, flash, MIDI, modo
   host, emulador) y da un vídeo pronto.
2. **Después, el gordo: el modular que lee `.pch`**, partiendo del motor de Kitten Rack
   (ideas 2 y 6): módulos de utilidad nuevos, el conversor de `.pch` con g1-taller y el envío
   desde el ordenador. Antes de escribir una línea, hablar con sus autores. Es lo que nos
   diferencia de los 154 firmwares que hay.
3. El generativo y el West Coast pueden ser **motores o patches dentro del modular**, en vez
   de firmwares aparte.

La base, **fm1-base** (MPL-2.0: elegimos licencia). Si tomamos código GPL (el ATEK303 lo es,
Kitten Rack también), el firmware será GPL-3.0, que encaja con lo de abrir el software y
dejar lo exclusivo de Patreon para los contenidos.
