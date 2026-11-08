# ATEK303 para el FM-1

La voz del **ATEK303** de VCV Rack (`../../VCV-Rack/ATEK303`) como firmware del M-VAVE FM-1,
sobre **fm1-base**. Primer paso del plan (ver `../docs/05-ideas-animatek.md`): aprender la
plataforma con un motor que ya es nuestro. Licencia GPL-3.0-or-later, como el ATEK303.

## Estado (2026-11-08): lo que trae la 1.2

- **WAVE es una lista de 14 osciladores**: SAW, SAW+SUB, FAT, DETUNE+SUB, SHINE, METAL, GROWL, BREATH, WILD,
  OCTAVES, OCT UP, FIFTH UP, SQR+SUB y SQUARE. Son las recetas del ATEK303 de VCV y bajos al estilo del Roland
  TB-3 (solo los osciladores), con código propio: una capa antes del filtro con seno afinable, la otra onda con
  salto y desafinado, ruido, ring y cross-mod. SAW y SQUARE en los extremos: los presets de antes suenan igual.
- **ALGO** elige la onda desde cualquier página.
- **Editor**: K1 semitono, K2 octava, K3 ACC/SLIDE, K4 gate; SELECT recorre el patrón entero.
- **EDIT** recorre editor → página SEQ → página PATT; **GLO** abre OPTIONS; LOCK pasa a **OCT− + SEQ**.
- **Lista de presets**: SEL carga al momento (también sonando) y las 16 teclas blancas son presets.
- Probado en el FM-1 real el 08/11 (instalación con `--classic`, presets intactos); ocho pruebas del simulador.

## Estado (2026-11-07): lo que trae la 1.1

- **64 pasos** en cuatro páginas, con el generador v5 de UZZ-VCV-RACK (el mismo patrón que en VCV con la misma
  semilla). SEQ en marcha entra en el siguiente compás; un preset, al acabar el patrón (NEXT en ámbar).
- **Editor por páginas**: SELECT los 16 pasos de la página, K4 la página, K1 gate, K2 octava, K3 semitono,
  OCT− + K4 la longitud (en la 1.2 cambian los mandos, ver arriba).
- **12 presets de fábrica de Javier** (`app/factory_presets.h`, de su copia por USB); un FM-1 nuevo arranca en el 01.
  Pantalla de guardar con OVERWRITE? NO / YES. Copia y restauración por USB (`fm1_backup.py --magic AT64`).
- **OPTIONS** (OCT− + HOME): color, canales MIDI, notas del secuenciador y reloj por USB, y el ajuste fino del
  ATEK303 de VCV (ver «Opciones»).
- **Batería** arriba a la derecha y **limitador** a la salida (+3 a +6 dB de media, sin recorte duro).
- Reloj MIDI externo clavado con el DAW (el paso 2 en el 7.º pulso). En el plugin de fm1-emu el DAW no manda
  reloj (el plugin no convierte el transporte); con el FM-1 real, sí, por USB.
- Todo instalado y probado en el FM-1 real el 07/11; siete pruebas del simulador en `app/tests/`.

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
- [x] **Probado en un FM-1 real** (06/11): instalado con `terceros/fm1-base/tools/fm1_install.py --classic`
      después de copiar la flash entera; Javier: «funciona genial». En Linux, siempre `--classic`: la instalación
      rápida se quedó al 2 % («USB LOST»).
- [x] HOME recorre VCF, VOICE, PATT y SEQ (probado en el FM-1 real).
- [x] **1.0** (`FM-1_330010000`): entra desde la V15 y desde otro firmware (AMB-1), y se sale de él. Ficha del hub en
      `../fmhub.json`, portada en `art/cover.jpg`. Release: `./build.sh --release X.Y` y el `.fwsc` a la release de GitHub.
- Es **nuestro firmware base**: se queda así, sin más motores (los de Plaits y la caja de ritmos, aparte).

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
- En `--keys`, los encoders del emulador van con otra numeración que `plat.h`: el encoder PRESETS es `enc6`.
- En `fm1-live --bench`, la entrada por guion (MIDI, botones) no le llegó a ningún firmware, ni a
  X0X: para medir se compiló una variante con la demo arrancando sola.

## Fase 2: el secuenciador (2026-11-06)

