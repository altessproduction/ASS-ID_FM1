# Cómo funciona un firmware del FM-1

Investigación del 2026-11-06, leyendo el código de `terceros/` (Felucca, fm1-base, Dinghy,
FM1 Open, fm1-emu, FM-1-RE y el unbricker). Lo que no se ha comprobado lleva *(sin confirmar)*.

## La máquina

| | |
| --- | --- |
| Chip | JieLi **AC791N (WL82)**: un chip de audio y Bluetooth de altavoces, no de sintes |
| CPU | dos núcleos **pi32v2 a 240 MHz**. El segundo es opcional y en algunas unidades falla (lee mal la RAM): los firmwares serios tienen plan B con un solo núcleo |
| Coma flotante | sí: AMB-1 hace todo el audio en `float`. Felucca y otros usan coma fija por velocidad |
| RAM | solo la interna del chip, **sin RAM externa** (fm1-base). Bluetooth o WiFi se comen casi toda: un firmware lleva uno de los dos, no ambos. Tamaño exacto *(sin confirmar)* |
| Flash | **1 MB** (chips `0x856014` o `0xB36014`, el de las unidades de 2026 con el logo delante) |
| Audio | 44,1 kHz estéreo, a 24 bits en el render. Además, la unidad de DSP y ecualizador del chip |
| Panel | 27 teclas (con velocidad y presión según el firmware), 14 botones, 7 encoders, el pote MASTER, LEDs |
| Pantalla | 240 × 240 en color (ST7789 por SPI). Pintar una pantalla entera tarda ~77 ms: el buen firmware no espera a la pantalla |
| Conexiones | USB-C (MIDI, audio por USB, actualizaciones), MIDI IN por minijack TRS, Bluetooth MIDI, altavoz y auriculares |

Hermano: el teclado **SMK-37** de M-VAVE lleva el mismo chip, la misma flash y la misma vía
de actualización. Muchos firmwares se compilan para los dos.

## Cómo está repartida la flash

| Zona | Qué hay |
| --- | --- |
| `0x0000–0x3FFF` | el arranque (uboot). **Ninguna herramienta de rescate lo escribe**: si se rompe, no hay vuelta por USB |
| `0x4000–0x92FFF` | el firmware (unos 572 KB). Es lo que se cambia al instalar |
| `0x93000–0xE8FFF` y `0xFC000–0xFEFFF` | ajustes, sonidos, proyectos, samples, calibración |

Cambiar de firmware **borra o pisa los datos del anterior** (los formatos no son compatibles
entre firmwares): copia antes con la herramienta de cada uno.

## El paquete `.fwsc` y cómo se instala

- Un firmware se reparte como **`.fwsc`**: la app más el *loader* de actualización, con una
  identidad (`FM-1_015` es la V15 oficial; Felucca usa `FM-1_918`).
- Se instala por **SysEx sobre USB-MIDI**. La única comprobación es un **CRC-16**: **no hay
  firma**, así que el FM-1 acepta cualquier paquete bien hecho. Por eso se abrió todo.
- Formas de instalar: el actualizador de M-VAVE (M-UPGRADE), los **instaladores web** (Chrome
  o Edge, por WebMIDI), **The Firmware Hub** (instala muchos y te devuelve al oficial) y
  `fm1_install.py` desde la terminal.
- Los firmwares de la familia Felucca guardan un **modo de rescate**: OCT− al encender, o solo
  si el firmware falla dos veces al arrancar. Así se puede instalar otro aunque el nuevo no
  arranque.

## Con qué se escribe un firmware

Todos son **C**, compilado con el **clang 4.0.1 de JieLi para pi32v2** más tres archivos del
**SDK AC79** de JieLi (Apache-2.0). El toolchain de JieLi es **Linux x86-64**: en este
ordenador corre nativo, sin Docker.

```sh
tools/get_toolchain.sh   # (en Felucca, Dinghy o fm1-base) baja el clang de JieLi a ~/.jieli/toolchain
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
./build.sh               # deja el .fwsc en build/
```

Hay cuatro bases donde apoyarse. **Para el firmware de Animatek, la recomendada es fm1-base**:

| Base | Licencia | Qué da | Para nosotros |
| --- | --- | --- | --- |
| **fm1-base** (Charles Vestal, autor de AMB-1, X0X, FoMni) | **MPL-2.0**: la app puede tener la licencia que queramos | pantalla, fuentes, panel, audio, MIDI USB y TRS, almacenamiento, copias de seguridad por USB, Bluetooth o WiFi a elegir, **simulador en el ordenador y en el navegador** con la misma app, plantilla en `template/` | **la mejor**: es la de AMB-1, tiene todo hecho y deja elegir licencia. En beta (la interfaz `plat.h` puede cambiar) |
| **Dinghy** (zvenson) | GPL-3.0 | el firmware mínimo: un seno a 4 voces en ~300 líneas encima de toda la fontanería de Felucca y SLOOP, modo host (WAV y PNG sin FM-1), comprobaciones que impiden lo que brickea | muy buena para aprender; obliga a GPL |
| **Felucca** (Leo Kuroshita, Hügelton) | GPL-3.0 (motores MIT, bsp MPL-2.0) | el original: 33.000 líneas, 14 motores, secuenciador, editor web, tests | para estudiar y para tomar motores |
| **FM1 Open** (M-VAVE) | GPLv3 con excepción, motor en binario | el oficial: 8 pistas, librería DX7, Workbench en el navegador | compila **solo en Windows** y el motor es cerrado: no nos sirve de base |

## Cómo se prueba sin el sinte

1. **Modo host** (Dinghy, fm1-base, Felucca): la misma app compilada para el ordenador saca
   el sonido a WAV y la pantalla a PNG. Segundos por prueba.
2. **El emulador fm1-emu**: corre el `.fwsc` de verdad, el mismo que se instalaría. Ver
   [02-emulador.md](02-emulador.md).
3. **El FM-1 real**, al final, con copia de seguridad hecha.

## Fuentes

- Código en `terceros/`: `fm1-base/README.md`, `dinghy/README.md`, `Felucca/BUILDING.md`,
  `fm1_open/README.md`, `fm1-emu/crates/fm1-soc/src/lib.rs`, `rescate/MvaveFM1Unbricker/README.md`.
- Ingeniería inversa: <https://github.com/AL-255/FM-1-RE>
