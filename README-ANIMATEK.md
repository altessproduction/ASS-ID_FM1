# FM1-Animatek

Todo lo del **M-VAVE FM-1** en un sitio: el emulador para probar firmwares sin el sinte,
los firmwares de la comunidad para estudiarlos, cómo rescatar un FM-1 bloqueado y, sobre
todo, el camino para hacer **el firmware de Animatek**.

Empezado el 2026-11-06, antes de tener el FM-1 en casa: todo se prueba primero en el
emulador.

## Por dónde empezar

| Quiero… | Lee |
| --- | --- |
| Entender cómo funciona un firmware del FM-1 y con qué se hace | [docs/01-como-funciona.md](docs/01-como-funciona.md) |
| Probar firmwares en el ordenador o en Bitwig | [docs/02-emulador.md](docs/02-emulador.md) y [emulador/](emulador/README.md) |
| Rescatar un FM-1 que no arranca | [docs/03-rescate.md](docs/03-rescate.md) |
| Ver qué firmwares hay, cuáles triunfan y cómo están hechos (AMB-1, Mutable…) | [docs/04-firmwares-existentes.md](docs/04-firmwares-existentes.md) |
| Las ideas para el firmware de Animatek, con lo que se puede y lo que no | [docs/05-ideas-animatek.md](docs/05-ideas-animatek.md) |
| Bajar los firmwares al disco | [firmwares/](firmwares/README.md) |
| **ATEK303**, nuestro firmware acid (1.2): qué hace, cómo se compila y se instala | [atek303/README.md](atek303/README.md) · [cheat sheet](https://animatek.github.io/FM1-Animatek/atek303/cheatsheet/) · [tocarlo en el navegador](https://animatek.github.io/FM1-Animatek/atek303/play/) |

## Carpetas

```
docs/          lo que sabemos, en español
emulador/      cómo compilar y usar fm1-emu aquí (Linux, Bitwig)
firmwares/     cómo bajar los firmwares; las descargas van a firmwares/descargas/ (fuera de Git)
backups/       copias de la flash de nuestro FM-1 cuando llegue (fuera de Git)
herramientas/  clonar-terceros.sh: baja todo el código de otros a terceros/
terceros/      el código de otros, clonado (fuera de Git)
```

Para tenerlo todo: `herramientas/clonar-terceros.sh`.

## Reglas

- **El código de otros no entra en nuestro Git**: vive en `terceros/` y se vuelve a clonar.
- **Ni el firmware de M-VAVE ni las copias de la flash entran en Git.** El `FM-1.fwsc` es
  de M-VAVE, y una copia de la flash lleva su firmware, nuestros sonidos y la calibración.
- **Nada de flashear el FM-1 real sin copia de seguridad** de la flash entera (ver
  [docs/03-rescate.md](docs/03-rescate.md)).

## Licencia

**GPL-3.0-or-later** ([LICENSE](LICENSE)). El ATEK303 lleva el motor de Open303 (licencia en
[atek303/app/dsp/LICENSE-open303.txt](atek303/app/dsp/LICENSE-open303.txt)) y se monta sobre
[fm1-base](https://github.com/charlesvestal/fm1-base), que es MPL-2.0 y se clona aparte: `atek303/base` es un enlace a
`terceros/fm1-base`, así que antes de compilar hay que ejecutar `herramientas/clonar-terceros.sh`.