El generador de patrones del ATEK303 SEQ de **UZZ-VCV-RACK** (`src/AcidPattern.hpp`, `AcidCorpus.hpp` y lo
que usa de `AcidGen.hpp`: la versión 5, de 64 pasos y calibrada con un corpus de líneas acid), portado a
C en `app/dsp/acid_pattern.c`. Es el mismo algoritmo, con los mismos flujos de azar: **una semilla da
exactamente el mismo patrón en VCV y en el FM-1**. Comprobado con `tests/pattern_golden.cpp` (el C++
original) contra `tests/pattern_port.c`: 7.680 patrones (6 ajustes × 8 escalas × 4 longitudes —64, 32,
16 y 7— × 40 semillas), cada uno con 6 mutaciones y su rejilla, salen idénticos byte a byte (61.440
líneas), también con la `fm_powf` de fastmath que usa el FM-1:

```sh
cd tests
g++ -std=c++17 -I../../../VCV-Rack/UZZ-VCV-RACK/src pattern_golden.cpp -o build/pattern_golden
gcc -std=c99 -I../app/dsp -I../base/include pattern_port.c ../app/dsp/acid_pattern.c -o build/pattern_port
./build/pattern_golden > build/golden.txt && ./build/pattern_port > build/port.txt && cmp build/golden.txt build/port.txt
```

(Hasta el 2026-11-07 era la versión 4 del repo `ATEK303`, de 16 pasos.)

El paso, el gate, el tie y el slide siguen `Atek303Seq::process` con la voz pegada (el slide lo
hace la voz). La resolución es de 32 muestras (0,7 ms).

**Controles**

| | |
| --- | --- |
| PLAY | arranca o para (reloj interno; con reloj MIDI, lo sigue: start, stop, continue, 6 ticks por paso) |
| SEQ | GENERATE: semilla nueva. **OCT− + SEQ** activa o quita LOCK (SEQ encendido mientras). Con LOCK, muta las tres capas. Sonando, **espera al siguiente compás** (16 pasos; NEXT en ámbar mientras) |
| ENV / LFO / ARP | muta TIEMPO / NOTAS (octavas) / SLIDES y ACENTOS, **en la página de 16 pasos a la vista** (al momento) |
| HOME | recorre las páginas de sonido: VCF, VOICE, PATT, SEQ; desde el editor, la lista o un efecto, directo a VCF |
| REC | deshace la última mutación o edición |
| GLO | **OPTIONS** (color, MIDI, ajuste fino); GLO otra vez, o cualquier botón menos OCT−, las cierra |
| EDIT | lo de la secuencia, en ciclo: **editor → página SEQ → página PATT → editor** |
| ALGO | **WAVE** desde cualquier página: SAW, las combinaciones de la capa extra, SQUARE |
| teclas | parado: tocan la voz. Sonando: **transportan** (la tecla es la nueva raíz, como en un 303) |
| SELECT | páginas: 1 y 2 la voz (la 2 lleva GATE, la duración de las notas del secuenciador), 3 DENSITY, RANGE, ACCENTS, SLIDES, 4 STEPS (1–64), SCALE, TEMPO, ROOT |
| SAVE | guarda knobs, patrón, semilla, mutaciones, tempo, raíz y octava |

Abajo, siempre, una tira de 16 pasos con los colores del editor acid de VCV y el paso en curso
enmarcado.

**64 pasos (2026-11-07)**, como el ATEK303 SEQ de UZZ-VCV-RACK: cuatro páginas de un compás. STEPS
va de 1 a 64 (32 = dos páginas). La tira y el editor enseñan la página que suena (sonando), la del
cursor (en el editor) o la primera (parado); con más de 16 pasos, cuatro marcas encima de la tira
dicen qué página se ve (azul) y cuáles existen (gris). El editor pasa de página al llevar el cursor
más allá del paso 16. Los presets ocupan ahora 4 objetos de la memoria (8 en cada uno: 0xD8000,
0xDA000, 0xD4000 y 0xD6000) y su formato cambió (`AT64`): los guardados antes no se cargan.

