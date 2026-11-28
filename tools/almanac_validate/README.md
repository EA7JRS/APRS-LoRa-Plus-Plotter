# Validación del almanaque (ephem) contra datos oficiales

El núcleo (`lib/plotter_core/src/ephem.cpp`, `celnav.cpp`, `astro.cpp`) calcula el
almanaque **offline** (Meeus / VSOP87 / celnav). Este banco lo contrasta con un patrón
automático (**astropy / ERFA**, sin descargas de kernels) y deja los valores para cotejar
con el **Anuario del Observatorio Astronómico Nacional (OAN / IGN)**, la fuente oficial en
España (`oan.es/servidorEfem`, Anuario del Real Observatorio de Madrid).

## Uso
```sh
pip install astropy numpy
tools/almanac_validate/run.sh
```

## Resultados (2026–2035, astropy/ERFA builtin)
- **Fases de la Luna**: < 5 s.
- **Estaciones** (equinoccios / solsticios): < 30 s.
- **Perigeo / apogeo**: distancia < 45 km; instante dentro de la diferencia entre teorías lunares (~15 min).
- **Perihelio / afelio** (geocéntrico, con el bamboleo lunar, como USNO / IMCCE / OAN): < 2,5 h.
  Antes, con la distancia al baricentro Tierra-Luna, el instante se iba hasta ~1 día y se
  saltaba algún evento; corregido en `nextEarthApsis` (distancia geocéntrica) y `nextExtremum`
  (cruce de la derivada en vez de sección áurea sobre el valor).

astropy/ERFA no es grado JPL, pero basta para validar a < 1 min; el kernel DE440 de JPL no
se puede descargar desde la sesión de Claude (proxy). Para el sello oficial, cotejar los
instantes con el Anuario del OAN.
