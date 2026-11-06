# El emulador: fm1-emu

De **Joakim Eriksson** (MIT). Emula el chip del FM-1 entero (CPU, pantalla, teclas, knobs,
audio, USB) y corre **los firmwares sin tocarlos**: el mismo `.fwsc` que instalarías.
Código en `terceros/fm1-emu` (Rust).

## La web que cambia de firmware

Lo que viste es esto: **<https://joakimeriksson.github.io/fm1-emu/>**. Eliges firmware en la
lista, Start, y se toca con el teclado del ordenador, el ratón o un teclado MIDI (Web MIDI en
Chrome, Edge o Firefox).

- Corre **154 firmwares**: todos los de [The Firmware Hub](https://thefirmwarehub.com/) que
  funcionan, más algunos de fm1-editor.com. La página no aloja ninguno: los baja de la web de
  cada autor al arrancar.
- `?fw=amb` abre uno directamente (AMB-1). Lista completa en `terceros/fm1-emu/README.md`.
- Guarda solo los cambios de la flash (sonidos, ajustes) por firmware. **Save & backup**
  exporta un `.fm1save` portátil.
- El firmware de M-VAVE no se puede bajar así: se abre a mano el `FM-1.fwsc` de su web.
- También hay página del SMK-37.

## En el ordenador: `fm1-live` (Linux)

Programa con ventana, audio y MIDI. En Linux x86-64 **compila el firmware a código nativo
(JIT)** y va a los 240 MHz reales.

```sh
cd terceros/fm1-emu
cargo run --release -p fm1-live -- --fw list          # lista de firmwares
node tools/fetch_firmwares.mjs                        # los baja a reference/firmwares
cargo run --release -p fm1-live -- --fw amb           # AMB-1 en una ventana, con audio
cargo run --release -p fm1-live -- --fwsc ruta.fwsc   # cualquier paquete
```

Opciones útiles: `--midi NOMBRE` o `--midi-ch N` (toca desde cualquier MIDI del ordenador),
`--flash ARCHIVO` (guarda la flash entre sesiones), `--usb` (el FM-1 aparece como dispositivo
MIDI "(emu)" en el ordenador, y así se prueban los instaladores), `--syx banco.syx` (carga un
banco de DX7 en el firmware de M-VAVE).

`FM1_STRICT=1` lo hace ir **como un FM-1 de verdad** (tiempos de cada instrucción, la pantalla
a su velocidad): sirve para saber si nuestro firmware llegará a tiempo en el sinte. Sin eso
el emulador es generoso.

También existe ya compilado en [las releases](https://github.com/joakimeriksson/fm1-emu/releases/latest)
(`fm1-live` para Linux x86-64).

## En Bitwig: el plugin

El plugin está hecho en Rust como **CLAP** (nih-plug), y el VST3 y el AU son envoltorios.
Oficialmente solo hay **Mac y Windows**: los scripts no compilan para Linux, aunque nih-plug
y clap-wrapper sí lo soportan (`crates/fm1-plugin/README.md`, tabla de plataformas).

**Para Bitwig en Linux basta el CLAP**, que es solo la librería de Rust:

```sh
cd terceros/fm1-emu/crates/fm1-plugin      # va aparte: está fuera del workspace de Cargo
cargo build --release
mkdir -p ~/.clap && cp target/release/libfm1_plugin.so ~/.clap/FM-1.clap
```

En Linux puede pedir librerías de sistema para la ventana (egui: X11 o Wayland) y para el
diálogo de abrir archivo (rfd: GTK3 o el portal de escritorio).

**Compilado e instalado aquí el 06/11** en `~/.clap/FM-1.clap`: ver [emulador/README.md](../emulador/README.md). El
plugin carga un `.fwsc` o un `.fm1save`, recibe el MIDI del DAW, se toca su panel con el
ratón y guarda firmware y sonidos con el proyecto. El firmware de M-VAVE escucha en el canal 2.

## Fuentes

- <https://github.com/joakimeriksson/fm1-emu> (README y `crates/fm1-plugin/README.md`)
- La web: <https://joakimeriksson.github.io/fm1-emu/>
