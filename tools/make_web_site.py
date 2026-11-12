#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Prepara los ficheros del plotter y de su instrumento de viento para el instalador web de APRS LoRa+
# (https://ea7jrs.github.io/APRS-LoRa-Plus/, grupo "Nautica"): el contrato esta en docs/WEB_INSTALLER.md del tracker.
# Se llama una vez por placa con la misma carpeta de salida: latest.json va juntando las placas (el instrumento,
# waveshare_lcd4_wind, sale en el instalador como otra placa del grupo Nautica).
#
#   make_web_site.py --board waveshare_lcd7b --version 5.4.1 --build-dir .pio/build/waveshare_lcd7b \
#                    --boot-app0 RUTA/boot_app0.bin --out site
#
# Deja en "site":
#   latest.json                              {"device":"plotter","version","boards":{placa:{"version","chip","ota"}}}
#   manifests/<placa>.json                   manifiesto de ESP Web Tools (rutas ../firmware/<placa>/...)
#   firmware/<placa>/{bootloader,partitions,boot_app0,firmware}.bin
#   ota/<placa>.json                         tamano y SHA-256 del firmware
# No se graba la zona de ajustes (NVS): al instalar por la web se conserva la configuracion de la placa.
import argparse
import hashlib
import json
import os
import shutil
import sys


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--board", required=True)
    p.add_argument("--version", required=True)
    p.add_argument("--build-dir", required=True)
    p.add_argument("--boot-app0", required=True)
    p.add_argument("--out", required=True)
    a = p.parse_args()

    fw_dir = os.path.join(a.out, "firmware", a.board)
    for d in (fw_dir, os.path.join(a.out, "manifests"), os.path.join(a.out, "ota")):
        os.makedirs(d, exist_ok=True)
    src = {
        "bootloader.bin": os.path.join(a.build_dir, "bootloader.bin"),
        "partitions.bin": os.path.join(a.build_dir, "partitions.bin"),
        "boot_app0.bin": a.boot_app0,
        "firmware.bin": os.path.join(a.build_dir, "firmware.bin"),
    }
    for name, path in src.items():
        if not os.path.isfile(path):
            sys.exit("falta " + path)
        shutil.copyfile(path, os.path.join(fw_dir, name))

    parts = [                                             # ESP32-S3: el cargador va en 0x0
        {"path": "../firmware/%s/bootloader.bin" % a.board, "offset": 0x0},
        {"path": "../firmware/%s/partitions.bin" % a.board, "offset": 0x8000},
        {"path": "../firmware/%s/boot_app0.bin" % a.board, "offset": 0xE000},
        {"path": "../firmware/%s/firmware.bin" % a.board, "offset": 0x10000},
    ]
    product = "APRS LoRa+ Wind" if "wind" in a.board else "APRS LoRa+ Plotter"
    manifest = {
        "name": "%s %s" % (product, a.board),
        "version": a.version,
        "new_install_prompt_erase": False,
        "builds": [{"chipFamily": "ESP32-S3", "parts": parts}],
    }
    with open(os.path.join(a.out, "manifests", a.board + ".json"), "w") as f:
        json.dump(manifest, f, indent=2)

    fw = os.path.join(fw_dir, "firmware.bin")
    ota = {
        "board": a.board,
        "chip": "esp32s3",
        "version": a.version,
        "size": os.path.getsize(fw),
        "sha256": sha256(fw),
        "url": "firmware/%s/firmware.bin" % a.board,
        "ota": False,                                     # plotter e instrumento solo se actualizan por USB
    }
    with open(os.path.join(a.out, "ota", a.board + ".json"), "w") as f:
        json.dump(ota, f, indent=2)

    path = os.path.join(a.out, "latest.json")
    latest = {"device": "plotter", "version": a.version, "boards": {}}
    if os.path.isfile(path):
        with open(path) as f:
            latest = json.load(f)
        if latest.get("version") != a.version:
            sys.exit("latest.json tiene la version %s y esta placa la %s" % (latest.get("version"), a.version))
    latest["boards"][a.board] = {"version": a.version, "chip": "esp32s3", "ota": False}
    with open(path, "w") as f:
        json.dump(latest, f, indent=2)
    print("%s: version %s, %d bytes" % (a.board, a.version, ota["size"]))


if __name__ == "__main__":
    main()
