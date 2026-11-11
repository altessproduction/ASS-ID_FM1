# Rescatar un FM-1 bloqueado

Herramientas de la comunidad, **no oficiales**. Lo de aquí sale de sus README
(`terceros/rescate/`) y del hilo de r/MVaveFM1 que confirmó el método. A tu riesgo.

## Antes de que pase: prevenir

1. **Copia de la flash entera** el primer día, antes de instalar nada (paso «Copia» abajo).
   Guárdala en `backups/` (fuera de Git) y en otro disco.
2. Guarda el **`FM-1.fwsc` oficial** (M-VAVE ya solo ofrece la V16; la V15 la recomiendan los
   rescates: hay una copia enlazada en <https://github.com/aroum/fm1-custom-fw/issues/1>).
3. Al instalar: **batería cargada**, cable directo **sin hub**, Chrome o Edge, y no tocar
   nada hasta que acabe.
4. Antes de cambiar de firmware, **exporta tus sonidos** con la herramienta del firmware que
   tienes: el nuevo los pisa.
5. Prueba el firmware antes en el emulador.

## Qué le pasa: los tres casos

| Síntoma | Qué es | Solución |
| --- | --- | --- |
| Arranca, pero el firmware nuevo va mal o no deja instalar otro | se queda «encerrado» (pasa con la flash nueva `0xB36014`) | **Caso 1**: modo de rescate del firmware o The Firmware Hub. Si no, caso 2 |
| Pantalla negra, los instaladores dicen «FM-1 not found», pero el ordenador ve un disco **`WL82 UBOOT1.00`** | brick blando: el firmware está roto, pero el modo de arranque del chip vive | **Caso 2**: unbricker por USB |
| Pantalla negra y **nada** por USB (solo se enciende la luz de carga) | brick duro | **Caso 3**: FM-1 Transporter (una placa RP2040) |

Si se ha roto la zona de arranque (`0x0000–0x3FFF`), ninguna de estas vías la arregla.

## Caso 1: el modo de rescate del firmware

- Familia Felucca (SLOOP, AMB-1, X0X, Dinghy y muchos más): **mantén OCT− mientras lo
  enciendes**. Entra solo el actualizador, sin sonido. Si el firmware falla dos veces al
  arrancar, entra solo.
- Desde ahí: The Firmware Hub (<https://thefirmwarehub.com>) o el instalador de cada uno
  tienen **«volver al oficial»**.

## Caso 2: brick blando, por USB (Linux sirve)

`terceros/rescate/MvaveFM1Unbricker` (Quixotic7). Usa
[jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool) para hablar con el modo de arranque
del chip (`WL82 UBOOT1.00`, USB `4C4A:8057`), que vive en la ROM y ninguna actualización borra.

```sh
cd terceros/rescate/MvaveFM1Unbricker
python3 fm1_unbrick.py setup              # baja jl-uboot-tool y sus dependencias
python3 fm1_unbrick.py find               # ¿ve el disco WL82 UBOOT1.00?  (en Linux: lsscsi -g o dmesg)
python3 fm1_unbrick.py info               # chip y flash
python3 fm1_unbrick.py backup ../../../backups/fm1-$(date +%F)   # copia: lee la flash dos veces y compara
python3 fm1_unbrick.py restore FM-1.fwsc  # copia + escribe 0x4000..0x92FFF + verifica byte a byte
```

- `restore` hace siempre la copia antes, pide escribir `WRITE` y verifica. Si la
  verificación falla (código de salida 3): **no lo apagues**, vuelve a lanzar `restore`.
- Con el V15 oficial comprueba el hash; también vale un paquete de la familia Felucca.
- Si se queda encerrado en un firmware con la flash nueva: ponlo en modo ROM (mantén
  **OCT−, OCT+ y HOME 5 s**, y enciende con **OCT−**; puede variar según el firmware) y
  `python3 fm1_unbrick.py restore FM-1.fwsc --i-know`.
- **`softkey`**: si el FM-1 aún arranca, lo manda al modo ROM por MIDI (SysEx
  `F0 22 24 35 7D F7`) sin escribir nada. Sirve también para hacer **la copia del primer día**.
- `repair-settings`: devuelve a fábrica las zonas de ajustes si quedaron corruptas al volver
  al oficial.
- Después de escribir: desenchufa, apaga, espera 5 s y enciende.

En Mac hay otra herramienta: `terceros/rescate/fm1-mac-recovery`.

## Caso 3: brick duro, con el FM-1 Transporter

Una **Seeed XIAO RP2040** (o una RP2040-Zero, o un ESP32-S3) conectada a las líneas de datos
del USB del FM-1 lo fuerza a entrar en el modo de arranque. Hay que abrirlo y soldar o pinzar
cables (GND y dos pines de datos; el rojo, de alimentación, no se conecta: el FM-1 va con su
batería, **cargada**).

- Guía paso a paso y script que lo automatiza: `terceros/rescate/MvaveFM1Unbricker/TRANSPORTER-GUIDE.md`
  y `fm1_transporter_recover.py wizard --v15 FM-1.fwsc` (Linux vale: el Transporter es un puerto serie).
- Guía para principiantes con RP2040-Zero: `terceros/rescate/FM1-Unbricking-For-Dummies`.
- El Transporter: <https://github.com/kurogedelic/FM-1-transporter>. Hay versión ESP32:
  <https://github.com/dagnazty/fm1-esp32-unbrick>.

**Recomendación:** comprar una XIAO RP2040 (unos euros) cuando llegue el FM-1, y tenerla en
el cajón antes de empezar a instalar firmwares propios.

## Fuentes

- <https://github.com/Quixotic7/MvaveFM1Unbricker>
- <https://github.com/GoodPerson42/FM1-Unbricking-For-Dummies>
- <https://github.com/HuskiePJN/fm1-mac-recovery>
- <https://github.com/dagnazty/fm1-esp32-unbrick>
- <https://github.com/aroum/fm1-custom-fw>
- Un caso real: <https://github.com/isod89/sloop-fm1/issues/185>
