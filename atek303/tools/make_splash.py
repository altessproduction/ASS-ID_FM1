#!/usr/bin/env python3
"""La imagen del arranque a app/splash_img.h: escalada, con 16 colores y en
tiradas (índice, longitud), para dibujarla en app_splash() sin gastar casi flash.
Uso: python3 tools/make_splash.py [ALTO] [IMAGEN]   (por defecto 196 px y art/splash-smiley.png)"""
import sys
from PIL import Image
H = int(sys.argv[1]) if len(sys.argv) > 1 else 196
SRC = sys.argv[2] if len(sys.argv) > 2 else "art/splash-smiley.png"
im = Image.open(SRC).convert("RGB")
w = round(im.width * H / im.height)
im = im.resize((w, H), Image.LANCZOS).quantize(colors=16, method=Image.Quantize.MEDIANCUT)
pal = im.getpalette()[:48]
px = list(im.get_flattened_data() if hasattr(im, "get_flattened_data") else im.getdata())
runs = []
i = 0
while i < len(px):
    j = i
    while j < len(px) and px[j] == px[i] and j - i < 255:
        j += 1
    runs.append((px[i], j - i))
    i = j
rgb565 = lambda r, g, b: ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
with open("app/splash_img.h", "w") as f:
    f.write(f"/* generado por tools/make_splash.py desde {SRC}: no editar a mano */\n")
    f.write(f"#define SPLASH_W {w}\n#define SPLASH_H {H}\n#define SPLASH_RUNS {len(runs)}\n")
    f.write("static const uint16_t SPLASH_PAL[16] = {" + ", ".join(
        "0x%04X" % rgb565(*pal[3 * k:3 * k + 3]) if 3 * k + 2 < len(pal) else "0" for k in range(16)) + "};\n")
    f.write("static const uint8_t SPLASH_RLE[%d] = {\n" % (2 * len(runs)))
    for k in range(0, len(runs), 12):
        f.write("    " + ", ".join(f"{a}, {b}" for a, b in runs[k:k + 12]) + ",\n")
    f.write("};\n")
print(f"{w}x{H}, {len(runs)} tiradas, {2 * len(runs)} bytes")
