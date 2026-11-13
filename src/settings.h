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
    int chartLayers = 1 | 2 | 16;   // capas de la carta: 1 avisos, 2 terremotos, 4 dia / noche, 8 NAVAREA, 16 marcas OpenSeaMap, 32 METAR / TAF, 64 SIGMET
    int seamarkOpa = 100;           // transparencia de las capas: % de opacidad, 20..100
    int nightOpa = 40;
    bool anchorOn = false;          // fondeo vigilado: sigue tras un reinicio
    double anchorLat = 0, anchorLon = 0;
    int anchorRadius = 40;          // radio de alarma, m
    int anchorRode = 30;            // cadena filada, m
};
Values& get();
void load();
void save();
}
