# Copyright (C) 2026 Manuel - EA7JRS
#
# This file is part of APRS LoRa+ Plotter.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Teselas de prueba para tools/ui_preview: una costa inventada, con la rejilla y el nombre de cada tesela a la vista,
# para comprobar en el PC que la carta las coloca donde toca. No son cartas de verdad.
#   python3 tools/ui_preview/fake_tiles.py /tmp/plotter_sd/charts/osm
import math, os, sys
from PIL import Image, ImageDraw

out = sys.argv[1]
LAT, LON = 36.5307, -6.2837           # donde arranca el simulador

def world(lat, lon, z):
    n = 256 * 2 ** z
    s = math.sin(math.radians(lat))
    return (lon + 180) / 360 * n, (0.5 - math.log((1 + s) / (1 - s)) / (4 * math.pi)) * n

def lat_of(py, z):
    n = 256 * 2 ** z
    return 90 - 360 * math.atan(math.exp(-(0.5 - py / n) * 2 * math.pi)) / math.pi

def lon_of(px, z):
    return px / (256 * 2 ** z) * 360 - 180

def land(lat, lon):                   # costa inventada al norte, con una ensenada
    return lat > 36.546 + 0.004 * math.sin((lon + 6.3) * 260) + 0.003 * math.sin((lon + 6.3) * 90)

for z in (12, 13, 14, 15):
    cx, cy = world(LAT, LON, z)
    tx0, ty0 = int(cx // 256), int(cy // 256)
    r = 4 if z >= 14 else 3
    for tx in range(tx0 - r, tx0 + r + 1):
        for ty in range(ty0 - r + 1, ty0 + r):
            im = Image.new("RGB", (256, 256))
            px = im.load()
            for j in range(256):
                la = lat_of(ty * 256 + j + 0.5, z)
                for i in range(256):
                    lo = lon_of(tx * 256 + i + 0.5, z)
                    if land(la, lo): px[i, j] = (214, 201, 160)
                    elif land(la + 0.004, lo): px[i, j] = (150, 198, 224)      # aguas someras
                    else: px[i, j] = (184, 219, 240)
            d = ImageDraw.Draw(im)
            d.rectangle([0, 0, 255, 255], outline=(120, 150, 170))
            d.text((6, 4), "%d/%d/%d" % (z, tx, ty), fill=(70, 95, 115))
            os.makedirs("%s/%d/%d" % (out, z, tx), exist_ok=True)
            im.save("%s/%d/%d/%d.png" % (out, z, tx, ty))
print("teselas de prueba en", out)
