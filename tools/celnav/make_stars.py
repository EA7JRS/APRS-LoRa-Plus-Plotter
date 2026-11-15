#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later
"""Genera lib/plotter_core/src/celnav_stars.inc: las 57 estrellas de navegacion y la Polar (J2000 y movimiento propio)
a partir del catalogo de estrellas brillantes de PyEphem (pip install ephem; datos de Hipparcos). Uso: make_stars.py > ...inc"""
import ephem.stars

NAV = ("Acamar Achernar Acrux Adhara Aldebaran Alioth Alkaid Alnair Alnilam Alphard Alphecca Alpheratz Altair Ankaa Antares "
       "Arcturus Atria Avior Bellatrix Betelgeuse Canopus Capella Deneb Denebola Diphda Dubhe Elnath Eltanin Enif Fomalhaut "
       "Gacrux Gienah Hadar Hamal Kaus_Australis Kochab Markab Menkar Menkent Miaplacidus Mirfak Nunki Peacock Pollux Procyon "
       "Rasalhague Regulus Rigel Rigil_Kentaurus Sabik Schedar Shaula Sirius Spica Suhail Vega Zubenelgenubi Polaris").split()
rows = {}
for line in ephem.stars.db.strip().split("\n"):
    name, _, ra, dec, mag = line.split(",")[:5]
    rah, pmra = ra.split("|")
    decd, pmdec = dec.split("|")
    rows[name] = (float(rah), float(decd), float(pmra), float(pmdec), float(mag))
print("// GENERADO por tools/celnav/make_stars.py (catalogo de PyEphem, Hipparcos): no editar a mano.")
print("// nombre, magnitud, AR J2000 (horas), declinacion J2000 (grados), movimiento propio en AR (x cos dec) y dec (mas/anio)")
for n in NAV:
    ra, dec, pmra, pmdec, mag = rows[n.replace("_", " ")]
    print('{"%s", %.2ff, %.8f, %.8f, %.2f, %.2f},' % (n.replace("_", " "), mag, ra, dec, pmra, pmdec))
