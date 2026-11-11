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
    int tcpKind = 0;                // 0 apagado, 1 NMEA 0183, 2 gpsd
    int tcpPort = 10110;
    int udpPort = 0;                // 0 = sin escucha
    int pilotOut = 0;               // datos de ruta al piloto: 0 no, 1 RS485, 2 servidor TCP
    int units = 0;                  // 0 nautico, 1 metrico, 2 imperial
    int baseMap = 0;                // 0 OpenSeaMap, 1 IHM, 2 IGN, 3 ortofoto (carpetas de la tarjeta)
    bool anchorOn = false;          // fondeo vigilado: sigue tras un reinicio
    double anchorLat = 0, anchorLon = 0;
    int anchorRadius = 40;          // radio de alarma, m
    int anchorRode = 30;            // cadena filada, m
};
Values& get();
void load();
void save();
}
