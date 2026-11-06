# FM1-Animatek

Investigación y desarrollo de firmwares para el **M-VAVE FM-1**: el emulador (fm1-emu), los
firmwares de la comunidad, el rescate de un FM-1 bloqueado y el firmware propio de Animatek.
Empezado el 2026-11-06. Público en GitHub desde el 2026-11-06: `animatek/FM1-Animatek`, GPL-3.0. En español.

Empieza por `README.md`; lo que se sabe está en `docs/` (01 cómo funciona, 02 emulador,
03 rescate, 04 firmwares existentes, 05 ideas).

## Reglas

- **El código de otros vive en `terceros/`** (ignorado por Git) y se clona con
  `herramientas/clonar-terceros.sh`. No se copia código ajeno a nuestras carpetas sin
  respetar su licencia: casi todo es GPL-3.0; fm1-base es MPL-2.0; los motores de Felucca
  MIT; los ports de Mutable MIT. Un firmware nuestro que tome código GPL será GPL.
- **Nunca en Git**: `*.fwsc`, `*.fm1save`, `*.bin`, las descargas y las copias de la flash.
  El `FM-1.fwsc` oficial es de M-VAVE.
- **El FM-1 real no se flashea sin copia completa de la flash** (doble lectura, ver
  `docs/03-rescate.md`) y sin que Javier lo pida. Probar siempre antes en el emulador.
- **Compilar o ejecutar código de terceros** (cargo, build.sh, el toolchain de JieLi) lo
  aprueba Javier: el modo automático lo bloquea.
- Lo que no se ha oído no se afirma: las ideas llevan «sin probar» hasta sonar en el emulador.

## Changelog

Cada cambio, una línea en `CHANGELOG.md` (español, por fecha, lo más reciente arriba) y una
entrada en el changelog compartido con
`cambios apuntar "FM1-Animatek" "<qué cambió, verificación, commit>" -a claude`
(nunca editando `/mnt/SPEED/CODE/CHANGELOG.md` a mano: es un enlace a Obsidian).
