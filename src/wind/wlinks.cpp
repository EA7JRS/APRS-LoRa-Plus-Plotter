/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "wlinks.h"

#include <cmath>
#include <cstring>

#include "board.h"
#include "geo.h"
#include "motion.h"
#include "n2k.h"
#include "n2ktx.h"
#include "nmea.h"
#include "sim.h"
#include "stream.h"
#include "tsync.h"
#include "wble.h"
#include "wnet.h"
#include "wsettings.h"

#ifdef ESP32
#include "driver/twai.h"
#endif

namespace wlinks {

using namespace plotter;

static DataStore* store = nullptr;
static Status st;
static NmeaSimulator sim;
static Nmea0183 simNmea, serialNmea, bleNmea;
static LineBuffer serialLine, bleLine;
static N2kReader n2k;
static N2kAddressClaim* claim = nullptr;
[[maybe_unused]] static bool canRunning = false;

static uint64_t deviceName() {
    N2kName n;
#ifdef ESP32
    n.unique = (uint32_t)(ESP.getEfuseMac() & 0x1FFFFF);      // de la MAC: distinto en cada placa
#else
    n.unique = 0x12345;
#endif
    return n2kNameValue(n);
}

[[maybe_unused]] static void canTx(const N2kAddressClaim::Frame& f) {
#ifdef ESP32
    if (!canRunning) return;
    twai_message_t m = {};
    m.extd = 1;
    m.identifier = f.id;
    m.data_length_code = (uint8_t)f.len;
    memcpy(m.data, f.d, f.len);
    if (twai_transmit(&m, 0) == ESP_OK) st.canSent++;
    else st.canErrors++;
#else
    (void)f;
#endif
}

// NMEA 2000 a 250 kbit/s. Escucha siempre que este encendido; con canOut, ademas reclama direccion y emite el viento.
static void canStart() {
#ifdef ESP32
    if (canRunning) return;
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)board::PIN_CAN_TX, (gpio_num_t)board::PIN_CAN_RX, TWAI_MODE_NORMAL);
    g.rx_queue_len = 64;
    g.tx_queue_len = 8;
    twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (twai_driver_install(&g, &t, &f) != ESP_OK) { Serial.println("[links] CAN: no se pudo instalar el controlador"); return; }
    if (twai_start() != ESP_OK) { twai_driver_uninstall(); Serial.println("[links] CAN: no arranca"); return; }
    canRunning = true;
    Serial.printf("[links] NMEA 2000 a 250 kbit/s (TX %d, RX %d)%s\n", board::PIN_CAN_TX, board::PIN_CAN_RX,
                  st.canOut ? ", emite el viento" : ", solo escucha");
#endif
    if (st.canOut) {
        if (!claim) claim = new N2kAddressClaim(deviceName(), 35);
        N2kAddressClaim::Frame fr;
        claim->start(millis(), fr);
        canTx(fr);
        st.canAddress = claim->address();
    }
}

static void canStop() {
#ifdef ESP32
    if (canRunning) {
        twai_stop();
        twai_driver_uninstall();
        canRunning = false;
    }
#endif
    st.canAddress = -1;
}

[[maybe_unused]] static void canFrame(uint32_t id, const uint8_t* d, int len, uint32_t now) {
    st.canFrames++;
    st.canLastMs = now ? now : 1;
    if (claim && st.canOut) {
        N2kAddressClaim::Frame fr;
        if (claim->onFrame(id, d, len, now, fr)) canTx(fr);
        st.canAddress = claim->address();
    }
    n2k.feedFrame(id, d, len, Source::Can, now, *store);
    int64_t utc;
    if (n2k.takeUtc(utc)) tsync::offer(plotter::TimeSource::Gps, utc);   // GPS de la red NMEA 2000
}

static void canLoop(uint32_t now) {
#ifdef ESP32
    if (!canRunning) return;
    twai_message_t m;
    int budget = 64;
    while (budget-- > 0 && twai_receive(&m, 0) == ESP_OK) {
        if (!m.extd || m.rtr) continue;
        canFrame(m.identifier, m.data, m.data_length_code, now);
    }
    static uint32_t tCheck = 0;
    if (now - tCheck >= 2000) {
        tCheck = now;
        twai_status_info_t info;
        if (twai_get_status_info(&info) == ESP_OK) {
            if (info.state == TWAI_STATE_BUS_OFF) twai_initiate_recovery();
            else if (info.state == TWAI_STATE_STOPPED) twai_start();
        }
    }
#else
    (void)now;
#endif
}

void applySerialPins() {
    bool sw = wsettings::get().rs485Swap;
    int rx = sw ? board::PIN_RS485_TX : board::PIN_RS485_RX, tx = sw ? board::PIN_RS485_RX : board::PIN_RS485_TX;
    Serial1.end();
    Serial1.setRxBufferSize(1024);
    Serial1.begin(4800, SERIAL_8N1, rx, tx);
    Serial.printf("[links] veleta: RS485 a 4800 baudios (RX %d, TX %d)\n", rx, tx);
}

