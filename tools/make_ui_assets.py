#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Pantalla de bienvenida (LVGL 9.6) en src/ui_assets.cpp, la misma que APRS LoRa+ Display y la app Android:
# tools/assets/welcome_logo.jpg (copia de la de APRS-LoRa-Plus-Display) con los bordes fundidos con #131A22, en 256 colores
# (LV_COLOR_FORMAT_I8; LVGL 9 no escala las imagenes indexadas, asi que va una por pantalla):
#  - img_splash para la 7B del plotter (1024 x 600): 540 lineas
#  - img_splash para la LCD-4 del instrumento de viento (480 x 480): 400 lineas
# Cada una va dentro de su #if de placa, para no gastar flash con la otra.
#
#   python3 tools/make_ui_assets.py
import argparse

from PIL import Image

SPLASH_BG = (0x13, 0x1A, 0x22)


def feather(img, bg, frac=0.16):
    w, h = img.size
    m = Image.new("L", (w, h))
    bw = max(1, int(min(w, h) * frac))
    px = m.load()
    for y in range(h):
        for x in range(w):
            d = min(x, y, w - 1 - x, h - 1 - y) / bw
            t = 1.0 if d >= 1 else d * d * (3 - 2 * d)
            px[x, y] = int(255 * t)
    return Image.composite(img, Image.new("RGB", (w, h), bg), m)


def carray(name, data):
    out = ["static const uint8_t %s[] = {" % name]
    for i in range(0, len(data), 32):
        out.append("    " + ",".join(str(b) for b in data[i:i + 32]) + ",")
    out.append("};")
    return out


def splash(src, h):
    img = Image.open(src).convert("RGB")
    w = round(img.size[0] * h / img.size[1])
    img = feather(img.resize((w, h), Image.LANCZOS), SPLASH_BG)
    q = img.quantize(colors=256, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG)
    pal = q.getpalette()[:768]
    pal += [0] * (768 - len(pal))
    data = bytearray()
    for i in range(256):
        r, g, b = pal[i * 3:i * 3 + 3]
        data += bytes((b, g, r, 255))                     # lv_color32_t: azul, verde, rojo, alfa
    data += q.tobytes()
    return w, h, data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--splash", default="tools/assets/welcome_logo.jpg")
    ap.add_argument("--out", default="src/ui_assets.cpp")
    a = ap.parse_args()
    out = [
        "/*",
        " * Copyright (C) 2026 Manuel - EA7JRS",
        " * SPDX-License-Identifier: GPL-3.0-or-later",
        " *",
        " * Generado por tools/make_ui_assets.py (pantalla de bienvenida de APRS LoRa+). No editar a mano.",
        " */",
        '#include "ui_assets.h"',
        "",
    ]
    for macro, h in (("BOARD_LCD7B", 540), ("BOARD_LCD4", 400)):
        w, hh, data = splash(a.splash, h)
        out.append("#if defined(%s)" % macro)
        out += carray("splash_data", data)
        out.append("const lv_image_dsc_t img_splash = { { LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_I8, 0, %d, %d, %d, 0 }, "
                   "sizeof(splash_data), splash_data, nullptr, nullptr };" % (w, hh, w))
        out.append("#endif")
        print("%s: %dx%d, %d bytes" % (macro, w, hh, len(data)))
    out += ["const uint32_t SPLASH_BG_RGB = 0x%02X%02X%02X;" % SPLASH_BG, ""]
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("\n".join(out))


if __name__ == "__main__":
    main()
