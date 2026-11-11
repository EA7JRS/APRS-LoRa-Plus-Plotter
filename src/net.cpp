/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "net.h"

#ifdef ESP32
#include <WiFi.h>
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include "gnss.h"
#endif

namespace net {

static Status st;
const Status& status() { return st; }

#ifdef ESP32

static Config cfg;
static volatile uint32_t cfgVersion = 0;
static SemaphoreHandle_t cfgLock = nullptr;
static StreamBufferHandle_t sbTcp = nullptr, sbUdp = nullptr, sbOut = nullptr;

static void task(void*) {
    Config c;
    uint32_t seen = 0xFFFFFFFFu, lastBegin = 0, lastTry = 0, lastInfo = 0;
    bool wifiStarted = false, udpOn = false;
    WiFiClient client;
    WiFiUDP udp;
    static uint8_t buf[1024];
    for (;;) {
        uint32_t now = millis();
        if (seen != cfgVersion) {                                  // configuracion nueva: se empieza de cero
            xSemaphoreTake(cfgLock, portMAX_DELAY);
            c = cfg;
            seen = cfgVersion;
            xSemaphoreGive(cfgLock);
            client.stop();
            if (udpOn) { udp.stop(); udpOn = false; }
            if (wifiStarted) WiFi.disconnect(true);
            wifiStarted = false;
            st.wifi = st.tcp = false;
            st.ip[0] = 0;
            lastBegin = lastTry = 0;
        }
        if (!c.ssid[0]) { vTaskDelay(pdMS_TO_TICKS(300)); continue; }
        if (WiFi.status() != WL_CONNECTED) {
            if (st.wifi) { st.wifi = st.tcp = false; st.ip[0] = 0; client.stop(); if (udpOn) { udp.stop(); udpOn = false; } }
            if (!wifiStarted || now - lastBegin > 20000) {         // reintento cada 20 s
                WiFi.mode(WIFI_STA);
                WiFi.setAutoReconnect(true);
                WiFi.begin(c.ssid, c.pass);
                wifiStarted = true;
                lastBegin = now;
            }
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        if (!st.wifi || now - lastInfo > 5000) {
            lastInfo = now;
            snprintf(st.ip, sizeof(st.ip), "%s", WiFi.localIP().toString().c_str());
            st.rssi = WiFi.RSSI();
            st.wifi = true;
        }
        // UDP: lo que otros equipos manden a este puerto (AIS de AIS-catcher o AIS Dispatcher, NMEA de un multiplexor)
        if (c.udpPort > 0 && !udpOn) udpOn = udp.begin((uint16_t)c.udpPort) == 1;
        if (udpOn) {
            int n;
            while ((n = udp.parsePacket()) > 0) {
                int got = udp.read(buf, sizeof(buf) - 1);
                if (got <= 0) break;
                if (buf[got - 1] != '\n') buf[got++] = '\n';
                xStreamBufferSend(sbUdp, buf, (size_t)got, 0);     // si el bucle principal no da abasto, se pierde
                st.udpBytes += (uint32_t)got;
            }
        }
        // TCP: un servidor de la red local
        if (c.tcpKind != TCP_OFF && c.host[0] && c.tcpPort > 0) {
            if (!client.connected()) {
                st.tcp = false;
                if (lastTry == 0 || now - lastTry > 10000) {       // reintento cada 10 s
                    lastTry = now ? now : 1;
                    if (client.connect(c.host, (uint16_t)c.tcpPort, 3000)) {
                        client.setNoDelay(true);
                        if (c.tcpKind == TCP_GPSD) client.print(plotter::kGpsdWatchNmea);   // gpsd: que mande el NMEA tal cual
                        st.tcp = true;
                    }
                }
            } else {
                st.tcp = true;
                size_t out = xStreamBufferReceive(sbOut, buf, sizeof(buf), 0);       // lo que haya para enviar
                if (out) client.write(buf, out);
                int avail = client.available();
                while (avail > 0) {
                    int got = client.read(buf, avail > (int)sizeof(buf) ? sizeof(buf) : (size_t)avail);
                    if (got <= 0) break;
                    xStreamBufferSend(sbTcp, buf, (size_t)got, 0);
                    st.tcpBytes += (uint32_t)got;
                    avail = client.available();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void begin() {
    if (cfgLock) return;
    cfgLock = xSemaphoreCreateMutex();
    sbTcp = xStreamBufferCreate(8192, 1);
    sbUdp = xStreamBufferCreate(4096, 1);
    sbOut = xStreamBufferCreate(1024, 1);
    xTaskCreatePinnedToCore(task, "net", 6144, nullptr, 1, nullptr, 0);
}

void apply(const Config& c) {
    if (!cfgLock) begin();
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    cfg = c;
    cfgVersion = cfgVersion + 1;
    xSemaphoreGive(cfgLock);
}

int readTcp(uint8_t* b, int max) { return sbTcp ? (int)xStreamBufferReceive(sbTcp, b, (size_t)max, 0) : 0; }
void sendTcp(const char* d, int len) { if (sbOut && st.tcp && len > 0) xStreamBufferSend(sbOut, d, (size_t)len, 0); }
int readUdp(uint8_t* b, int max) { return sbUdp ? (int)xStreamBufferReceive(sbUdp, b, (size_t)max, 0) : 0; }

#else   // en el PC no hay red

void begin() {}
void apply(const Config&) {}
int readTcp(uint8_t*, int) { return 0; }
void sendTcp(const char*, int) {}
int readUdp(uint8_t*, int) { return 0; }

#endif

}
