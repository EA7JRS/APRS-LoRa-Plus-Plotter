/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "wnet.h"

#include "wsettings.h"

#ifdef ESP32
#include <WiFi.h>
#include <WiFiUdp.h>
#endif

namespace wnet {
static Status st;
#ifdef ESP32
static WiFiUDP udp;
static uint32_t lastTry = 0;
#endif

void apply() {
    const wsettings::Values& v = wsettings::get();
    st.on = v.wifiOn && v.wifiSsid[0];
#ifdef ESP32
    WiFi.disconnect(true);
    st.connected = false;
    if (!st.on) { WiFi.mode(WIFI_OFF); return; }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(v.wifiSsid, v.wifiPass);
    lastTry = millis();
    Serial.printf("[wifi] uniendo a \"%s\"\n", v.wifiSsid);
#endif
}

void loop() {
#ifdef ESP32
    if (!st.on) return;
    bool c = WiFi.status() == WL_CONNECTED;
    if (c != st.connected) {
        st.connected = c;
        if (c) {
            snprintf(st.ip, sizeof(st.ip), "%s", WiFi.localIP().toString().c_str());
            Serial.printf("[wifi] conectado, %s\n", st.ip);
        } else st.ip[0] = 0;
    }
    if (c) st.rssi = WiFi.RSSI();
    else if (millis() - lastTry > 20000) {                   // la reconexion automatica a veces no vuelve: otra vez
        lastTry = millis();
        WiFi.disconnect();
        WiFi.begin(wsettings::get().wifiSsid, wsettings::get().wifiPass);
    }
#endif
}

bool send(const char* text, int len) {
#ifdef ESP32
    if (!st.connected || len <= 0) return false;
    if (!udp.beginPacket(IPAddress(255, 255, 255, 255), (uint16_t)wsettings::get().udpPort)) return false;
    udp.write((const uint8_t*)text, (size_t)len);
    if (!udp.endPacket()) return false;
    st.sent++;
    return true;
#else
    (void)text; (void)len;
    return false;
#endif
}

const Status& status() { return st; }
}
