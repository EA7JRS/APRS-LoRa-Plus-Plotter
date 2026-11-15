#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later
"""Lector de motor del plotter (NMEA 2000 y J1939) contra CANboat: arma cada mensaje con las definiciones de CANboat y
comprueba que lib/plotter_core/src/n2k.cpp lee lo mismo. Uso: decoder_check.py <canboat.json> <canboat-j1939.json> <dump>"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
import canboat_check as cb

K = 273.15
N2K = [
    (127488, {"Instance": 0, "Speed": 2310.5, "Boost Pressure": 64000}, {"rpm": 2310.5, "boostKpa": 64}),
    (127489, {"Instance": 0, "Oil pressure": 405000, "Oil temperature": 96.35 + K, "Temperature": 83.25 + K, "Fuel Rate": 6.8,
              "Total Engine hours": 3600 * 812, "Engine Load": 58},
     {"oilPressKpa": 405, "oilTempC": 96.35, "coolantC": 83.25, "fuelRateLh": 6.8, "hours": 812, "load": 58}),
    (127505, {"Instance": 0, "Type": 0, "Level": 71.2}, {"fuelPct": 71.2}),
    (127508, {"Instance": 0, "Voltage": 13.74}, {"volt": 13.74}),
]
J1939 = [
    (61444, {"Engine RPM": 1812.5}, {"rpm": 1812.5}),
    (61443, {"Engine Percent Load At Current Speed": 47}, {"load": 47}),
    (65262, {"Engine Coolant Temp": 84 + K, "Engine Oil Temp 1": 96.5 + K}, {"coolantC": 84, "oilTempC": 96.5}),
    (65263, {"Engine Oil Pressure": 420000}, {"oilPressKpa": 420}),
    (65266, {"Fuel Rate": 11.35}, {"fuelRateLh": 11.35}),
    (65270, {"Boost Pressure": 124000}, {"boostKpa": 124}),
    (65271, {"Battery Voltage": 27.65}, {"volt": 27.65}),
    (65276, {"Fuel Level 1": 62.4}, {"fuelPct": 62.4}),
    (65253, {"Engine Total Hours of Operation": 3600 * 1520}, {"hours": 1520}),
]


def run(defs, cases, dump):
    version, by = cb.load(defs)
    n = bad = 0
    for pgn, fields, want in cases:
        p = by[pgn]
        data = bytearray([0xFF] * (p.get("Length") or 8))
        for name, value in fields.items():
            f = cb.field(p, name)
            raw = round((value - (f.get("Offset") or 0)) / (f.get("Resolution") or 1))
            cb.set_bits(data, f["BitOffset"], f["BitLength"], raw & ((1 << f["BitLength"]) - 1))
        out = json.loads(subprocess.check_output([dump, "dec", str(pgn), data.hex()]))
        n += 1
        if not out["ok"]:
            print("FALLO %d: el lector no lo acepta" % pgn); bad += 1
        for k, v in want.items():
            n += 1
            got = out["v"].get(k)
            if got is None or abs(got - v) > max(abs(v) * 1e-4, 0.02):
                print("FALLO %d %s: leido %r, CANboat %r" % (pgn, k, got, v)); bad += 1
        n += 1
        if set(out["v"]) - set(want):
            print("FALLO %d: datos de mas: %s" % (pgn, sorted(set(out["v"]) - set(want)))); bad += 1
    print("CANboat %s (%s): %d comprobaciones, %d fallos" % (version, os.path.basename(defs), n, bad))
    return bad


if __name__ == "__main__":
    sys.exit(1 if run(sys.argv[1], N2K, sys.argv[3]) + run(sys.argv[2], J1939, sys.argv[3]) else 0)
