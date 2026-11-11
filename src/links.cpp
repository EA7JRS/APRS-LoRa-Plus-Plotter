/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "links.h"

#include "board.h"
#include "sim.h"
#include "stream.h"

namespace links {

using namespace plotter;

static DataStore* store = nullptr;
static Status st;
static NmeaSimulator sim;
static Nmea0183 simNmea, serialNmea;
static LineBuffer serialLine;
static AisReport ais;
static uint32_t simLast = 0;

void begin(DataStore* s) {
    store = s;
    // El simulador entra como "sensor": mientras este encendido manda sobre lo demas, para ver la interfaz sin nada conectado
    Serial1.setRxBufferSize(2048);
    Serial1.begin(st.serialBaud, SERIAL_8N1, board::PIN_RS485_RX, board::PIN_RS485_TX);
    Serial.printf("[links] RS485 a %u baudios (RX %d, TX %d); simulador %s\n", (unsigned)st.serialBaud, board::PIN_RS485_RX,
                  board::PIN_RS485_TX, st.simulator ? "encendido" : "apagado");
}

void setSimulator(bool on) { st.simulator = on; }

void setSerialBaud(uint32_t baud) {
    if (baud == st.serialBaud) return;
    st.serialBaud = baud;
    Serial1.updateBaudRate(baud);
}

const Status& status() { return st; }
const NmeaStats& serialStats() { return serialNmea.stats(); }
const AisReport& lastAis() { return ais; }

void loop() {
    if (!store) return;
    uint32_t now = millis();

    int budget = 512;                                              // sin acaparar el bucle si llega una rafaga
    while (budget-- > 0 && Serial1.available()) {
        if (!serialLine.push((char)Serial1.read())) continue;
        st.serialLines++;
        st.serialLastMs = now;
        if (serialNmea.feed(serialLine.line(), Source::Serial, now, *store) == NmeaResult::Ais) {
            ais = serialNmea.ais();
            st.aisMessages++;
        }
    }
    serialNmea.sky().expire(now);

    if (st.simulator && now - simLast >= 1000) {
        simLast = now;
        sim.step(now);
        char line[100];
        for (int i = 0; i < NmeaSimulator::count(); i++)
            if (sim.sentence(i, line, sizeof(line))) simNmea.feed(line, Source::Sensor, now, *store);
    }
}

}
