# Changelog

## 5.5.0 — 2026-11-25

- **Unificación del ecosistema en la 5.5.0** (tracker, app Android, rotor, Display, plotter y los kits al mismo número).
- **Sol, Luna y Tierra (`ephem`, núcleo):** lo que se deduce de las efemérides de `celnav` y de las constantes físicas de los tres cuerpos, para todo el ecosistema. **Luna:** fase verdadera por la elongación real (`moonPhaseTrue`; la del mes sinódico medio se desvía horas), próximas fases (`nextMoonPhase`), perigeo y apogeo (`nextMoonApsis`), diámetro aparente, eclipses de Luna y de Sol (`nextEclipse`: tipo y magnitud o gamma; de los de Sol solo se dice que lo hay en algún lugar de la Tierra), **marea de equilibrio** (`tideForceIndex`: fuerza de Luna y Sol frente a la sicigia media = 100; orienta, no predice ningún puerto) y **rebote lunar** (`emeInfo`: pérdida del trayecto, retardo del eco, Doppler y distancia topocéntrica). **Sol:** distancia, diámetro, irradiancia fuera de la atmósfera, índice UV de cielo despejado, rotación de Carrington, equinoccios y solsticios, perihelio y afelio. **Tierra:** horizonte óptico y de radio (4/3), alcance entre antenas, gravedad normal y radio del elipsoide WGS84. Contrastado con Meeus (ej. 49.a y 50.a), con las fases, estaciones y eclipses de 2026 y 2027 y con la pérdida de referencia en 432 MHz (261,6 dB). 2061 comprobaciones.
- **Meteo:** la línea de la Luna usa la fase y la parte iluminada verdaderas y añade la marea de equilibrio del momento («marea 107 %»).

## 5.4.7 — 2026-11-20

- **Colores de las escalas de NOAA** en el núcleo para las pantallas del ecosistema: `noaaScaleRgb` (1 amarillo, 2 naranja claro, 3 naranja, 4 rojo, 5 rojo oscuro), `xrayToR` (escala R de los rayos X: M1, M5, X1, X10, X20) y `xrayClassFlux` (5 comprobaciones; 1966 en total).
- **Terremotos de ShakeNet** (Raspberry Shake, QuakeLink): tercera fuente de terremotos de los avisos oficiales, cerca de la posición (M3,5+, 15°, 24 h), en el formato texto de FDSN; con duplicados mandan EMSC y USGS. Núcleo: `shakeUrl`, `quakeParseFdsnText`, `alertIsQuake` (8 comprobaciones; 1961 en total).
- **MarineTraffic sin clave**: el aviso de Ajustes > Embarcación da la ficha pública del barco (`mtcheck::webUrl`) y el botón **MarineTraffic** lo confirma a mano («MarineTraffic: OK»; otra pulsación lo quita). Sus condiciones prohíben leer la web con un programa.
- **Salida y puesta del sol exactas** (Meteo y crepúsculos): `sunTimes` y el mediodía solar se calculan con el lugar preciso del sol en ese momento (antes, con la declinación del mediodía: 2–3 min de error en España, 11 min a 66°; ahora menos de un segundo).
- **Radio**: botón en la barra de arriba con los mensajes MAYDAY, MAYDAY RELAY (de la última alerta LSD recibida), PAN PAN y SÉCURITÉ para leer por el canal 16, en español o inglés, con los datos de la embarcación (distintivo deletreado, MMSI cifra a cifra), la situación, la hora UTC y las personas a bordo; la ventana MAN OVERBOARD lo abre con «hombre al agua». Núcleo `radiotel` y `vessel::typeNameEn` (30 comprobaciones; 1996 en total).
- **Demoras**: botón de la Carta con la situación por dos demoras a luces de la tarjeta o waypoints, simultáneas o trasladadas (rumbo y velocidad efectivos, botón GPS), corrección total (botón Ct con la declinación y la tablilla), ángulo de corte y diferencia con el GPS; líneas y situación dibujadas en la carta. Núcleo `bearingfix` (11 comprobaciones; 1953 en total).
- **Corriente**: botón de la Carta con la corriente medida (GPS frente a rumbo y corredera, media de 2 min), la corriente por la estima (Marcar salida / Por la estima) y el rumbo a gobernar con corriente y abatimiento (verdadero, de aguja con la declinación y la tablilla, velocidad efectiva y tiempo al destino). Núcleo `current` y `rhumbDestination` en `geo` (34 comprobaciones; 1942 en total).
- **Latitud por la meridiana y por la Polar**: Carta > Astro > **Latitud** da la latitud de una sola altura (por la meridiana, por la Polar o, con aviso, fuera del meridiano) y la hora UTC y la altura del paso por el meridiano del astro elegido; botón **Polar**. Núcleo: `latitudeFromAltitude` y `meridianPassageJd` en `celnav` (14 comprobaciones; 1908 en total).
- **Luces en la Carta**: la lista `/luces.txt` de la tarjeta se dibuja con el color y el ritmo de cada luz (las principales desde el zoom 8, el resto desde el 11); tocar una abre su ficha con característica, altura, alcance, «se verá a» y distancia y demora, e **Ir a**.
- **Libro de Faros → lista de luces**: `tools/faros/libro_faros.py` convierte el PDF del Libro de Faros del IHM en la lista de luces del plotter, para uso personal (los datos del IHM no van en el repositorio).
- **LSD / DSC**: sentencia DSC de NMEA 0183 de una emisora con LSD o de la app APRS LoRa+ (que decodifica el canal 70 y MF / HF), por el RS485, la red o Bluetooth; Meteo > **LSD** con la lista (repeticiones en 10 min, tocar una con posición pone rumbo a ella) y ventana **DSC DISTRESS** para alertas, retransmisiones y acuses de socorro (IR A). Núcleo `dsc` (18 comprobaciones; 1894 en total).
- **NAVTEX**: sentencias NRX (forma de Furuno y de IEC 61162-1) de un receptor NAVTEX o de la app APRS LoRa+, por el RS485, la red o Bluetooth; Meteo > **NAVTEX** con la lista (tocar uno con posición pone rumbo a ella) y ventana **NAVTEX SAR / DISTRESS** para el asunto D. Las posiciones de los avisos aceptan también la forma compacta «3607.5N 00521.2W». Núcleo `navtex` (16 comprobaciones; 1876 en total).

