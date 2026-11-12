/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Ajustes del instrumento de viento (NVS, espacio "wind").
#pragma once
#include <Arduino.h>

namespace wsettings {
struct Values {
    bool simulator = true;          // datos de prueba mientras no haya veleta conectada
    int brightness = 100;           // luz de fondo, 20..100
    int roseStyle = 2;              // pagina Rosa: 0 banda, 1 dos bandas, 2 columna
    bool rs485Swap = false;         // intercambia RX y TX del RS485 (la tabla de Waveshare esta sin verificar)
    bool bleOn = true;              // anuncia el servicio NMEA para el plotter
    bool can = true;                // NMEA 2000: escucha velocidad y rumbo (para el viento real)
    bool canOut = true;             // y emite el viento (PGN 130306) con direccion reclamada
    int vaneOffset = 0;             // correccion de la veleta, grados (se suma al angulo aparente)
    bool wifiOn = false;            // WiFi: manda el viento por UDP a la red
    char wifiSsid[33] = "";
    char wifiPass[65] = "";
    int udpPort = 10110;
    bool night = false;             // modo noche: todo en rojos sobre negro
    int twsMax = 0;                 // alarmas, 0 = apagada: viento real y aparente (kn) y rolada (grados)
    int awsMax = 0;
    int shiftMax = 0;
};
Values& get();
void load();
void save();
}
