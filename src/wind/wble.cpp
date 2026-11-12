/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "wble.h"

#ifdef ESP32
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#endif

namespace wble {
static Status st;

#ifdef ESP32
static const char* const kService = "4c6f5261-2b00-4ea7-b100-000000000001";
static const char* const kCommands = "4c6f5261-2b00-4ea7-b100-000000000002";
static const char* const kData = "4c6f5261-2b00-4ea7-b100-000000000003";
static NimBLEServer* server = nullptr;
static NimBLECharacteristic* chData = nullptr;
static bool started = false;
static StreamBufferHandle_t inBuf = nullptr;                   // lo que escribe el plotter: rumbo y velocidad en NMEA

struct CmdCb : NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        NimBLEAttValue v = c->getValue();
        if (!v.size() || !inBuf) return;
        xStreamBufferSend(inBuf, v.data(), v.size(), 0);          // si el bucle no da abasto, se pierde
        st.received += (uint32_t)v.size();
    }
};
static CmdCb cmdCb;

struct ServerCb : NimBLEServerCallbacks {
    void onConnect(NimBLEServer*, NimBLEConnInfo&) override { st.connected = true; }
    void onDisconnect(NimBLEServer* s, NimBLEConnInfo&, int) override { st.connected = s->getConnectedCount() > 0; }
};
static ServerCb serverCb;

static void start() {
    if (!started) {
        NimBLEDevice::init("APRS LoRa+ Wind");
        NimBLEDevice::setMTU(247);
        server = NimBLEDevice::createServer();
        server->setCallbacks(&serverCb, false);
        server->advertiseOnDisconnect(true);
        NimBLEService* svc = server->createService(kService);
        chData = svc->createCharacteristic(kData, NIMBLE_PROPERTY::NOTIFY);
        // el plotter escribe aqui rumbo y velocidad del barco (HDT, VTG, VHW) para el viento real
        NimBLECharacteristic* cmd = svc->createCharacteristic(kCommands, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        cmd->setCallbacks(&cmdCb);
        if (!inBuf) inBuf = xStreamBufferCreate(1024, 1);
        svc->start();
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        adv->addServiceUUID(kService);                                        // el plotter filtra por el servicio
        adv->enableScanResponse(true);
        adv->setName("APRS LoRa+ Wind");
        started = true;
    }
    NimBLEDevice::getAdvertising()->start();
    Serial.println("[ble] servicio NMEA anunciado");
}
#endif

void set(bool on) {
    if (on == st.on) return;
    st.on = on;
#ifdef ESP32
    if (on) start();
    else if (started) {
        NimBLEDevice::getAdvertising()->stop();
        for (uint16_t h : server->getPeerDevices()) server->disconnect(h);
        st.connected = false;
    }
#endif
}

bool send(const char* text, int len) {
#ifdef ESP32
    if (!st.on || !st.connected || !chData || len <= 0) return false;
    bool any = false;
    // a cada central por su lado, en trozos que quepan en su MTU (el plotter junta las lineas)
    for (uint16_t h : server->getPeerDevices()) {
        int room = (int)server->getPeerMTU(h) - 3;
        if (room < 20) room = 20;
        bool ok = true;
        for (int off = 0; off < len && ok; off += room) {
            int n = len - off < room ? len - off : room;
            ok = chData->notify((const uint8_t*)text + off, (size_t)n, h);
        }
        any = any || ok;
    }
    if (any) st.sent++;
    return any;
#else
    (void)text; (void)len;
    return false;
#endif
}

const Status& status() { return st; }
int read(uint8_t* buf, int max) {
#ifdef ESP32
    return inBuf ? (int)xStreamBufferReceive(inBuf, buf, (size_t)max, 0) : 0;
#else
    (void)buf; (void)max;
    return 0;
#endif
}
}