- **Astro > Desvío (Ct): corrección total y tablilla de desvíos.** Demora de aguja de un astro (Sol, Luna, planetas, estrellas, botón *Polar*) + hora UTC → azimut verdadero, corrección total, declinación del WMM y desvío al rumbo de aguja (*Proa* toma el del compás). *A la tablilla* guarda la observación (24 como mucho, una por proa); la tablilla ajusta los coeficientes A–E por mínimos cuadrados y da el desvío cada 30°. *Aplicar al rumbo* (apagado de serie) suma el desvío al rumbo magnético del compás antes de la declinación. Núcleo `deviation` (13 comprobaciones; 1860 en total); NVS `devCard`, `devOn`.

- **Avisos de AEMET por su polígono**: con posición, el plotter baja el paquete público con todos los CAP en vigor (el .tar.gz que enlaza el RSS de Meteoalerta), lo descomprime (`gunzip` propio en el núcleo, ventana de 32 KB, CRC-32) y se queda con los avisos cuyo polígono tiene dentro el barco; solo lo vuelve a bajar si cambia o si el barco se mueve más de 5 km. Sin posición o si el paquete falla, el RSS con el filtro por zonas, como antes. Núcleo: `gunzip`, `TarStream`, `aemetCapBundle`, `aemetRssBundle` (12 comprobaciones; 1841 en total). Sustituye a la rama `avisos-meteo`, que duplicaba los avisos oficiales.

## 5.4.6 — 2026-11-15
_La 5.4.5 no tuvo entrada propia: lo que ya llevaba está también en esta lista._

