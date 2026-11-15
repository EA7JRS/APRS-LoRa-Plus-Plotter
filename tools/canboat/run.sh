#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later
# Contrasta el nucleo con una release FIJADA de CANboat (https://canboat.github.io/canboat/, Apache-2.0): los codificadores
# NMEA 2000 (n2ktx) y el lector de motor NMEA 2000 / J1939 (n2k). Uso: tools/canboat/run.sh [carpeta con los json].
set -e
cd "$(dirname "$0")/../.."
CANBOAT_VERSION=v9.2.0
DIR="${1:-.build/canboat-$CANBOAT_VERSION}"
mkdir -p .build "$DIR"
for f in canboat.json canboat-j1939.json; do
    [ -s "$DIR/$f" ] || curl -fsSL "https://raw.githubusercontent.com/canboat/canboat/$CANBOAT_VERSION/docs/$f" -o "$DIR/$f"
done
g++ -std=gnu++17 -O1 -Wall -Wextra -Werror -Ilib/plotter_core/src lib/plotter_core/src/*.cpp tools/canboat/dump_frames.cpp -o .build/canboat_dump
./.build/canboat_dump | python3 tools/canboat/canboat_check.py comprobar "$DIR/canboat.json"
python3 tools/canboat/decoder_check.py "$DIR/canboat.json" "$DIR/canboat-j1939.json" ./.build/canboat_dump
