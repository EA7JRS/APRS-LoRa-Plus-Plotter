# Changelog

## 5.4.1 — 2026-11-12 — primera versión (preliminar)

Mismo número de versión que el resto de APRS LoRa+. Compila en la CI; **sin probar en la placa**.

- **Pestañas:** Instrumentos (8 casillas con la fuente y la antigüedad de cada dato), Carta, Rutas, Radar AIS, Viento,
  Piloto, Meteo y Ajustes.
- **Carta:** teselas de la microSD (OpenSeaMap, IHM, IGN, ortofoto), barco, derrota, blancos AIS, ir a un punto,
  waypoints y ruta en GPX, medir, y fondeo con alarma de garreo.
- **Radar AIS:** distancia, demora, CPA / TCPA, riesgo de abordaje, blancos perdidos y ficha de cada blanco.
- **Alertas:** COLLISION RISK, MAN OVERBOARD, ANCHOR DRAG, avisos de Signal K y aviso de RS485 compartido. Sin sonido.
- **Entradas:** RS485 (AIS y NMEA 0183), NMEA 2000 por CAN (solo escucha, decodificador propio), WiFi (NMEA por TCP y
  UDP, gpsd, Signal K nativo, el móvil con la app), Bluetooth (tracker APRS LoRa+ con PIN, pasarela, UART de Nordic) e
  internet (OpenWeatherMap) como reserva cuando falta un sensor.
- **Piloto:** rumbo a gobernar, desvío, timón y salida APB / RMB por RS485 o por la red.
- **Ajustes:** simulador, puerto serie, brillo, unidades (náutico, métrico, imperial), mapa base, NMEA 2000, red y Bluetooth.
- **Instalación:** instalador web de APRS LoRa+ (grupo Náutica), esptool o PlatformIO.
- **Manuales:** guía rápida, manual de usuario y manual de instalación y funcionamiento, en español y en inglés.
- **Núcleo** (`lib/plotter_core`): 1466 comprobaciones en el PC.
