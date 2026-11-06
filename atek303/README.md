# ATEK303 para el FM-1

La voz del **ATEK303** de VCV Rack (`../../VCV-Rack/ATEK303`) como firmware del M-VAVE FM-1,
sobre **fm1-base**. Primer paso del plan (ver `../docs/05-ideas-animatek.md`): aprender la
plataforma con un motor que ya es nuestro. Licencia GPL-3.0-or-later, como el ATEK303.

## Estado (2026-11-06)

- [x] **Motor portado a C** (`app/dsp/atek_voice.c`): Open303 con nuestros cambios, el oscilador
      ATEK y el filtro de diodos ATEK, en coma flotante simple y sin libm. El filtro elíptico de
      12.º orden de Open303 va como seis biquads, que el `float` sí aguanta.
- [x] **Comprobado contra el original**: −51,5 dB de diferencia y espectros a 0,03 dB (`tests/`).
- [x] **App** (`app/app.c`): teclas mono (legato = slide, pulsación fuerte = acento), OCT−/OCT+,
      K1–K4 en dos páginas (SELECT), SAVE, **PLAY = una línea acid de demostración** y MIDI de
      entrada (notas, CC 71 y 74–79).
- [x] **Compilado**: el toolchain de JieLi está en `~/.jieli` y el SDK AC79 en `~/fw-AC79_AIoT_SDK`.
      El firmware ocupa 100 KB, sin doble precisión emulada.
- [x] **Simulador host**: `app/tests/basic.txt` pasa 15 de 15 comprobaciones (teclas, slide,
      acento, knobs, páginas, octava, guardar y reiniciar, MIDI).
- [x] **Emulador**: suena. Con la demo, el audio usa el **42 % de un núcleo** (AMB-1, un 23 %), y
      el emulador lo corre a **3,6 veces el tiempo real** (AMB-1 a 3,3; X0X a 3,0).
- [x] **Plugin CLAP** recompilado e instalado en `~/.clap/FM-1.clap`. El firmware para cargarlo
      está en `../firmwares/propios/atek303.fwsc`.
- [x] Probado en Bitwig por Javier (fase 1): «suena genial».
- [x] **Fase 2, el secuenciador** (ver abajo).
- [x] Javier confirma que DENSITY y compañía en tiempo real se quedan así («es más divertido»).
- [x] **Fase 3: efectos y editor de pasos** (ver abajo).
- [ ] Probar las fases 2 y 3 en Bitwig.

### Arreglo en el emulador (local, en `terceros/fm1-emu`)

La capa de hardware de fm1-base (felucca-bsp) del 06/11 comprueba que el PLL de audio tenga los
valores con los que lo deja el arranque de M-VAVE antes de encender el sonido
(`fm1_audio_init_checked`, en `bsp/boards/fm1/fm1_audio.h`). El emulador no los imitaba (leía 0),
así que **cualquier firmware compilado hoy sobre fm1-base salía mudo en el emulador** (la
plantilla y AMB-1 compilados aquí también), aunque en un FM-1 real suene. Se arregló en
`crates/fm1-soc/src/lib.rs` (región Ana: `0x119A0` y `0x119A4` devuelven los valores de fábrica
mientras nadie los escriba). Es un cambio local en código de otros: conviene mandárselo a Joakim
Eriksson como issue o PR. Si se vuelve a clonar el emulador, hay que aplicarlo otra vez.

### Notas del banco de pruebas

- `fm1-emu` (sin JIT) acepta notas MIDI con `--keys "T:midi:90247f"`, y así se midió la CPU.
- En `fm1-live --bench`, la entrada por guion (MIDI, botones) no le llegó a ningún firmware, ni a
  X0X: para medir se compiló una variante con la demo arrancando sola.

## Fase 2: el secuenciador (2026-11-06)

El generador de patrones del ATEK303 SEQ (`src/AcidPattern.hpp` y lo que usa de `AcidGen.hpp`),
portado a C en `app/dsp/acid_pattern.c`. Es el mismo algoritmo, con los mismos flujos de azar:
**una semilla da exactamente el mismo patrón en VCV y en el FM-1**. Comprobado con
`tests/pattern_golden.cpp` (el C++ original) contra `tests/pattern_port.c`: 3.840 patrones (6
ajustes × 8 escalas × 2 longitudes × 40 semillas), cada uno con 6 mutaciones y su rejilla, salen
idénticos byte a byte (30.720 líneas).

El paso, el gate, el tie y el slide siguen `Atek303Seq::process` con la voz pegada (el slide lo
hace la voz). La resolución es de 32 muestras (0,7 ms).

**Controles**

| | |
| --- | --- |
| PLAY | arranca o para (reloj interno; con reloj MIDI, lo sigue: start, stop, continue, 6 ticks por paso) |
| SEQ | GENERATE: semilla nueva. Con LOCK, muta las tres capas |
| ENV / LFO / EDIT | muta TIEMPO / NOTAS (octavas) / SLIDES y ACENTOS |
| HOME | directo a la página 1 (CUTOFF), y sale del editor |
| REC | deshace la última mutación o edición |
| GLO | LOCK |
| teclas | parado: tocan la voz. Sonando: **transportan** (la tecla es la nueva raíz, como en un 303) |
| SELECT | páginas: 1 y 2 la voz (la 2 lleva GATE, la duración de las notas del secuenciador), 3 DENSITY, RANGE, ACCENTS, SLIDES, 4 STEPS, SCALE, TEMPO, ROOT |
| SAVE | guarda knobs, patrón, semilla, mutaciones, tempo, raíz y octava |

