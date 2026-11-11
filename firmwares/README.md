# Firmwares

Los firmwares se bajan a `firmwares/descargas/`, que **no entra en Git**: son de sus autores
y se pueden volver a bajar.

## Todos los del emulador, de golpe

El emulador trae un script que baja los paquetes de la web de cada autor (los de The Firmware Hub):

```sh
cd terceros/fm1-emu
node tools/fetch_firmwares.mjs        # a terceros/fm1-emu/reference/firmwares
```

*(Sin ejecutar todavía: ejecutar código de terceros lo aprueba Javier.)*

## Uno suelto

Cada firmware publica su `.fwsc` en las releases de GitHub, por ejemplo AMB-1:
<https://github.com/charlesvestal/fm1-amb/releases>. Guárdalo en `firmwares/descargas/`.

## El oficial de M-VAVE

`FM-1.fwsc`, de la página del FM-1 en la web de M-VAVE (ahora la V16). Para el rescate se
recomienda la **V15**: ver [../docs/03-rescate.md](../docs/03-rescate.md). Nunca en Git.

## Para estudiar

En `terceros/` hay clonados AMB-1, Felucca, SLOOP, Kitten Rack, BB1, GATOR, Dinghy, fm1-base y
FM1 Open: ver [../docs/04-firmwares-existentes.md](../docs/04-firmwares-existentes.md).
