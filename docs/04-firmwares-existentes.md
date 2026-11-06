# Los firmwares que hay, cuáles triunfan y cómo están hechos

Datos del 2026-11-06. El emulador corre **154 firmwares** (lista completa con enlaces en
`terceros/fm1-emu/README.md`); en solo cinco semanas desde el primero (26/09).

## Los que más triunfan (estrellas en GitHub, 06/11)

| ★ | Firmware | Autor | Qué es | Licencia |
| ---: | --- | --- | --- | --- |
| 744 | **SLOOP** | isod89 (3dSam) | groovebox en directo de 4 pistas | GPL-3.0 |
| 654 | **Felucca** | Hügelton (Leo Kuroshita) | el original: 14 motores, secuenciador, editor web | GPL-3.0 |
| 312 | FM1 Open | M-VAVE | el oficial abierto (motor cerrado) | GPLv3 + excepción |
| 221 | **X0X** | Charles Vestal | groovebox: 909, 808 y dos 303 | GPL-3.0 |
| 214 | JIANT | juanjiant-bit | groovebox «biosintética», FM6 con Dexed | GPL-3.0 |
| 187 | Melodee | keremimo | sinte con editor web | GPL-3.0 |
| 136 | Skiff | illuminus5 | groovebox de 6 pistas, 14 motores | GPL-3.0 |
| 114 | **AMB-1** | Charles Vestal | ambient generativo, Music for Airports | MPL-2.0 |
| 102 | ChoralRoot | Quixotic7 | máquina de acordes | GPL-3.0 |
| 83 | GHOULBOX | jasonpersinger | dungeon synth (fork de Felucca) | GPL-3.0 |
| 77 | FoMni | Charles Vestal | arpa de acordes estilo Omnichord | GPL-3.0 |
| 76 | Clipper | isod89 | sinte profundo: 23 motores, 407 sonidos | GPL-3.0 |
| 65 | LOOM-8 | Charles Vestal | groovebox estilo OP-XY | MPL-2.0 |

**Lo que se lleva:** las **grooveboxes** (SLOOP, X0X, JIANT, Skiff, LOOM-8) y los
**generativos que suenan solos** (AMB-1, y detrás Bunker Evolved, DUB-1, HYP-1, Tessera,
Berlinan, Jam Playground). Luego, las **imitaciones de clásicos** (Juno, Minimoog, CS-80,
Rhodes, Omnichord, HiChord, OP-XY, J-6). Charles Vestal es el autor con más éxitos: hace
máquinas con una idea clara y un manual cuidado.

**Lo que ya existe y nos toca de cerca** (para no repetir y para aprender):

- **Kitten Rack** (Black Kitten Devices, GPL-3.0): **un modular dentro del FM-1**. Más de 50
  módulos estilo Eurorack (VCO, ladder, low pass gate, Turing, Marbles, Euclidean, Lorenz,
  Maths, eco de cinta, muelle, granular) con cables, editados en el propio FM-1 o en un editor
  web; 33 patches de fábrica. Es exactamente la base para la idea de leer `.pch` del G1.
- **BB1** y **X0X**: un bajo **303 derivado de Open303** (Robin Schmidt, MIT), con
  sobremuestreo, en `terceros/bb1-firmware/firmware/src/bb/x0x/bass303.c`. Prueba que un
  303 serio cabe y va en tiempo real.
- **GATOR**: **Plaits de Mutable entero**, sus 24 modelos. **Bubba Machine** usa los 808 y 909 de Plaits.
- **April OS** (sampler estilo Casio SK-1), **CHOPPAH** y **Amenity** (breaks), **Bytemania** (bytebeat).

## AMB-1 por dentro

`terceros/fm1-amb` (MPL-2.0). El motor entero son **1.784 líneas en un solo archivo**,
`app/dsp/amb.c`, más 1.474 de pantalla (`ui.c`). Está hecho sobre **fm1-base**.

**La idea:** no se toca, se cuida. Cuatro capas tiran de **una misma escala** (la tónica y
hasta 11 notas más; cada tecla mete o quita una nota) y de **un acorde que va derivando**:

