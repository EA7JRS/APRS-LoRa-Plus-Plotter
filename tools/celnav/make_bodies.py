#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
"""Genera lib/plotter_core/src/celnav_bodies.inc: series VSOP87 recortadas (Tierra B y R, Venus, Marte, Jupiter y
Saturno) y las tablas 47.A / 47.B de la Luna (Meeus, "Astronomical Algorithms"). Los coeficientes se leen del paquete
PyMeeus (pip install pymeeus); se guardan los terminos de amplitud >= UMBRAL (1e-8 rad o UA), que deja los planetas
dentro de 0,06' de PyEphem entre 2026 y 2036.   Uso: make_bodies.py > lib/plotter_core/src/celnav_bodies.inc"""
from pymeeus import Earth, Venus, Mars, Jupiter, Saturn, Moon

UMBRAL = 100.0

def trunc(ser):
    out = []
    for k, s in enumerate(ser):
        lim = UMBRAL / (0.05 ** k) if k else UMBRAL
        out.append([t for t in s if abs(t[0]) >= lim or t[2] == 0])
    while out and not out[-1]:
        out.pop()
    return out

def emit(name, ser):
    ser = trunc(ser)
    for k, s in enumerate(ser):
        print("const Term %s%d[] = {" % (name, k))
        line = "   "
        for a, b, c in s:
            item = " {%.10g, %.10g, %.10g}," % (a, b, c)
            if len(line) + len(item) > 118:
                print(line); line = "   "
            line += item
        print(line + "\n};")
    print("const Series %s[] = {%s};" % (name, ", ".join("SER(%s%d)" % (name, k) for k in range(len(ser)))))

print("// Generado por tools/celnav/make_bodies.py: no editar a mano. VSOP87 (Bretagnon y Francou) segun Meeus, ap. III,")
print("// recortada a amplitudes >= %g (1e-8 rad o UA); Luna: tablas 47.A y 47.B de Meeus." % UMBRAL)
emit("kEarthB", Earth.VSOP87_B)
emit("kEarthR", Earth.VSOP87_R)
for nm, mod in (("Venus", Venus), ("Mars", Mars), ("Jupiter", Jupiter), ("Saturn", Saturn)):
    for c in "LBR":
        emit("k%s%s" % (nm, c), getattr(mod, "VSOP87_" + c))
print("const MoonLr kMoonLr[] = {")
for d, m, mp, f, l, r in Moon.PERIODIC_TERMS_LR_TABLE:
    print("    {%d, %d, %d, %d, %d, %d}," % (d, m, mp, f, l, r))
print("};\nconst MoonB kMoonB[] = {")
for d, m, mp, f, b in Moon.PERIODIC_TERMS_B_TABLE:
    print("    {%d, %d, %d, %d, %d}," % (d, m, mp, f, b))
print("};")