**Arranque** (valores de Javier, 2026-11-07): CUTOFF 369 Hz, RESO 80 %, ENV MOD 4 %, DECAY 707 ms;
DENSITY 100 %, RANGE 80 %, ACCENTS 51 %, SLIDES 44 %; el primer patrón crece de esos knobs.

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
referencia). Desde el 2026-11-08, como pidió Javier: **SELECT** mueve el cursor por todo el patrón
(la página a la vista lo sigue; al final vuelve al paso 1); **K1** la nota de semitono en semitono,
**K2** la octava (UP / DOWN), **K3** ACC / SLIDE (nada, ACC, SLIDE, los dos), **K4** el gate (REST,
NOTE, TIE); **OCT− mantenido + K4** cambia STEPS (1–64) sin salir del editor, y OCT− solo baja la
octava al soltarlo. Una tecla pone la nota del paso y pasa al siguiente (con el secuenciador parado,
además suena); ENV el acento y LFO el slide; REC deshace; EDIT pasa a la página SEQ (y luego a PATT) y HOME sale a VCF. Una nota fuera de la escala pasa la
escala a CROMÁTICA sin perder ninguna altura. La edición vuelve al patrón con `acid_import` (el
`importRendered` del C++; 2.000 patrones van y vuelven sin cambios, `tests/pattern_import.c`).
**Ojo:** girar DENSITY, RANGE, ACCENTS o SLIDES, o GENERATE, rehacen el patrón desde la semilla y
borran lo editado (REC lo recupera si fue lo último).

Cambio de botones: mutar slides y acentos pasa de EDIT a **ARP**.

Pruebas del simulador: `app/tests/basic.txt` (25), `fx.txt` y `editor.txt`, todas pasan.

## Movimientos grabados (quitados el 2026-11-06)

Se probaron parameter locks (REC grababa por paso y había una vista de carriles con barras), pero a Javier no le
convencieron y se quitaron el mismo día. El código, por si se retoma, quedó fuera del repo en el scratchpad de la
sesión; con la descripción del changelog se puede rehacer.

## Opciones (OCT− + HOME)

**OCT− + HOME** (OCT− mantenido y HOME) abre **OPTIONS**; HOME (o cualquier botón menos OCT−) la cierra y la
guarda. HOME solo sigue pasando de página, aunque se mantenga; OCT− solo baja la octava al soltarlo. SELECT elige la línea y K1–K4 cambian el valor. Las opciones son del aparato, no de los presets:
van en su propio objeto (`OBJ_OPTIONS`, 0xDE000), no se guardan con un preset ni entran en la copia por USB.

- **DISPLAY · COLOR:** ANIMATEK (los azules de VCV) o los temas de fm1-base: GREEN, AMBER, CYAN, RED y MONO. Los
  colores del editor (nota, tie, acento, slide) salen del tono del tema.
- **MIDI:** canal de entrada (ALL o 1–16: notas y CC de otro canal se ignoran; el reloj siempre entra), canal de
  salida (1–16, para las teclas y el secuenciador), **SEQ NOTES OUT** (las notas del secuenciador por USB: acento
  con velocidad 127, el resto 80; un slide manda la nota nueva antes de soltar la anterior) y **CLOCK OUT** (con
  el reloj interno, Start, Stop y 24 pulsos por negra; con reloj externo no manda nada).
- **FINE TUNING:** el menú «Fine tuning» del ATEK303 de VCV, con sus mismos valores: anchura de pulso, caída de la
  cuadrada, esquina y caída de la sierra, drive del filtro, tiempo de slide, auto-legato, saturación del OTA y de
  salida, deriva analógica y unidad. Por defecto, los del modelo «circuit», que es como sonaba hasta ahora.

## Batería

Arriba a la derecha, en la cabecera: tres rayas con lo que queda (ámbar si está vacía) y la pila en azul
cuando el USB da corriente (no hay línea del cargador: con USB no se sabe la carga). La nota que suena va a
su izquierda. En la web no sale (no hay batería). Sin probar en el FM-1.

## Colores

La paleta de los módulos Animatek de VCV (`../../VCV-Rack/UZZ-VCV-RACK/src/ui/CommonWidgets.hpp` y `AcidEditor.hpp`):
fondo pizarra #16181C, valores en el azul de las pantallas #5DB7FF, barras y selección en el azul del logo #2C7FFF,
rótulos #9AA2B5, grises #2C2C2C y #5A5A5A. La tira de pasos y el editor usan los colores del editor acid: nota
#2C7FFF, tie #1E4E9C, acento #CFE6FF, slide #00C2FF y octava #7F9CC4. Van en `animatek_palette()`, que pisa la
paleta de fm1-base al empezar la interfaz.

## Navegador de páginas

Al pie de las páginas de knobs hay una barra con 5 pestañas que se desplaza con la página (VCF, VOICE, PATT,
SEQ, DELAY, CHOR, DRIVE, REVRB). La actual va recuadrada en el azul del logo, como en la referencia que mandó Javier,
y hay flechas cuando quedan páginas a un lado. La tira de pasos queda justo encima.

