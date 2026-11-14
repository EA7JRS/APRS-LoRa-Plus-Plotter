/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ajustes que se guardan entre arranques (memoria NVS del ESP32).
 */
#pragma once
#include <Arduino.h>

namespace settings {
struct Values {
    bool simulator = true;          // datos de prueba mientras no haya nada conectado
    uint32_t serialBaud = 38400;    // RS485: 38400 (AIS) o 4800 (NMEA clasico)
    int brightness = 100;           // luz de fondo, 10..100
    bool can = true;                // NMEA 2000 por el CAN (los pines dejan de ser el USB nativo)
    char wifiSsid[33] = "";        // red: WiFi, servidor TCP de datos y escucha UDP
    char wifiPass[65] = "";
    char tcpHost[40] = "";
    int tcpKind = 0;                // 0 apagado, 1 NMEA 0183, 2 gpsd, 3 Signal K
    int tcpPort = 10110;
    int udpPort = 0;                // 0 = sin escucha
    char owmKey[40] = "";          // clave de OpenWeatherMap (datos de reserva de internet)
    bool alertsOn = true;           // avisos oficiales por la WiFi (meteorologia, tsunamis, terremotos, tiempo espacial)
    char alertZones[64] = "";      // zonas (MeteoAlarm / RSS de AEMET): palabras separadas por comas; vacio = todas
    char alertCountry[3] = "";     // pais ISO (vacio = por la posicion: Espana y EE. UU.)
    bool bleOn = false;             // enlace BLE con la pasarela
    int blePin = 123456;            // PIN de control del tracker APRS LoRa+ (para recibir su NMEA por BLE)
    char bleAddr[18] = "";          // equipo recordado ("" = el primero que aparezca)
    int pilotOut = 0;               // datos de ruta al piloto: 0 no, 1 RS485, 2 servidor TCP, 3 Bluetooth (pasarela XIAO)
    int pilotModel = 1;             // 0 generico (APB + RMB), 1 Raymarine ST2000+ de Manuel (RMB + APB con waypoints de 4 letras, VWR, VHW)
    int units = 0;                  // 0 nautico, 1 metrico, 2 imperial
    int baseMap = 0;                // 0 OpenSeaMap, 1 IHM, 2 IGN, 3 ortofoto (carpetas de la tarjeta)
    bool anchorOn = false;          // fondeo vigilado: sigue tras un reinicio
    double anchorLat = 0, anchorLon = 0;
    int anchorRadius = 40;          // radio de alarma, m
    int anchorRode = 30;            // cadena filada, m
    // Embarcacion propia (Ajustes > Embarcacion); se sincroniza con el transpondedor APRS LoRa+ MAPRS por BLE
    char vesselName[21] = "";
    uint32_t vesselMmsi = 0;
    char vesselCall[10] = "";       // indicativo APRS sin SSID
    char vesselMCall[8] = "";       // distintivo de llamada maritimo del MMSI
    int vesselMcOk = 0;             // distintivo comprobado: 0 no, 1 MarineTraffic, 2 MARS de la UIT (a mano)
    char vesselMcPair[24] = "";     // MMSI/distintivo comprobado ("224123456/EBXY")
    char mtKey[48] = "";            // clave de la API de MarineTraffic (40 hex)
    int vesselType = 0;             // tipo del perfil nautico AIS (0 velero de recreo, 1 lancha de recreo...)
    int vesselLen = 0;              // eslora, m
    int vesselBeam = 0;             // manga, m
    int vesselDraftDm = 0;          // calado, decimas de metro
    // Indicativo validado en RadioID.net (DMR ID / CCS7), como la app: sin el, no se mandan datos al transponder
    char licCall[10] = "";
    char licIds[64] = "";
    char licName[48] = "";
    char licCountry[32] = "";
    uint32_t licAt = 0;             // hora UNIX de la validacion (0 = sin hora)
};
Values& get();
void load();
void save();
}
