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

## Estado (2026-11-11)
- `lib/plotter_core`: geo, motion, wind, units, astro, store, nmea, ais. 214 comprobaciones en `test/host/test_core.cpp`
  (`tools/run_host_tests.sh`), con valores de referencia publicados (Vincenty: Flinders Peak - Buninyong; AIS: ejemplos de gpsd).
- Sin firmware todavía: falta `platformio.ini`, los drivers de la 7B y la interfaz LVGL.
- `astro` usa la serie de NOAA (~1 min); las pruebas del sol son de coherencia, no contra un almanaque.

## Siguiente
1. Esqueleto del firmware: `platformio.ini`, drivers de la 7B copiados de Display, LVGL, pantalla de Instrumentos con simulador NMEA.
2. NMEA 2000 por TWAI (librería NMEA2000) y UART / RS485.
3. Carta con teselas en microSD, waypoints y rutas; radar AIS; alarmas; zumbador externo.
4. Pendiente de Manuel: marca del piloto automático; comprobar pines libres de la 7B para el anemómetro.
