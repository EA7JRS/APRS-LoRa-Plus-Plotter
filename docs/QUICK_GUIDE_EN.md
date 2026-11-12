# APRS LoRa+ Plotter — Quick guide

Version 5.4.1 · Waveshare ESP32-S3-Touch-LCD-7B (7", 1024 × 600)

**This is an aid to navigation. It does not replace official charts or a proper lookout.**
This first version builds, but **it has not been tested on the board yet**: use it for testing, not for navigation.

The on-screen texts are in Spanish; this guide gives each one in Spanish with its meaning.

## 1. Getting started

On power-up the APRS LoRa+ welcome screen (logo and version) shows for a couple of seconds, then the working screen.

1. Install the firmware with the web installer: https://ea7jrs.github.io/APRS-LoRa-Plus/#nautica, **Náutica** group
   (other ways in `INSTALLATION_EN.md`).
2. Insert a FAT32 microSD card. It works without one, but the chart has no map and waypoints are not saved.
3. Power on. It starts with the **simulator** on: you will see a boat and five test AIS targets.
4. When you connect real data, turn the simulator off in **Ajustes > Simulador**: while it is on, its data takes priority.

## 2. The screen

- **Top bar**: tab name, position, **Marcar** (saves a waypoint where you are) and **MOB** (man overboard).
- **Bottom bar**: eight tabs.

| Tab | What it is for |
|---|---|
| Instrumentos (Instruments) | Speed, course, depth, wind and temperature, with the source of each value |
| Carta (Chart) | Map, boat, track, AIS targets, waypoints, routes, measuring and anchoring |
| Rutas (Routes) | Waypoint list and the route |
| Radar AIS | Targets around the boat, with collision risk |
| Viento (Wind) | Apparent and true wind dial, and a 10-minute history |
| Piloto (Pilot) | Course to steer, cross-track error and data for the autopilot |
| Meteo (Weather) | Pressure, temperature, humidity, wind, gust and visibility |
| Ajustes (Settings) | Simulator, serial port, brightness, units, base map, NMEA 2000, network, Bluetooth and «Acerca de» (About: version, licence, credits and donation QR) |

## 3. Everyday tasks

**Go to a point**: tap the chart → **Ir a** (Go to). To stop: **Parar**.

**Save a waypoint**: **Marcar** (top bar) saves your position. Or tap the chart → **Marcar**.

**Build a route**: tap the chart → **Medir** (Measure) → tap the points in order → **A ruta** (To route). Then, in
Rutas, **Navegar** (Navigate).

**Measure a distance**: tap the chart → **Medir** → tap the second point. **Salir** (Exit) to finish.

**Anchor**: on the chart, **Fondeo** (Anchoring) → set the rode with **− 5 m / + 5 m** → **Fondear** when you drop the
anchor. If the boat leaves the circle, the alarm is raised. **Levar** (Weigh) ends the watch.

**Man overboard**: **MOB**. It marks the position and gives distance and bearing back to it.

**Move the chart**: drag. **+ / −** change the zoom. **Centrar barco** (Centre boat) follows the boat again.

## 4. Alerts

They appear in a window over any screen:

| Window | When |
|---|---|
| COLLISION RISK | An AIS target will pass within 0.5 nm in less than 20 min |
| MAN OVERBOARD | You pressed MOB |
| ANCHOR DRAG | The boat has been outside the anchor circle for 10 s |
| ANCHOR WATCH · NO GPS FIX | Anchored and one minute without a position |
| SIGNAL K ALARM / EMERGENCY | Notification from the Signal K server |
| RS485 COMPARTIDO (shared RS485) | The pilot output uses the same port data is coming in on |

**In this version alerts make no sound**: the buzzer is not implemented yet.

## 5. Connecting data

| Source | Where |
|---|---|
| AIS / NMEA 0183 by cable | RS485 port. Ajustes > Puerto serie (38400 or 4800) |
| NMEA 2000 | CAN port. Ajustes > NMEA 2000 |
| Local network (NMEA, gpsd, Signal K) | Ajustes > Red |
| Phone with the APRS LoRa+ app | Ajustes > Red: the phone's Wi-Fi, «TCP: NMEA 0183», empty server, port 10110 |
| APRS LoRa+ tracker or gateway | Ajustes > Bluetooth (the tracker PIN goes in Ajustes > Red) |
| Internet weather | Ajustes > Red: OpenWeatherMap key |
| APRS LoRa+ Wind instrument | By itself on NMEA 2000; or Ajustes > Bluetooth; or Ajustes > Red, UDP port 10110 (`WIND_INSTRUMENT_EN.md`) |

If a sensor stops sending, the plotter uses the next source that has that value. Internet data is shown in amber and
labelled «estimado» (estimated).

More detail in `USER_MANUAL_EN.md`.

APRS LoRa+ · © 2026 Manuel - EA7JRS · GPL-3.0-or-later
