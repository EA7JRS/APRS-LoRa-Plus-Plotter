# APRS LoRa+ Plotter — contexto del proyecto

Propietario: Manuel (EA7JRS). Idioma de trabajo: español, respuestas breves y directas.
Los archivos nuevos llevan la cabecera `Copyright (C) 2026 Manuel - EA7JRS` y licencia GPL-3.0-or-later.
No publicar nada ni crear repositorios sin que lo pida Manuel.

## Qué es
Plotter náutico para la Waveshare ESP32-S3-Touch-LCD-7B (1024×600), inspirado en las funciones del Garmin GPSMAP x3
(manual de usuario estudiado el 2026-11-11), con diseño propio. Repositorio aparte (privado) para integrarlo después en
`EA7JRS/APRS-LoRa-Plus-Display`, del que reutiliza los drivers de la 7B (pantalla, táctil GT911, expansor de pines),
LVGL ~9.6, NimBLE-Arduino ^2.5.2 y ArduinoJson 7.4.3.
Maquetas (lienzo Design, cuatro páginas por fases): https://claude.ai/artifact/3rGbM86ZobibCQ2KU44DAD

## Decisiones (Manuel, 2026-11-11)
- Solo placa 7B (Manuel tiene la LCD-7 rev 1.2 de 800×480; no hay entorno para ella de momento).
- Datos: NMEA 2000 por el CAN de la placa (pasarela XIAO de `APRS-LoRa-Plus-NMEA2000`, transpondedores Garmin AIS 600 / 800),
  serie (receptor AIS Comar SLR200G a 38400; emisoras con LSD Icom IC-M423GE / IC-M330GE, Garmin, Standard Horizon GX),
  TCP en la red local (Signal K, gpsd, AIS-catcher, Navtex, SafetyNET), AISHub (servicio web, 1 consulta/min), BLE con el tracker
  APRS LoRa+ y datos de internet (OpenWeatherMap, IQAir, AEMET, Portus). Prioridad: sensores, CAN, serie, TCP, BLE, internet.
- En la 7B el USB nativo comparte pines con el CAN: sin AIS por USB.
- Mapas base: OpenSeaMap por defecto; IHM (WMS cartaENCp2..p5, solo visualización, «no válida para la navegación»), IGN, Esri con
  clave, relieve del fondo (fuente por confirmar: EMODnet; el MBAR24 del IHM es CC BY-NC).
- Capas: Portus (oleaje, viento, corrientes, nivel, temperatura, boyas), radar de lluvia de AEMET con su escala de colores,
  rayos, avisos y predicción marítima de AEMET. Windy y Windfinder descartados.
- Emergencias: la misma ventana que la app Android (`AlertOverlay.kt`, `AlertCenter.kt`), más «DSC DISTRESS» (falta en la app).
- Viento: anemómetro de cazoletas con un imán (un pulso por vuelta, radio ~4 cm): factor inicial 1,2 kn por Hz, sin calibrar,
  rebote 5 ms. Sin veleta: la dirección sale de internet y los ángulos se marcan como estimados; el modo Viento del piloto
  queda bloqueado. Beaufort junto a la velocidad.
- Unidades: náutico (profundidad en metros), métrico, imperial y usuario.
- Piloto automático: solo se da por viable enviar datos de ruta; los mandos dependen de la marca (pendiente saber cuál).
- Declinación: Modelo Magnético Mundial calculado a bordo, con descarga del archivo de coeficientes cuando caduque.
- Fase 4: mareas (constantes por puerto; mirar el WMS de mareas del IHM) y rectas de altura (Meeus, contrastar con el Almanaque).
- Librerías: versiones fijadas y una tarea semanal que proponga actualizaciones tras compilar.

- Tomado de OpenCPN (estudiado el 2026-11-11; ideas y formatos, no código): GPX para waypoints, rutas y tracks; línea de costa
  mundial ligera como fondo de reserva; reproducción de archivos NMEA (simulador y repaso de travesías); datos de mareas
  (`data/tcdata` de OpenCPN: `ticon-europe-global.tcd`, 1690 estaciones; confirmar puertos españoles y licencia); filtros y
  prioridad por conexión con monitor de tramas; blanco AIS perdido, ocultar amarrados y silenciar por buque. GRIB, más adelante.