| Capa | Cómo suena | Cómo se genera |
| --- | --- | --- |
| **PAD** | cuatro voces que «respiran»: Rhodes por FM o voces (sierras por dos formantes), filtro suave y chorus | cada voz crece, se sostiene y se apaga con su propio ciclo, de duraciones desiguales (1; 1,21; 1,43; 1,67), como los loops de *Music for Airports*. Al cambiar de acorde (cada 20 s a 5 min), las voces con notas comunes se quedan y las otras entran de nuevo, con conducción de voces mínima |
| **BELLS** | campanas suaves de **FM de dos operadores** | **cuatro loops de cinta de 13, 17, 23 y 29 negras** (números primos: nunca coinciden igual). Cada uno lleva una frase corta que se desplaza contra las otras. EVOLVE cambia una nota de vez en cuando; DENSITY, cuántas notas lleva cada loop |
| **BASS** | un seno y su suboctava, con glide | la tónica del acorde, a veces la quinta |
| **PULSE** | un golpe suave, un hat cerrado y uno abierto | tres patrones **euclidianos** de 16, 12 y 7 semicorcheas: polirritmia. HITS dice cuántos golpes; los patrones giran y cambian |

Por encima, **el ARC**: una marea lenta de minutos entre escaso (menos campanas, vocal más
oscura, menos pulso, más reverb) y lleno. Y **silencios**: el pulso se calla unos compases, el
bajo un acorde, un loop una pasada. Después, la cadena de *Discreet Music*: un Echoplex (160 ms
con realimentación), un loop de cinta de ~5,6 s con dos cabezas (cada repetición más oscura) y
la **reverb de placa de Dattorro**. FREEZE congela el acorde y deja la placa sonando para siempre.

**Lo técnico que vale copiar:**
- `en_render()` va en la interrupción de audio; todo lo demás manda **comandos a una cola** que
  el render vacía al empezar cada bloque. Así la pantalla nunca corta el audio.
- Todo en `float`, **sin libm**: sus propias funciones rápidas (`fastmath.h`).
- Azar con **xorshift** de Marsaglia; ritmos con **Bjorklund** (Toussaint).
- Ocho «piezas» = recetas de todos los parámetros (`en_piece`).
- Sigue el **reloj MIDI** si le llega, y manda MIDI por USB.

Para leer: `app/dsp/amb.h` (la explicación entera en el comentario de arriba) y luego `amb.c`.

## Cómo metieron a Mutable Instruments

Emilie Gillet publicó el código de sus módulos con **licencia MIT** (el de los módulos; el
hardware es CC-BY-SA), así que se puede portar con su aviso de copyright. Hay dos maneras:

1. **Portar a coma fija y recortar** (Felucca, motor PHYS): los modelos de cuerda y modales
   vienen de **DaisySP** (que a su vez viene de Rings) y las **cuerdas simpáticas de Rings**,
   reescritas en C con enteros (`firmware/src/phys_symp.c`, `phys_dsp.c`). Recortan donde el
   chip no llega: **3 cuerdas simpáticas en vez de 7**, líneas de retardo de 256 muestras a
   16 bits, filtros más simples. Cada archivo explica en su cabecera qué cambió («Departures»).
   Tienen tests que comparan el resultado con el original en coma flotante.
2. **Portar entero** (GATOR con Plaits): el modelo completo, porque Plaits es monofónico y cabe.

El patrón: elegir el módulo, portar lo justo, medir CPU con los tests (Felucca guarda el coste
de cada motor en `tests/cpu_baseline.txt`) y escribir qué se ha simplificado.

## Fuentes

- La lista: <https://github.com/joakimeriksson/fm1-emu> y <https://thefirmwarehub.com>
- Lista curada: <https://github.com/cicloid/awesome-fm-1>
- AMB-1: <https://github.com/charlesvestal/fm1-amb> · Kitten Rack: <https://github.com/BlackKittenDevices/kitten-rack>
- Felucca (créditos de Mutable en `CREDITS.md` y `LICENSING.md`): <https://github.com/hugelton/Felucca>
