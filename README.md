# APRS LoRa+ Plotter

Plotter náutico para la pantalla Waveshare ESP32-S3-Touch-LCD-7B (1024×600), parte del ecosistema APRS LoRa+.
Repositorio aparte por ahora; la idea es integrarlo después en APRS-LoRa-Plus-Display.

**Es una ayuda a la navegación. No sustituye a la carta oficial ni a la vigilancia.**

## Estado

Fase 1 en curso. Hecho: el núcleo de cálculo (`lib/plotter_core`), sin dependencias de Arduino y con pruebas en el PC.
Todavía no hay firmware que compile para la placa.

| Módulo | Contenido |
|---|---|
| `geo` | Círculo máximo, loxodrómica, Vincenty (WGS84), desvío de ruta |
| `motion` | CPA / TCPA, viento real y aparente, VMG, corriente |
| `wind` | Escala Beaufort, anemómetro de cazoletas por pulsos |
| `units` | Sistemas náutico, métrico, imperial y usuario |
| `astro` | Orto, ocaso, crepúsculos, posición del sol, edad de la luna |
| `store` | Tabla única de datos con fuente, hora y prioridad |
| `nmea` | NMEA 0183: RMC, GLL, VTG, DPT, DBT, MWV, HDT, VHW, MTW, VDM, VDO |
| `ais` | Mensajes AIS 1, 2, 3, 5, 18, 19 y 24 |
| `gpx` | Lectura y escritura de GPX 1.1 (waypoints, rutas y tracks) |

## Pruebas

```sh
./tools/run_host_tests.sh
```

## Fases

1. **Núcleo:** instrumentos, carta con waypoints y rutas, radar AIS, alarmas, viento, piloto (datos de ruta). Datos por CAN (NMEA 2000) y serie.
2. **Tracker y emergencias:** enlace BLE con el tracker APRS LoRa+ y la ventana de emergencias de la app Android, LSD de la emisora.
3. **Internet y mensajes:** mapas base (OpenSeaMap, IHM), capas de Portus y AEMET, meteo, Navtex y SafetyNET por TCP.
4. **Mareas y navegación astronómica:** mareas por cálculo armónico y rectas de altura con sextante.

Vista previa de las pantallas: https://claude.ai/artifact/3rGbM86ZobibCQ2KU44DAD

## Licencia

GPL-3.0-or-later. APRS LoRa+ · © 2026 Manuel - EA7JRS