Abajo, siempre, una tira de 16 pasos con los colores de los LEDs del módulo (verde nota, azul tie,
ámbar slide, rojo acento) y el paso en curso enmarcado.

**Diferencia a propósito con VCV:** en el módulo, DENSITY, RANGE, ACCENTS y SLIDES solo cuentan en
el siguiente GENERATE. Aquí, al girarlos, **el patrón vuelve a crecer desde la misma semilla**
(el algoritmo v4 está hecho para eso: subir la densidad solo añade notas). Se pierden las
mutaciones hechas. Si no convence, se vuelve al comportamiento de VCV en una línea.

**Comprobado:** el simulador host pasa 25 de 25 (teclas, PLAY, avance de pasos, GENERATE, mutar,
deshacer, LOCK, transportar, tempo, guardar y reiniciar). En el emulador suena con PLAY y con reloj
MIDI externo (arranca con start, avanza con los ticks y para con stop). Firmware de 110 KB y CPU como
en la fase 1 (el secuenciador apenas pesa).

**Límite conocido:** el MIDI se lee una vez por bloque de audio (5,8 ms), así que con reloj externo
el paso puede llegar hasta 5,8 ms tarde. Si se nota, se reparte la lectura dentro del bloque.

## Fase 3: efectos y editor de pasos (2026-11-06)

**Efectos** (`app/dsp/fx.c`), la cadena clásica del acid, de mono a estéreo: **DRIVE** (OVERDRIVE,
RAT o FUZZ, con antialias en el clipper, TONE y LEVEL) → **MOD** (CHORUS o FLANGER) → **DELAY**
ping-pong sincronizado al tempo (1/16 .. 1/4.; cada repetición más oscura; al girar TIME dobla el tono
como una cinta) → **REVERB** (la placa de Dattorro, con el tanque modulado y pre-delay). Las páginas,
en el orden que pidió Javier: 5 DELAY, 6 CHORUS, 7 DRIVE, 8 REVERB (el sonido sigue el orden de arriba);
el botón FX salta a ellas. Todos salen apagados (MIX a 0). Probados solos en el ordenador
(`tests/fx_render.c`): estables al máximo, sin NaN, y la reverb decae siempre (DECAY topa en 0,9).
Memoria: 326 de 344 KB de la zona de buffers (delay de 0,6 s por lado en 16 bits, reverb en float).
CPU en el emulador con todo puesto: alrededor del 60 % de un núcleo sonando (la voz sola, ~42 %).

**Editor de pasos** (botón EDIT): un piano roll de una octava con las filas UP, DOWN, GATE, ACC y
SLIDE debajo, como la hoja de patrones del 303 (la idea de Javier, con una captura de un editor de
referencia). SELECT o K1 mueven el cursor; una tecla pone la nota del paso y pasa al siguiente (con
el secuenciador parado, además suena); K2 la octava, K3 el gate (REST, NOTE, TIE), K4 la nota de
semitono en semitono; ENV el acento y LFO el slide; REC deshace; EDIT o HOME salen. Una nota fuera de la escala pasa la
escala a CROMÁTICA sin perder ninguna altura. La edición vuelve al patrón con `acid_import` (el
`importRendered` del C++; 2.000 patrones van y vuelven sin cambios, `tests/pattern_import.c`).
**Ojo:** girar DENSITY, RANGE, ACCENTS o SLIDES, o GENERATE, rehacen el patrón desde la semilla y
borran lo editado (REC lo recupera si fue lo último).

Cambio de botones: mutar slides y acentos pasa de EDIT a **ARP**.

Pruebas del simulador: `app/tests/basic.txt` (25), `fx.txt` y `editor.txt`, todas pasan.

## Pantalla de arranque

La carita acid en azul Animatek, la imagen de Javier (`art/splash-smiley.png`). Para cambiarla:
sustituir el PNG y `python3 tools/make_splash.py` (genera `app/splash_img.h`, 16 colores en tiradas,
unos 10 KB). Se dibuja en `app_splash()`.

## Ojo con la CPU

Medido en el emulador: el 42 % de un núcleo, contando instrucciones (los ciclos reales del FPU
pueden subirlo algo). Cabe, pero es una voz cara: el 4× de sobremuestreo con cinco `tanh` por
muestra. Si hace falta sitio para el secuenciador u otra voz, por orden: tanh más barata,
coeficientes del filtro cada 2-4 muestras y sobremuestreo 2×, comparando cada cambio con la prueba.

## Carpetas

```
app/            la app (fm1-base la compila con su plataforma)
  app.c, app.h  teclas, knobs, pantalla, MIDI, ajustes
  dsp/          el motor: atek_voice.c/.h, y la licencia de Open303
  fm1.toml      nombre, identidad del paquete, opciones de compilación
tests/          la comparación con el original: ref_render.cpp (C++), port_render.c, comparar.py
base -> ../terceros/fm1-base
build.sh        ./build.sh (firmware), --host (simulador), --web (emulador del navegador)
```

## La prueba de parecido

```sh
cd tests && mkdir -p build
A=/mnt/SPEED/CODE/VCV-Rack/ATEK303
g++ -std=c++17 -O2 -w -I$A/thirdparty/open303 -I$A/src -I. -o build/ref_render ref_render.cpp $A/thirdparty/open303/*.cpp -lm
gcc -std=c99 -O2 -ffp-contract=off -I../app/dsp -I../base/include -I. -o build/port_render port_render.c ../app/dsp/atek_voice.c -lm
cd build && ./ref_render ref.wav && ./port_render port.wav && python3 ../comparar.py ref.wav port.wav
```

Cada cambio del motor tiene que seguir pasando esta comparación (o explicar por qué no).