## Presets (2026-11-06)

32 presets, 16 en cada uno de dos objetos de la memoria de fm1-base (con copia A/B y CRC: un corte de
luz a mitad de una escritura no los pierde). Cada preset guarda todo menos la página: patrón (con su
semilla y mutaciones), voz, efectos, pasos, escala, raíz, tempo, octava y LOCK.

- **SAVE** abre la **pantalla de guardar** (como la del FMini-1): el hueco (de 32) con EMPTY u
  OVERWRITE y el nombre. Se abre en el preset cargado (en la lista, en el hueco del cursor; sin
  ninguno cargado, en el primer libre). **PRESETS** cambia de hueco (el nombre sigue al del hueco),
  **SELECT** da otro nombre acid aleatorio de dos palabras («SQUELCH TRIP», «SNAKE LOOP»…), **SAVE**
  guarda y **cualquier otro botón cancela**. En un hueco ocupado pregunta **OVERWRITE?** con NO / YES
  (NO de entrada; se cambia girando cualquier encoder; SAVE confirma, y con NO vuelve atrás). También
  guarda el estado con el que arranca.
- **Encoder PRESETS**: el primer giro abre la **lista de presets** (los 32 huecos, 8 a la vista) en el
  cargado; los siguientes mueven el cursor y cargan el preset que pisan: **parado, al momento;
  en marcha, al acabar el patrón**, como en un 303 (pasa al paso 1 del nuevo; mientras espera, la
  lista y la cabecera dicen **NEXT** en ámbar). Si se elige otro antes, entra el último. En la lista
  (SEL encendido): **SEL** carga el del cursor **al momento**, también sonando; las **16 teclas
  blancas** son los presets de la mitad del cursor (1–16 o 17–32), con el cargado encendido y los
  guardados a media luz, y una pulsación lo carga al momento.
  **ENV dos veces** (en 2 s) borra el seleccionado, porque la primera solo avisa; **SAVE** abre la
  pantalla de guardar en ese hueco; **HOME** la cierra. Si no se toca, se cierra sola a los 6 s.

**De fábrica (2026-11-07)**: un FM-1 sin presets guardados arranca con 8 en los huecos 1–8 (tabla
`FACTORY` en `app.c`; el patrón sale de una semilla fija con los knobs del preset, igual en todos los
FM-1). Solo se ponen donde la memoria no tiene nada: un FM-1 que ya guardó presets no los recibe, y
uno borrado no vuelve.

**Los presets de Javier como los de fábrica.** Se hacen en el FM-1 y se sacan con la copia por USB
(fm1-base, `backup = true`, magic `AT64`):

1. En el FM-1 con ATEK303, guardar los presets en los huecos que se quiera (y borrar los que sobren).
   Un FM-1 nuevo arranca **en el preset 01** (si el 01 está vacío, en el estado del último SAVE).
2. Pararlo (PLAY apagado) y, con el cable puesto:
   `python3 base/tools/fm1_backup.py backup mis-presets.bak --magic AT64`
   (en el venv con mido y python-rtmidi, como `fm1_install.py`). La copia es también la de seguridad
   de esos presets; `restore` la devuelve (pide confirmación en la pantalla: SEL sí, HOME no).
3. `python3 tools/factory_from_backup.py mis-presets.bak` escribe `app/factory_presets.h` (lista los
   presets que lleva). También lee una imagen de flash de 1 MiB (la del simulador, `flash_dump`).
   `--drop 19` deja fuera un hueco guardado sin querer (vacío en un FM-1 nuevo).
4. `./build.sh` (y `--web`): con esa cabecera, los 32 huecos y el arranque salen de ahí y la tabla
   `FACTORY` deja de usarse. Borrar la cabecera vuelve a la tabla.

Probado en el simulador: presets hechos y uno borrado → `flash_dump` → cabecera → un FM-1 nuevo
arranca con esos presets, sin el borrado, y con el estado guardado. La cabecera vale para el FM-1 y
la web; el simulador de `--host` se queda con la tabla, porque las pruebas de `app/tests/` cuentan
con ella.

