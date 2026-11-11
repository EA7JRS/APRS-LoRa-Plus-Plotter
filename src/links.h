/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Entradas de datos del plotter. Fase 1: simulador y puerto serie (RS485 de la placa) con NMEA 0183 / AIS.
 * Lo siguiente (NMEA 2000 por CAN, TCP y UDP por WiFi) entra por aqui mismo.
 */
#pragma once
#include <Arduino.h>

#include "ais.h"
#include "aistargets.h"
#include "nmea.h"
#include "store.h"

namespace links {

struct Status {
    bool simulator = true;
    uint32_t serialBaud = 38400;
    uint32_t serialLines = 0;           // lineas recibidas por el puerto serie
    uint32_t serialLastMs = 0;          // millis() de la ultima
    uint32_t aisMessages = 0;
    bool can = false;                   // NMEA 2000 por el CAN de la placa
    uint32_t canFrames = 0;             // tramas CAN recibidas
    uint32_t canMessages = 0;           // mensajes NMEA 2000 reconocidos
    uint32_t canLastMs = 0;
    uint32_t tcpLines = 0, udpLines = 0;   // lineas NMEA recibidas por la red
    uint32_t netLastMs = 0;
    char wxPlace[32] = "";              // ultima observacion de internet: lugar y cuando (0 = ninguna)
    uint32_t wxMs = 0;
    uint32_t pilotSent = 0;             // tramas enviadas al piloto
};

void begin(plotter::DataStore* store);
void loop();                            // llamar a menudo
void setSimulator(bool on);
// Datos de ruta para el piloto: via 1 = RS485, 2 = servidor TCP. Devuelve false si esa via no esta disponible.
bool sendPilot(const char* sentence, int len, int via);
void applyNetwork();                    // WiFi, TCP y UDP segun los ajustes guardados
void setCan(bool on);                   // NMEA 2000 (250 kbit/s, solo escucha); quita el USB nativo de los pines 19 / 20
void injectCanFrame(uint32_t canId, const uint8_t* data, int len);   // pruebas
void setSerialBaud(uint32_t baud);      // 4800 (NMEA clasico) o 38400 (AIS)
const Status& status();
const plotter::NmeaStats& serialStats();
// Blancos AIS (del puerto serie y del simulador), con distancia y CPA respecto al barco: se recalculan cada segundo
plotter::AisTargets& targets();
}