- Datos de la embarcación (nombre, MMSI, distintivo, tipo, dimensiones, antena GPS, sonda, depósitos): solo para el plotter.
- AIS en la carta con iconos propios por tipo de buque (no los de MarineTraffic). AISHub: zona visible, 1 consulta/min, atenuado
  y sin alarma de colisión. Alarma de garreo con pantalla de fondeo. Tablilla de desvíos (automática con compás electrónico;
  pendiente saber si Manuel tiene uno).

- AIS Dispatcher de AISHub (estudiado el 2026-11-11): va en la Raspberry para ser estación y obtener la clave de la API (el Comar
  saca a la vez NMEA para la pantalla y RS-232 para la Raspberry). El plotter añade entrada por UDP en la fase 1; reenviar él mismo
  a AISHub queda como opción secundaria (una estación debe estar siempre encendida).

- AIS-catcher (jvde-github, GPL-3.0; estudiado el 2026-11-11). Fase 1, hecho en el núcleo: comprobación de posiciones (saltos
  imposibles), contadores de calidad, mensajes 4, 9, 12, 14, 21 y 27 además de los de buques, dimensiones / calado / destino, y
  bloques de etiquetas NMEA (hora y estación). Fase 3: entrada JSON de AIS-catcher (nivel de señal por barco) y su API
  `/api/v1/vessels` para cargar la lista al arrancar. Su tabla de PGN AIS (129038, 129039, 129040, 129041, 129802, 129809, 129810)
  es la referencia para NMEA 2000. **Fuera** (Manuel): demodulación SDR, visor web, base de datos y catálogo de puertos.

- OpenPlotter (estudiado el 2026-11-11; Raspberry + Signal K, apps en Python, no se porta código). Aceptado por Manuel: entradas
  de pulsos genéricas (corredera de paletas, cuentavueltas), sondas DS18B20, entradas digitales con alarma (sentina, puerta),
  niveles de aviso de Signal K (nominal / normal / alert / warn / alarm / emergency) y más sensores I2C (BME280 / BMP280 / BME680,
  INA260, BH1750 para el brillo). Núcleo hecho en `inputs`; los drivers I2C y 1-Wire van con el firmware. **Fuera**: gestión de
  programas de la Raspberry y recepción SDR.
- Pypilot (pypilot/pypilot, estudiado el 2026-11-11): piloto abierto en Raspberry con IMU y controlador de motor. Servidor NMEA en
  TCP 20220: manda HDM (rumbo magnético), XDR PTCH / ROLL, RSA (timón) y MWV; acepta APB para seguir la ruta (toma el rumbo a
  gobernar y el error de rumbo, que limita a 0,15 mn). Puerto de control propio en 23322 (JSON; modos, rumbo, embragar), sin
  estudiar a fondo. El núcleo ya lee HDM / HDG, XDR y RSA y genera APB (`nmeaApb`). Sirve como compás para la tablilla de desvíos.

## Estado (2026-11-11)
- `lib/plotter_core`: geo, motion, wind, units, astro, store, nmea, ais, gpx, stream (líneas y tramas repetidas), inputs (pulsos, interruptores, DS18B20, niveles de aviso). 966 comprobaciones en `test/host/test_core.cpp`
  (`tools/run_host_tests.sh`), con valores de referencia publicados (Vincenty: Flinders Peak - Buninyong; AIS: ejemplos de gpsd).
- Sin firmware todavía: falta `platformio.ini`, los drivers de la 7B y la interfaz LVGL.
- `astro` usa la serie de NOAA (~1 min); las pruebas del sol son de coherencia, no contra un almanaque.

## Siguiente
1. Esqueleto del firmware: `platformio.ini`, drivers de la 7B copiados de Display, LVGL, pantalla de Instrumentos con simulador NMEA.
2. NMEA 2000 por TWAI (librería NMEA2000) y UART / RS485.
3. Carta con teselas en microSD y costa de reserva, waypoints y rutas (GPX); radar AIS; alarmas; zumbador externo; reproducción NMEA.
4. Pendiente de Manuel: marca del piloto automático; comprobar pines libres de la 7B para el anemómetro.