**Los de fábrica ahora son los de Javier** (copia del 2026-11-07, el 19 fuera con `--drop`): 1 COSMIC
TRAX, 2 MELTED BOX, 3 ELECTRIC PULSE, 4 ACID TRAX, 5 DUB ACID, 6 HYPER BLOB, 7 FUZZY SNAKE, 8 SPACE
MELT, 9 LASER PULSE, 10 BUBBLE ZAP, 11 BENT LINE, 12 SQUELCH GOO. La tabla de abajo es la de `app.c`,
la que queda sin cabecera:

| # | Nombre | Escala / raíz | BPM | Carácter |
| --- | --- | --- | --- | --- |
| 1 | ACID CLASSIC | ACID / C | 125 | saw, reso alta, delay 1/8. |
| 2 | SQUARE SQUELCH | ACID / D | 130 | square, reso al límite, overdrive |
| 3 | RUBBER BASS | MEN / A | 120 | grave y redondo, poca reso, seco |
| 4 | PHRYGIAN RAT | FRIG / E | 135 | RAT |
| 5 | DUB ACID | DOR / G | 118 | delay 1/4. largo, reverb, chorus |
| 6 | HYPER ACID | ACID / C | 145 | denso, muchos slides, fuzz |
| 7 | FLANGE WORM | BLUE / F | 128 | square con flanger |
| 8 | SPACE MELT | ARM / B | 100 | decay largo, chorus, delay y reverb |

Pruebas: `app/tests/steps64.txt` (64 pasos, páginas, mutación por página, preset en el hueco 32
tras reiniciar) y `app/tests/presets.txt` (los 8 de fábrica, guardar, cambiar, recargar, sobrescribir,
reiniciar, y el cambio en marcha que espera al final del patrón).

## Recuperación

Viene de fm1-base y no hay que hacer nada:
- **Modo seguro**: si el firmware falla o se cuelga dos veces al arrancar, entra solo en un modo sin
  sonido con el USB activo; ahí PLAY lo reintenta y REC entra en el modo de actualización del chip.
- **REC pulsado al encender**: el modo de actualización del chip (el disco «WL82 UBOOT» de las
  herramientas de rescate, ver `../docs/03-rescate.md`).
- **La vía de actualización por USB** sigue viva con el firmware en marcha: cualquier instalador o The
  Firmware Hub pueden poner otro firmware o el oficial.

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

## Plaits en el WAVE (aparcado el 2026-11-06 por decisión de Javier; el plan, por si se retoma)

Plan del prototipo, ya investigado:
- **Fuente:** el Plaits que lleva GATOR (`terceros/fm1-gator/third_party/eurorack`, MIT, commit 08460a69). Su único cambio útil:
  `kSampleRate = FW_FS_HZ` en `plaits/dsp/dsp.h`; nosotros daremos `FW_FS_HZ = 44100` con un `fw_rate.h` propio.
- **Modelos:** VA, WAVESHAPE, FORMANT (grain), HARMONIC (additive) y WAVETABLE. Ninguno lleva envolvente propia.
- **Compilación:** fm1-base no compila C++. Hay que añadir a nuestra copia local de `tools/build.py` una opción `cxx`:
  `pi32v2/bin/clang++` con las flags de GATOR (`-mcpu=r3 -mfprev1 -O2 -std=gnu++11 -fno-exceptions -fno-rtti
  -fno-threadsafe-statics -fno-use-cxa-atexit -nostdinc++ -ffunction-sections -fdata-sections`), y en el host con `c++`.
  Hacen falta unos shims propios, escritos por nosotros (GATOR los tiene en GPL-3.0-only): `<algorithm>` (min, max,
  swap, fill, copy, sort), `<cmath>` y `<new>` (placement new), además de `__cxa_pure_virtual`.
- **Integración en la voz:** Plaits a 44,1 kHz en bloques de 12. Su salida (×0,5) sustituye a `osc_sample()`, subida
  a 4× por interpolación lineal, antes del filtro de diodos. La nota sale de `inst_freq` en cada bloque (así el slide
  sigue funcionando).
- **Pantalla:** página OSC nueva en la posición 3 (MODEL, HARMONICS, TIMBRE, MORPH). WAVE muestra la misma lista. Al
  insertar la página cambian los índices: FX pasa a las páginas 5-8 → 6-9, y hay que actualizar las pruebas. Cambia
  el formato de los ajustes, así que los presets anteriores se pierden (avisar a Javier).
- **Medir:** la CPU en el emulador con la demo y todos los efectos (`--keys "700-800:PLAY"`, variante con los efectos
  activados). Ojo: según el código de fm1-base, el FM-1 corre a 360 MHz y el emulador a 240.
