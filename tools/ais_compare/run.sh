#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compara el decodificador AIS con las grabaciones de prueba de gpsd (https://github.com/ntpsec/gpsd, BSD-2-Clause).
# Uso: tools/ais_compare/run.sh <carpeta test/daemon de gpsd>
set -e
D="$1"
[ -d "$D" ] || { echo "uso: $0 <gpsd/test/daemon>"; exit 2; }
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../lib/plotter_core/src"
OUT="$(mktemp -d)"
g++ -std=gnu++17 -O2 -I"$SRC" "$SRC"/*.cpp "$HERE/dump.cpp" -o "$OUT/dump"
for f in ais-nmea ais-18-27 ais-raw-messages ais-nmea-type6-fid55; do
    echo "== $f"
    "$OUT/dump" "$D/$f.log" > "$OUT/$f.out" 2>/dev/null
    python3 -I "$HERE/cmp.py" "$OUT/$f.out" "$D/$f.log.chk" | grep -v "no esta en gpsd \\['24'"
done