void begin(DataStore* s) {
    store = s;
    applySerialPins();
}

void setSimulator(bool on) { st.simulator = on; }

void setCan(bool listen, bool transmit) {
    bool was = canRunning || st.canAddress >= 0;
    st.can = listen;
    st.canOut = listen && transmit;
    if (was) canStop();
    if (listen) canStart();
}

Wind wind() {
    Wind w;
    uint32_t now = millis();
    double hdg = 0, stw = 0, sog = 0, cog = 0;
    w.app = store->get(Key::Awa, now, w.awa) && store->get(Key::Aws, now, w.aws);
    bool haveStw = store->get(Key::Stw, now, stw), haveSog = store->get(Key::Sog, now, sog);
    bool haveHdg = store->get(Key::Heading, now, hdg);
    if (w.app && (haveStw || haveSog) && haveHdg) {
        trueWind(w.awa, w.aws, haveStw ? stw : sog, hdg, w.twa, w.tws, w.twd);
        w.tru = true;
    }
    if (haveStw && haveSog && haveHdg && store->get(Key::Cog, now, cog)) {     // deriva: lo que el agua lleva al barco
        double dx = sog * sin(deg2rad(cog)) - stw * sin(deg2rad(hdg)), dy = sog * cos(deg2rad(cog)) - stw * cos(deg2rad(hdg));
        w.driftKn = sqrt(dx * dx + dy * dy);
        w.drift = true;
    }
    return w;
}

static int mwv(char* out, int size, double angle, char ref, double kn) {
    char body[48];
    snprintf(body, sizeof(body), "WIMWV,%.1f,%c,%.1f,N,A", norm360(angle), ref, kn);
    return snprintf(out, size, "$%s*%02X\r\n", body, nmeaChecksum(body));
}

// Viento al plotter, 4 veces por segundo (el NASA manda 10; sobra con 4): MWV aparente y, si hay rumbo y velocidad,
// MWV real, por Bluetooth y por WiFi; y por NMEA 2000 el PGN 130306 aparente y real (norte verdadero).
static void output(uint32_t now) {
    static uint32_t t = 0;
    static uint8_t sid = 0;
    if (now - t < 250) return;
    t = now;
    Wind w = wind();
    if (!w.app) return;
    char out[128];
    int n = mwv(out, sizeof(out), w.awa, 'R', w.aws);
    if (w.tru) n += mwv(out + n, sizeof(out) - n, w.twa, 'T', w.tws);
    wble::send(out, n);
    wnet::send(out, n);
    if (claim && st.canOut && claim->ready(now)) {
        sid = (uint8_t)((sid + 1) % 253);
        N2kAddressClaim::Frame f;
        f.len = 8;
        n2kWind(sid, w.aws, w.awa, 2, f.d);
        f.id = n2kMakeId(2, 130306, claim->address());
        canTx(f);
        if (w.tru) {
            n2kWind(sid, w.tws, w.twd, 0, f.d);
            canTx(f);
        }
    }
}

void loop() {
    uint32_t now = millis();
    bool vane = st.vaneLastMs && now - st.vaneLastMs < 5000;
    if (st.simulator && !vane) {                 // con la veleta enviando, el simulador se calla
        static uint32_t last = 0;
        if (now - last >= 200 || last == 0) {
            last = now ? now : 1;
            sim.step(now);
            char b[96];
            for (int i = 0; i < NmeaSimulator::count(); i++)
                if (sim.sentence(i, b, sizeof(b)) > 0) simNmea.feed(b, Source::Sensor, now, *store);
        }
    }
    int budget = 256;
    while (budget-- > 0 && Serial1.available()) {
        int c = Serial1.read();
        if (c < 0) break;
        st.serialBytes++;
        if (!serialLine.push((char)c)) continue;
        const char* line = serialLine.line();
        // La veleta entra como "sensor", la fuente de mas prioridad: otro viento del NMEA 2000 no la pisa
        if (serialNmea.feed(line, Source::Sensor, now, *store) != NmeaResult::Data) continue;
        if (strstr(line, "MWV")) {
            st.vaneLines++;
            st.vaneLastMs = now ? now : 1;
            int off = wsettings::get().vaneOffset;
            double awa;
            if (off && store->get(Key::Awa, now, awa)) store->set(Key::Awa, norm180(awa + off), Source::Sensor, now);
        }
    }
    // rumbo y velocidad que manda el plotter por Bluetooth: fuente "BLE", por debajo del NMEA 2000 del propio bus
    uint8_t b[128];
    for (int round = 0; round < 4; round++) {
        int n = wble::read(b, sizeof(b));
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            if (!bleLine.push((char)b[i])) continue;
            tsync::offerNmea(bleLine.line(), plotter::TimeSource::Peer);   // ZDA del plotter: pone el reloj
            if (bleNmea.feed(bleLine.line(), Source::Ble, now, *store) == NmeaResult::Data) { st.navLines++; st.navLastMs = now ? now : 1; }
        }
    }
    canLoop(now);
    wnet::loop();
    output(now);
}

const Status& status() { return st; }
}