- **Astro > Visibles**: lista de lo que hay ahora entre 10° y 80° (Sol, Luna, planetas y estrellas) por azimut; toca uno para elegirlo; en amarillo, las tres estrellas que mejor reparten el horizonte.
- **Astro: 99 estrellas más** (las «otras estrellas» de los almanaques: 157 en total; catálogo HYG / Hipparcos, CC BY-SA 4.0) y el error de índice y la elevación del ojo **se recuerdan**.
- **Astro: Luna y planetas.** Luna (limbo inferior o superior, con su paralaje y semidiámetro; Meeus cap. 47 completo, 0,25′ frente al JPL DE421) y Venus, Marte, Júpiter y Saturno (VSOP87 recortada con tiempo de luz y aberración, 0,06′ frente a PyEphem) en `celnav` y en Carta > Astro. La posición de la Luna de Meteo usa la misma serie.
- **Astro: traslado de rectas** (las anteriores se llevan al instante de la última por lo navegado, «(trasl.)») y **crepúsculo náutico** en la línea del Sol de Meteo.
- **Carta > Astro: recta de altura con el sextante.** Sol (limbo inferior o superior) y las 57 estrellas de navegación más la Polar; correcciones de error de índice, depresión, refracción, semidiámetro y paralaje; altura calculada, azimut y determinante; hasta cuatro rectas dibujadas en la carta y situación por dos o más. Núcleo `celnav` (`observedAltitude`, `reduceSight`, `fixFromSights`). Sin Luna ni planetas; sin traslado de rectas.
- **Navegación astronómica en el núcleo** (`celnav`): AHG y declinación del Sol, AHG de Aries, AHS y declinación de las 57 estrellas de navegación y la Polar, semidiámetro y ecuación del tiempo, con precisión de almanaque (0,1′; contrastado con un almanaque náutico de 2026 y con PyEphem). El Sol de todo el plotter (orto, ocaso, Meteo) usa ya ese cálculo (antes 0,2′–0,4′ de error).
- **Instrumentos > Motor** (tocando otra vez la pestaña): régimen, refrigerante, presión y temperatura de aceite, combustible, batería, consumo y horas. Lector de NMEA 2000 127489 (fast packet), 127505 y la sobrealimentación de 127488, y de un motor **J1939** en el mismo bus (61444, 61443, 65262, 65263, 65266, 65270, 65271, 65276, 65253). Sin probar con un bus real.
- **Formatos contrastados con CANboat v9.2.0 en la CI** (`tools/canboat/run.sh`): codificadores 130306 / 129283 / 129284 y lector de motor NMEA 2000 / J1939.
- **Distintivo marítimo comprobado** con MarineTraffic (clave de su API; núcleo `mtcheck`, `src/maritime.*`) o a mano en el MARS de la UIT; se manda al transponder (`mcok`).
- **Distintivo marítimo** del MMSI en Ajustes > Embarcación: se manda al transponder (campo `mc` de `vessel`), que lo pone en los datos estáticos AIS.
- **Validación del CALLSIGN en RadioID.net** (como en la app): Ajustes > Embarcación, DMR ID / CCS7 y *Validar*. Sin CALLSIGN validado no se mandan los datos del barco al transponder. Se guarda y se revisa cada 90 días. Núcleo `callsign` (mismas pruebas que `CallsignCheck` de la app); `net::requestGet` / `net::takeGet` en `lib/aprs_links`.
- **Ajustes > Embarcación** (nombre, MMSI, indicativo APRS, tipo, eslora, manga y calado): se mantienen iguales en el **APRS LoRa+ MAPRS transponder** (o un tracker con el perfil náutico AIS) enlazado por Bluetooth, con la orden y el evento `vessel` de su servicio de control. El plotter manda; sin datos propios, toma los del transponder. Núcleo `vessel`; `ble::command` y `ble::readEvent` en `lib/aprs_links` (copiar a la Display y al rotor). El botón de red pasa a llamarse «Red: WiFi y TCP».
- **Avisos oficiales por la WiFi** (`lib/aprs_links/alerts_net`, tarea propia): AEMET (RSS de Meteoalerta), MeteoAlarm (por país), NWS (EE. UU.), tsunami.gov, terremotos (USGS, EMSC) y tiempo espacial (SWPC). Ventana como las demás alertas y resumen en la pestaña Meteo. *Ajustes > Red*: zonas y país. Núcleo: `alerts`, `minijson`, `same` (NOAA Weather Radio).
- Registro de arranque con la línea común del ecosistema: «[INFO][Main] APRS LoRa+ (c) 2026 Manuel - EA7JRS».

## 5.4.4 — 2026-11-12
- Solo el número de versión (misma versión en todo el ecosistema; las novedades de la 5.4.4 son del tracker y la app).

## 5.4.3 — 2026-11-12
_La 5.4.2 no llegó a publicarse (solo una versión preliminar del plotter): todo lo preparado para ella sale en la 5.4.3._

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
