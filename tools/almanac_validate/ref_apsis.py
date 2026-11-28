# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
# Parte de APRS LoRa+ Plotter: validacion del almanaque (ephem) contra astropy/ERFA. No se compila en la CI.
"""Referencia fina de apsides por cruce de cero de la derivada de la distancia (rejilla horaria)."""
import numpy as np
import astropy.units as u
from astropy.time import Time
from astropy.coordinates import get_sun, get_body, solar_system_ephemeris
solar_system_ephemeris.set('builtin')

jd0 = Time('2026-01-01T00:00:00', scale='utc').jd
jd1 = Time('2028-01-01T00:00:00', scale='utc').jd

def iso(jd): return Time(jd, format='jd', scale='utc').utc.isot[:19]+'Z'

def apsides(dist_fn, step_h, label, unit_to, decimals):
    grid = np.arange(jd0, jd1, step_h/24.0)
    tg = Time(grid, format='jd', scale='utc')
    d = dist_fn(tg)
    deriv = np.gradient(d, grid)               # d(dist)/d(jd)
    res=[]
    for i in range(len(deriv)-1):
        if deriv[i] <= 0 < deriv[i+1] or deriv[i] >= 0 > deriv[i+1]:
            # cruce de cero de la derivada: interp lineal
            frac = deriv[i]/(deriv[i]-deriv[i+1])
            j = grid[i] + frac*(grid[i+1]-grid[i])
            dj = dist_fn(Time(j, format='jd', scale='utc'))
            kind = 'perihelion' if deriv[i] < 0 else 'aphelion'
            if label=='MOONAPSIS': kind = 'perigee' if deriv[i] < 0 else 'apogee'
            res.append((kind, j, float(dj)))
    for kind,j,dj in res:
        print(label, kind, iso(j), ('%.'+str(decimals)+'f')%(dj*unit_to))

apsides(lambda t: get_sun(t).distance.to(u.au).value, 3.0, 'EARTHAPSIS', 1.0, 8)
apsides(lambda t: get_body('moon', t).distance.to(u.km).value, 1.0, 'MOONAPSIS', 1.0, 1)
