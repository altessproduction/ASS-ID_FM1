# El emulador en este ordenador

Qué es y qué hace: [../docs/02-emulador.md](../docs/02-emulador.md). Aquí, cómo dejarlo
funcionando en Linux y en Bitwig.

## Estado (2026-11-06)

- [x] Código clonado en `terceros/fm1-emu`.
- [x] Rust en `~/.cargo` (rustup, sin sudo; cargo 1.99.1). Para usarlo: `export PATH=$HOME/.cargo/bin:$PATH`.
- [x] **Compilado** `fm1-live` y `fm1-emu` (`terceros/fm1-emu/target/release/`), con permiso de Javier.
- [x] **Plugin CLAP compilado e instalado** en `~/.clap/FM-1.clap` (15 MB; exporta `clap_entry`).
      Bitwig lo encuentra al reescanear plugins.
- [x] **155 firmwares** bajados (`node tools/fetch_firmwares.mjs`) y copiados a `firmwares/descargas/`.
- [ ] Probarlo en Bitwig (Javier).

## Probarlo en Bitwig

1. Bitwig: *Settings > Locations > Plug-ins*, que esté `~/.clap` (suele estar), y **Rescan**.
2. Instrumento nuevo: busca **FM-1** (fabricante fm1-emu).
3. En la ventana del plugin: **Load firmware / save…** y elige un `.fwsc` de
   `/mnt/SPEED/CODE/FM1-Animatek/firmwares/descargas/` (por ejemplo `amb.fwsc`, el AMB-1).
4. Tócalo desde el piano roll o un teclado MIDI. El firmware de M-VAVE escucha en el canal 2.
5. El proyecto guarda el firmware y sus sonidos.

Si va justo de CPU: el ajuste *CPU clock* del plugin baja el reloj emulado a 160 o 120 MHz.

## En una ventana, sin Bitwig

```sh
/mnt/SPEED/CODE/FM1-Animatek/terceros/fm1-emu/target/release/fm1-live \
    --fwsc /mnt/SPEED/CODE/FM1-Animatek/firmwares/descargas/amb.fwsc
```

## Los pasos

```sh
export PATH=$HOME/.cargo/bin:$PATH
cd /mnt/SPEED/CODE/FM1-Animatek/terceros/fm1-emu

# 1. El programa de escritorio (ventana, audio, MIDI)
cargo build --release -p fm1-live -p fm1-emu
node tools/fetch_firmwares.mjs
target/release/fm1-live --fw amb

# 2. El plugin para Bitwig (CLAP)
cd crates/fm1-plugin
cargo build --release
mkdir -p ~/.clap && cp target/release/libfm1_plugin.so ~/.clap/FM-1.clap
```

Atajo sin compilar: `fm1-live` para Linux x86-64 ya compilado en
<https://github.com/joakimeriksson/fm1-emu/releases/latest>. El plugin para Linux no existe
compilado: hay que hacerlo nosotros.
