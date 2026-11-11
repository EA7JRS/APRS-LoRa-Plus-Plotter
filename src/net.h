/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Red: WiFi (cliente), un servidor TCP de datos (NMEA 0183 o gpsd) y escucha por UDP. Todo en una tarea aparte, para que
 * una red caida o un servidor que no contesta no paren la pantalla; los bytes recibidos se recogen desde el bucle principal.
 */
#pragma once
#include <Arduino.h>

namespace net {

enum TcpKind { TCP_OFF = 0, TCP_NMEA = 1, TCP_GPSD = 2, TCP_KINDS = 3 };

struct Config {
    char ssid[33] = "";
    char pass[65] = "";
    char host[40] = "";
    int tcpKind = TCP_OFF;
    int tcpPort = 10110;
    int udpPort = 0;                    // 0 = sin escucha
};

struct Status {
    bool wifi = false;
    char ip[16] = "";
    int rssi = 0;
    bool tcp = false;
    uint32_t tcpBytes = 0, udpBytes = 0;
};

void begin();
void apply(const Config& c);            // cambia la configuracion: corta lo que hubiera y vuelve a conectar
const Status& status();
int readTcp(uint8_t* buf, int max);     // bytes recibidos desde la ultima llamada (0 si no hay)
int readUdp(uint8_t* buf, int max);     // cada datagrama acaba en salto de linea
}
