# APRS LoRa+ Plotter — Guía rápida

Versión 5.4.1 · Waveshare ESP32-S3-Touch-LCD-7B (7", 1024 × 600)

**Es una ayuda a la navegación. No sustituye a la carta oficial ni a la vigilancia.**
Esta primera versión compila, pero **todavía no se ha probado en la placa**: úsala para probar, no para navegar.

## 1. Puesta en marcha

Al encender sale un par de segundos la bienvenida de APRS LoRa+ (logo y versión) y después la pantalla de trabajo.

1. Instala el firmware con el instalador web: https://ea7jrs.github.io/APRS-LoRa-Plus/#nautica, grupo **Náutica**
   (más formas en `INSTALACION.md`).
2. Mete una tarjeta microSD en FAT32. Sin tarjeta funciona, pero la carta sale sin mapa y los waypoints no se guardan.
3. Enciende. Arranca con el **simulador** encendido: verás un barco y cinco blancos AIS de prueba.
4. Cuando conectes datos reales, apaga el simulador en **Ajustes > Simulador**: mientras está encendido, sus datos mandan.

## 2. La pantalla

- **Arriba**: nombre de la pestaña, posición, **Marcar** (guarda un waypoint donde estás) y **MOB** (hombre al agua).
- **Abajo**: ocho pestañas.

| Pestaña | Para qué |
|---|---|
| Instrumentos | Velocidad, rumbo, profundidad, viento y temperatura, con la fuente de cada dato |
| Carta | Mapa, barco, derrota, blancos AIS, waypoints, rutas, medir y fondeo |
| Rutas | Lista de waypoints y la ruta |
| Radar AIS | Blancos alrededor del barco, con riesgo de abordaje |
| Viento | Esfera de viento aparente y real, e historial de 10 minutos |
| Piloto | Rumbo a gobernar, desvío de la ruta y datos para el piloto automático |
| Meteo | Presión, temperatura, humedad, viento, racha y visibilidad |
| Ajustes | Simulador, puerto serie, brillo, unidades, mapa base, NMEA 2000, red, Bluetooth y «Acerca de» (versión, licencia, créditos y QR del donativo) |

## 3. Lo más usado

**Ir a un punto**: toca la carta → **Ir a**. Para parar: **Parar**.

**Guardar un waypoint**: **Marcar** (arriba) guarda tu posición. O toca la carta → **Marcar**.

**Hacer una ruta**: toca la carta → **Medir** → toca los puntos en orden → **A ruta**. Luego, en Rutas, **Navegar**.

**Medir una distancia**: toca la carta → **Medir** → toca el segundo punto. **Salir** para terminar.

**Fondear**: en la carta, **Fondeo** → ajusta la cadena con **− 5 m / + 5 m** → **Fondear** al largar el ancla.
Si el barco sale del círculo, salta la alarma. **Levar** quita la vigilancia.

**Hombre al agua**: **MOB**. Marca la posición y da distancia y demora para volver.

**Mover la carta**: arrastra. **+ / −** cambian el zoom. **Centrar barco** vuelve a seguir al barco.

## 4. Alertas

Salen en una ventana sobre cualquier pantalla:

| Ventana | Cuándo |
|---|---|
| COLLISION RISK | Un blanco AIS pasará a menos de 0,5 mn en menos de 20 min |
| MAN OVERBOARD | Has pulsado MOB |
| ANCHOR DRAG | El barco lleva 10 s fuera del círculo de fondeo |
| ANCHOR WATCH · NO GPS FIX | Fondeado y un minuto sin posición |
| SIGNAL K ALARM / EMERGENCY | Aviso del servidor Signal K |
| RS485 COMPARTIDO | La salida al piloto usa el mismo puerto por el que entran datos |

**En esta versión las alertas no suenan**: falta el zumbador.

## 5. Conectar datos

| Fuente | Dónde |
|---|---|
| AIS / NMEA 0183 por cable | Puerto RS485. Ajustes > Puerto serie (38400 o 4800) |
| NMEA 2000 | Puerto CAN. Ajustes > NMEA 2000 |
| Red local (NMEA, gpsd, Signal K) | Ajustes > Red |
| Móvil con la app APRS LoRa+ | Ajustes > Red: la WiFi del móvil, «TCP: NMEA 0183», servidor vacío, puerto 10110 |
| Tracker APRS LoRa+ o pasarela | Ajustes > Bluetooth (el PIN del tracker, en Ajustes > Red) |
| Tiempo de internet | Ajustes > Red: clave de OpenWeatherMap |
| Instrumento de viento APRS LoRa+ Wind | Solo: NMEA 2000; o Ajustes > Bluetooth; o Ajustes > Red, puerto UDP 10110 (`INSTRUMENTO_VIENTO.md`) |

Si un sensor deja de mandar, el plotter usa la siguiente fuente que tenga ese dato. Lo que viene de internet sale en
ámbar y con «estimado».

Más detalle en `MANUAL_USUARIO.md`.

APRS LoRa+ · © 2026 Manuel - EA7JRS · GPL-3.0-or-later
