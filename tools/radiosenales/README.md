<!-- Copyright (C) 2026 Manuel - EA7JRS — SPDX-License-Identifier: GPL-3.0-or-later -->
# Libro de Radioseñales del IHM → costeras y centros de salvamento

`radiosenales.py` lee el PDF del *Libro de Radioseñales* del Instituto Hidrográfico de la Marina y escribe la lista
`costeras.txt` de la tarjeta del plotter (`coast.h`, `parseCoastRecord`):

```
nombre;latitud;longitud;tipo;MMSI;canal;canal 2;kHz
```

- tipo `V`: costera de VHF, con su canal de trabajo, el canal de Salvamento Marítimo y el MMSI del centro que la lleva;
- tipo `S`: centro de coordinación de salvamento, con su MMSI;
- tipo `M`: costera de onda media (kHz). El programa no la saca; se puede escribir a mano.

```
python3 tools/radiosenales/radiosenales.py radiosenales.pdf -o costeras.txt
```

Necesita `pdftotext` (poppler-utils). Copie `costeras.txt` a la raíz de la tarjeta. Con ella, la pantalla **Radio**
dice la costera de VHF y el centro de salvamento más cercanos a la situación del barco, con su canal y su MMSI.

**Uso personal.** Los datos son © Instituto Hidrográfico de la Marina. La lista es para su tarjeta: no la publique ni la
suba a este ni a ningún otro repositorio. Este programa no lleva ningún dato del libro.

**Revise lo que sale.** Probado con la edición de 2023. El programa dice por pantalla qué estaciones pone con cada
MMSI: en la tabla del libro el MMSI de cada grupo va a media altura, y el grupo se deduce de ahí. La posición de un
centro de salvamento es la que imprime el libro en su cabecera. Los canales cambian: contraste con los avisos a los
navegantes.
