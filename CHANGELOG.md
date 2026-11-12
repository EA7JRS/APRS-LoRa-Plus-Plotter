# Changelog

## 5.4.2 — 2026-11-12
- **Fórmulas comunes del ecosistema** en el núcleo (las usan también tracker, rotor, Display y app): `wmm` (Modelo
  Magnético Mundial WMM2025 de NOAA / BGS, grado 12, se puede cargar un `WMM.COF` nuevo; igual que pygeomag a 0,002°),
  luna con Meeus (posición topocéntrica con paralaje, salida y puesta, fase en 8 partes; a 0,03° de PyEphem), sol con
  Meeus (0,02°, antes la serie de NOAA a 0,3°), refracción, `unixToUtc`, y `wxcalc` (punto de rocío, humedad, sensación
  térmica, índice de calor, presión al nivel del mar). 1586 comprobaciones.
- Ajustes > Estado > *Declinación*; rumbo verdadero = magnético + declinación si no llega uno verdadero.
- Meteo: sensación térmica, punto de rocío, sol (salida, puesta, crepúsculo civil) y luna (fase, salida, puesta).

- **Piloto Raymarine ST2000+** (el de Manuel; Piloto > *ST2000+*, de serie, o *Genérico*): RMB y APB con waypoints de 4
  letras («W001»: el ST2000+ solo lee cuatro), VWR con el viento aparente para su modo WindTrim (acepta VWR, no MWV) y VHW,
  una vez por segundo. Salida nueva **Bluetooth** (a la pasarela XIAO, que la saca a 4800). Aviso si sale por el RS485 sin
  estar a 4800. Núcleo: `nmeaVwr`, `pilotWaypointId`; 1554 comprobaciones.

- **Hora en todos los equipos**: el reloj interno se pone con la hora del GPS (RMC / ZDA de cualquier fuente NMEA, PGN
  126992 y 129029 de NMEA 2000; dos horas seguidas que cuadren) y, sin GPS, con NTP: `ntp.roa.es` (hora oficial de España,
  Real Instituto y Observatorio de la Armada), de reserva `hora.roa.es` y `pool.ntp.org`. Núcleo `timesync` (`ClockSync`:
  el GPS manda 6 h sobre NTP, luego la hora de otro equipo y al final el RTC) y módulo `tsync` de `lib/aprs_links`.
  Ajustes > Estado > *Hora*; el plotter manda la hora al instrumento por Bluetooth (ZDA). 1549 comprobaciones.
- **`lib/aprs_links` para el rotor**: la WiFi puede ser del programa (`ownWifi`), `aprs_net.h` y Bluetooth central
  opcional.

- **Instrumento de viento APRS LoRa+ Wind** (accesorio del plotter, nuevo entorno `waveshare_lcd4_wind`): Waveshare
  ESP32-S3-Touch-LCD-4 (480 × 480, revisión V4.0) con la veleta NASA Target 2 Wind por el RS485 (MWV a 4800). Páginas
  Rosa (tres repartos), Viento, Números, Historial de 30 min y Ajustes y enlace; viento real y deriva con la velocidad y
  el rumbo del NMEA 2000. Pasa el viento al plotter por Bluetooth (servicio de `docs/BLE_NMEA.md`), NMEA 2000 (PGN
  130306 con reclamación de dirección) y WiFi (UDP a la red). Diseño propio. Compila en la CI; **sin probar en la
  placa**. Manual: `docs/INSTRUMENTO_VIENTO.md` / `docs/WIND_INSTRUMENT_EN.md`.
- **Instrumento de viento, modo noche y alarmas**: rojos sobre negro (Ajustes > Noche) y alarmas de viento real,
  viento aparente y rolada con la ventana STRONG WIND / WIND SHIFT y el zumbador de la placa (núcleo `windalarm`).
- **Bienvenida al arrancar** en el plotter y en el instrumento: el logo de APRS LoRa+ de la app y de APRS LoRa+ Display
  (`tools/assets/welcome_logo.jpg`, `tools/make_ui_assets.py`) con «Plotter» o «Wind» y la versión, unos 2 s.
- **Acerca de** en el plotter (Ajustes > Acerca de) y en el instrumento: versión, copyright, licencia, proyecto,
  créditos y QR del donativo (paypal.me/ea7jrs), con el mismo marco que APRS LoRa+ Display.
- **Rumbo y velocidad del plotter al instrumento por Bluetooth** (HDT, VTG y VHW una vez por segundo, característica
  `…0002` de `docs/BLE_NMEA.md`): el instrumento calcula el viento real aunque no haya NMEA 2000.
- **Instalador web**: la release lleva las dos placas en `web-site-plotter.zip` (grupo Náutica).
- **Núcleo**: emisión NMEA 2000 (`n2ktx`: identificador, PGN 130306, NAME y reclamación de dirección); 1493 comprobaciones; alarmas de viento (`windalarm`), 1517.

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
