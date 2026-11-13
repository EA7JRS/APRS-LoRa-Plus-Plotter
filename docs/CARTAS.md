# Cartas en la tarjeta TF

La pestaña Carta dibuja como fondo las teselas que encuentre en la tarjeta (FAT32), con la numeración habitual de
OpenStreetMap (XYZ, 256 × 256 píxeles, PNG):

```
/charts/osm/<zoom>/<x>/<y>.png
```

Otros mapas base (Ajustes > Mapa base), con la misma numeración XYZ:

| Mapa base | Carpeta | Formato |
|---|---|---|
| OpenSeaMap | `/charts/osm` | PNG |
| IHM (no válida para la navegación) | `/charts/ihm` | PNG |
| IGN base | `/charts/ign` | JPEG (`.jpeg`) |
| Ortofoto PNOA | `/charts/pnoa` | JPEG (`.jpeg`) |

Encima del mapa base puede ir la capa transparente de **marcas de OpenSeaMap** (boyas, luces, balizas), con la misma
numeración, en `/charts/seamark/<zoom>/<x>/<y>.png` (PNG con transparencia). Se enciende en la carta, **Capas** >
*Marcas OpenSeaMap*, con su propia transparencia (− / +). Así una descarga de OpenSeaMap se puede guardar como viene:
el mapa de OpenStreetMap en `/charts/osm` y las marcas en `/charts/seamark`.

Los servicios TMS del IGN numeran la `y` al revés: al copiarlas hay que guardarlas con la `y` de OpenStreetMap.
Los JPEG no se han probado todavía con teselas reales.

- Zoom de 5 a 18. Para navegar por una zona basta con los zooms 10 a 15.
- Lo que no esté en la tarjeta se queda en azul liso: el barco, la derrota y los blancos AIS se ven igual.
- Las teselas se buscan al mover la carta; lo que falta se vuelve a mirar cada 30 s (por si se copian cartas nuevas).
- Hasta 24 teselas en pantalla; decodificadas ocupan una caché de 8 MB en la PSRAM.

## De dónde sacarlas

Más adelante (fase 3) el plotter las bajará él mismo con el internet del móvil y las guardará en la tarjeta. Mientras
tanto hay que copiarlas a mano. Cualquier programa que exporte «teselas OSM z/x/y» sirve. Condiciones a respetar:

- OpenStreetMap y OpenSeaMap piden no hacer descargas masivas de sus servidores y citar la fuente
  («© OpenStreetMap contributors»; la carta ya lo muestra).
- Las cartas del IHM y del IGN tienen sus propias condiciones; las del IHM no son válidas para la navegación.

## Probar en el PC

`tools/ui_preview/fake_tiles.py /tmp/plotter_sd/charts/osm` crea teselas de prueba (una costa inventada con la rejilla a
la vista) y `tools/ui_preview/build.sh <lvgl> carta.ppm 1 420 cerrar` dibuja la Carta con ellas.

## Sin verificar en la placa

- El montaje de la tarjeta (`board::sdBegin`): el CS va por el expansor y se deja activo; a la librería SD se le da como
  CS el GPIO 6, que se supone libre (`-DSD_DUMMY_CS=n` para cambiarlo).
- La velocidad al mover la carta con teselas reales.
