<!-- Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later -->
# Libro de Faros del IHM → lista de luces

`libro_faros.py` lee el PDF del *Libro de Faros y Señales de Niebla* del Instituto Hidrográfico de la Marina (parte I y
parte II) y escribe la lista de luces en el formato del plotter (`lights.h`, `parseLightRecord`):

```
nombre;latitud;longitud;característica;altura m;alcance mn;número nacional
Tarifa. Isla de Tarifa. Faro;36.00167;-5.61000;Fl(3) W 10s;41;26;20080
```

```
python3 tools/faros/libro_faros.py libro_de_faros_parte_i.pdf faros_parte_ii.pdf -o luces.txt
python3 tools/faros/libro_faros.py ... -o luces.txt --zona 35.5 -7.5 37.5 -1.5   # solo un recuadro (S W N E)
python3 tools/faros/libro_faros.py ... -o luces.txt --sin-boyas                    # solo luces con altura
```

Necesita `pdftotext` (poppler-utils).

**Uso personal.** Los datos son © Instituto Hidrográfico de la Marina. La lista es para tu tarjeta: no la publiques ni la
subas a este ni a ningún otro repositorio. Este programa no lleva ningún dato del libro.

Qué hace:
- El hemisferio sale de la cabecera de cada página (`N/W`, `N/E`).
- El nombre se completa con los de su grupo, del más cercano al más lejano, mientras quepa en 39 bytes
  («-Isla de Tarifa» + «--Faro» = «Tarifa. Isla de Tarifa. Faro»).
- La característica pasa a la forma del plotter: `Fl(3)WR 10s` → `Fl(3) W 10s` (el primer color, el del sector
  principal), `VQ(6)W+LFl 15s` → `VQ(6)+LFl W 15s`, `DirQG` → `Q G`, `3QR(vert)` → `Q R`.
- Con sectores de alcance distinto (W26 / R18) se queda el mayor.
- Se saltan las marcas sin luz y las que solo tienen señal de niebla.

Con las dos partes de 2020 y 2023: 5098 registros, 4860 luces; el lector del plotter entiende todas salvo dos
características (`Mo(GB)` y `IQ`), que se guardan tal cual.
