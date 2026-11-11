/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Enlace BLE con la pasarela (XIAO): el plotter es el central, busca un equipo que anuncie el servicio de datos NMEA,
 * se conecta y recibe lineas NMEA 0183 por notificaciones. Protocolo en docs/BLE_NMEA.md. Va en una tarea aparte.
 */
#pragma once
#include <Arduino.h>

namespace ble {

enum State { OFF = 0, SCANNING = 1, CONNECTING = 2, CONNECTED = 3 };

struct Status {
    int state = OFF;
    char name[24] = "";                 // nombre del equipo conectado (o del ultimo)
    char addr[18] = "";                 // su direccion, "aa:bb:cc:dd:ee:ff"
    int rssi = 0;
    uint32_t bytes = 0;                 // recibidos
    uint32_t connects = 0;              // conexiones logradas
};

// on = false: desconecta y deja de buscar. addr: direccion del equipo ya conocido ("" = el primero que aparezca).
void set(bool on, const char* addr);
const Status& status();
int read(uint8_t* buf, int max);        // bytes recibidos desde la ultima llamada
}
