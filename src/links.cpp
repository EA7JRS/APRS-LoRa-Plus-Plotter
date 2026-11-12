/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "links.h"

#include "ble.h"
#include "geo.h"
#include "nmea.h"
#include "board.h"
#include "n2k.h"
#include "net.h"
#include "settings.h"
#include "sim.h"
#include "sk_link.h"
#include "stream.h"
#include "tsync.h"

#include <cmath>
#include <ctime>

#ifdef ESP32
#include "driver/twai.h"
#endif

namespace links {

using namespace plotter;

static DataStore* store = nullptr;
static Status st;
static NmeaSimulator sim;
static Nmea0183 simNmea, serialNmea;
static LineBuffer serialLine;
static AisTargets tgs;
static uint32_t simLast = 0;
static N2kReader n2k;
static Nmea0183 tcpNmea, udpNmea, bleNmea;
static LineBuffer tcpLine, udpLine, bleLine;
[[maybe_unused]] static bool canRunning = false;

// NMEA 2000: el plotter solo escucha. El controlador va en modo normal para dar el acuse de recibo a las tramas de los
// demas (con un solo equipo mas en la red, sin acuse este repetiria sin parar), pero nunca transmite.
static void canStart() {
#ifdef ESP32
    if (canRunning) return;
    board::setCanMode(true);                                       // los pines 19 / 20 pasan del USB al transceptor
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)board::PIN_CAN_TX, (gpio_num_t)board::PIN_CAN_RX, TWAI_MODE_NORMAL);
    g.rx_queue_len = 96;
    g.tx_queue_len = 0;
    twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (twai_driver_install(&g, &t, &f) != ESP_OK) { Serial.println("[links] CAN: no se pudo instalar el controlador"); return; }
    if (twai_start() != ESP_OK) { twai_driver_uninstall(); Serial.println("[links] CAN: no arranca"); return; }
    canRunning = true;
    Serial.printf("[links] NMEA 2000 a 250 kbit/s (TX %d, RX %d), solo escucha\n", board::PIN_CAN_TX, board::PIN_CAN_RX);
#endif
}

static void canStop() {
#ifdef ESP32
    if (!canRunning) return;
    twai_stop();
    twai_driver_uninstall();
    canRunning = false;
    board::setCanMode(false);
#endif
}

static void canLoop(uint32_t now) {
#ifdef ESP32
    if (!canRunning) return;
    twai_message_t m;
    int budget = 96;
    while (budget-- > 0 && twai_receive(&m, 0) == ESP_OK) {
        if (!m.extd || m.rtr) continue;                            // NMEA 2000 solo usa identificadores de 29 bits
        st.canFrames++;
        st.canLastMs = now;
        if (n2k.feedFrame(m.identifier, m.data, m.data_length_code, Source::Can, now, *store) == N2kResult::Ais) {
            tgs.update(n2k.ais(), now);
            st.aisMessages++;
        }
        int64_t utc;
        if (n2k.takeUtc(utc)) tsync::offer(plotter::TimeSource::Gps, utc);   // hora del bus (GPS de la red NMEA 2000)
    }
    st.canMessages = n2k.stats().known;
    static uint32_t tCheck = 0;
    if (now - tCheck >= 2000) {                                    // si el controlador se ha salido del bus, vuelve a entrar
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

void begin(DataStore* s) {
    store = s;
    // El simulador entra como "sensor": mientras este encendido manda sobre lo demas, para ver la interfaz sin nada conectado
    Serial1.setRxBufferSize(2048);
    Serial1.begin(st.serialBaud, SERIAL_8N1, board::PIN_RS485_RX, board::PIN_RS485_TX);
    Serial.printf("[links] RS485 a %u baudios (RX %d, TX %d); simulador %s\n", (unsigned)st.serialBaud, board::PIN_RS485_RX,
                  board::PIN_RS485_TX, st.simulator ? "encendido" : "apagado");
}

void setSimulator(bool on) { st.simulator = on; }

bool sendPilot(const char* sentence, int len, int via) {
    if (len <= 0) return false;
    if (via == 1) { Serial1.write((const uint8_t*)sentence, (size_t)len); st.pilotSent++; return true; }
    if (via == 2 && net::status().tcp) { net::sendTcp(sentence, len); st.pilotSent++; return true; }
    return false;
}

void applyNetwork() {
    const settings::Values& v = settings::get();
    net::Config c;
    snprintf(c.ssid, sizeof(c.ssid), "%s", v.wifiSsid);
    snprintf(c.pass, sizeof(c.pass), "%s", v.wifiPass);
    snprintf(c.host, sizeof(c.host), "%s", v.tcpHost);
    c.tcpKind = v.tcpKind;
    c.tcpPort = v.tcpPort;
    c.udpPort = v.udpPort;
    snprintf(c.owmKey, sizeof(c.owmKey), "%s", v.owmKey);
    net::apply(c);
}

void applyBle() {
    const settings::Values& v = settings::get();
    ble::set(v.bleOn, v.bleAddr, (uint32_t)v.blePin);
}

// Pasarela por BLE: lineas NMEA como fuente "BLE". La primera vez que conecta se recuerda el equipo.
static void bleLoop(uint32_t now) {
    static uint8_t buf[256];
    for (int round = 0; round < 4; round++) {
        int n = ble::read(buf, sizeof(buf));
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            if (!bleLine.push((char)buf[i])) continue;
            st.bleLines++;
            tsync::offerNmea(bleLine.line());                     // hora del GPS del tracker o de la pasarela
            if (bleNmea.feed(bleLine.line(), Source::Ble, now, *store) == NmeaResult::Ais) {
                tgs.update(bleNmea.ais(), now);
                st.aisMessages++;
            }
        }
    }
    settings::Values& v = settings::get();
    const ble::Status& bs = ble::status();
    // Al instrumento de viento (equipo con la caracteristica de ordenes), una vez por segundo: rumbo verdadero (HDT),
    // rumbo y velocidad sobre el fondo (VTG) y velocidad en el agua (VHW), para que calcule el viento real
    static uint32_t tNav = 0;
    if (bs.state == ble::CONNECTED && bs.canSend && now - tNav >= 1000) {
        tNav = now;
        char out[200], body[64];
        int n = 0;
        double hdg, cog, sog, stw;
        bool hHdg = store->get(Key::Heading, now, hdg), hCog = store->get(Key::Cog, now, cog), hSog = store->get(Key::Sog, now, sog);
        bool hStw = store->get(Key::Stw, now, stw);
        auto add = [&](const char* b) { n += snprintf(out + n, sizeof(out) - n, "$%s*%02X\r\n", b, nmeaChecksum(b)); };
        if (hHdg) { snprintf(body, sizeof(body), "IIHDT,%.1f,T", norm360(hdg)); add(body); }
        if (hSog) {
            if (hCog) snprintf(body, sizeof(body), "IIVTG,%.1f,T,,M,%.1f,N,%.1f,K,A", norm360(cog), sog, sog * 1.852);
            else snprintf(body, sizeof(body), "IIVTG,,T,,M,%.1f,N,%.1f,K,A", sog, sog * 1.852);
            add(body);
        }
        if (hStw) {
            if (hHdg) snprintf(body, sizeof(body), "IIVHW,%.1f,T,,M,%.1f,N,%.1f,K", norm360(hdg), stw, stw * 1.852);
            else snprintf(body, sizeof(body), "IIVHW,,T,,M,%.1f,N,%.1f,K", stw, stw * 1.852);
            add(body);
        }
        // Hora (ZDA) para el reloj del instrumento, solo si la del plotter viene de GPS o NTP
        const tsync::Status ts = tsync::status();
        const time_t t = (time_t)tsync::now();
        if (t && (ts.source == plotter::TimeSource::Gps || ts.source == plotter::TimeSource::Ntp)) {
            struct tm u;
            gmtime_r(&t, &u);
            snprintf(body, sizeof(body), "IIZDA,%02d%02d%02d.00,%02d,%02d,%04d,00,00", u.tm_hour, u.tm_min, u.tm_sec, u.tm_mday, u.tm_mon + 1,
                     u.tm_year + 1900);
            add(body);
        }
        if (n > 0 && ble::write(out, n)) st.bleNavSent++;
    }
    if (v.bleOn && bs.state == ble::CONNECTED && bs.addr[0] && strcmp(v.bleAddr, bs.addr) != 0) {
        snprintf(v.bleAddr, sizeof(v.bleAddr), "%s", bs.addr);
        settings::save();
        ble::set(true, v.bleAddr, (uint32_t)v.blePin);             // desde ahora, solo ese equipo
    }
}

// Datos de internet: entran como la fuente de menos prioridad y marcados como estimados. Mientras el sensor de a bordo
// mande, ni se ven; si se calla (10 s), la tabla pasa sola a estos, y vuelve al sensor en cuanto reaparece.
static void internetLoop(uint32_t now) {
    static uint32_t tPos = 0;
    if (now - tPos >= 30000 || tPos == 0) {
        double la, lo;
        if (store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo)) { net::setPosition(la, lo); tPos = now ? now : 1; }
    }
    net::Weather w;
    if (!net::takeWeather(w)) return;
    const double kn = 3600.0 / 1852.0;
    auto put = [&](Key k, double v) { if (!std::isnan(v)) store->set(k, v, Source::Internet, now, Quality::Estimated); };
    put(Key::Tws, w.windMs * kn);
    put(Key::Twd, w.windDeg);
    put(Key::Gust, w.gustMs * kn);
    put(Key::Pressure, w.pressureHpa);
    put(Key::AirTemp, w.tempC);
    put(Key::Humidity, w.humidityPct);
    put(Key::Visibility, w.visibilityM / 1852.0);
    snprintf(st.wxPlace, sizeof(st.wxPlace), "%s", w.place);
    st.wxMs = now ? now : 1;
}

// Bytes de la red -> lineas -> lector NMEA / AIS, como fuente "red"
static void netLoop(uint32_t now) {
    static uint8_t buf[512];
    if (settings::get().tcpKind == net::TCP_SIGNALK) {             // Signal K: lineas JSON, mucho mas largas que una sentencia
        static char sk[4096];
        static int skLen = 0;
        static bool skOver = false;
        for (int round = 0; round < 8; round++) {
            int n = net::readTcp(buf, sizeof(buf));
            if (n <= 0) break;
            for (int i = 0; i < n; i++) {
                char c = (char)buf[i];
                if (c == '\n') {
                    if (!skOver && skLen > 0) {
                        sk[skLen] = 0;
                        sk::Notice nt;
                        st.skValues += (uint32_t)sk::feedLine(sk, now, *store, nt);
                        st.tcpLines++;
                        st.netLastMs = now;
                        if (nt.present) {
                            snprintf(st.skKey, sizeof(st.skKey), "%s", nt.key);
                            snprintf(st.skState, sizeof(st.skState), "%s", nt.state);
                            snprintf(st.skMessage, sizeof(st.skMessage), "%s", nt.message);
                            st.skSeq++;
                        }
                    }
                    skLen = 0;
                    skOver = false;
                } else if (c != '\r') {
                    if (skLen < (int)sizeof(sk) - 1) sk[skLen++] = c;
                    else skOver = true;                            // linea demasiado larga: se descarta entera
                }
            }
        }
    }
    for (int pass = settings::get().tcpKind == net::TCP_SIGNALK ? 1 : 0; pass < 2; pass++) {
        LineBuffer& lb = pass ? udpLine : tcpLine;
        Nmea0183& rd = pass ? udpNmea : tcpNmea;
        uint32_t& lines = pass ? st.udpLines : st.tcpLines;
        for (int round = 0; round < 4; round++) {
            int n = pass ? net::readUdp(buf, sizeof(buf)) : net::readTcp(buf, sizeof(buf));
            if (n <= 0) break;
            for (int i = 0; i < n; i++) {
                if (!lb.push((char)buf[i])) continue;
                lines++;
                st.netLastMs = now;
                tsync::offerNmea(lb.line());                       // gpsd, el movil, un multiplexor...
                if (rd.feed(lb.line(), Source::Tcp, now, *store) == NmeaResult::Ais) {
                    tgs.update(rd.ais(), now);
                    st.aisMessages++;
                }
            }
        }
    }
}

void setCan(bool on) {
    st.can = on;
    if (on) canStart(); else canStop();
}

// Para probar sin red: una trama como si llegara del bus
void injectCanFrame(uint32_t canId, const uint8_t* data, int len) {
    if (!store) return;
    uint32_t now = millis();
    st.canFrames++;
    st.canLastMs = now;
    if (n2k.feedFrame(canId, data, len, Source::Can, now, *store) == N2kResult::Ais) {
        tgs.update(n2k.ais(), now);
        st.aisMessages++;
    }
    int64_t utc;
    if (n2k.takeUtc(utc)) tsync::offer(plotter::TimeSource::Gps, utc);
    st.canMessages = n2k.stats().known;
}

void setSerialBaud(uint32_t baud) {
    if (baud == st.serialBaud) return;
    st.serialBaud = baud;
    Serial1.updateBaudRate(baud);
}

const Status& status() { return st; }
const NmeaStats& serialStats() { return serialNmea.stats(); }
AisTargets& targets() { return tgs; }

void loop() {
    if (!store) return;
    uint32_t now = millis();

    int budget = 512;                                              // sin acaparar el bucle si llega una rafaga
    while (budget-- > 0 && Serial1.available()) {
        if (!serialLine.push((char)Serial1.read())) continue;
        st.serialLines++;
        st.serialLastMs = now;
        tsync::offerNmea(serialLine.line());                       // el Comar y las emisoras sacan RMC
        if (serialNmea.feed(serialLine.line(), Source::Uart, now, *store) == NmeaResult::Ais) {
            tgs.update(serialNmea.ais(), now);
            st.aisMessages++;
        }
    }
    serialNmea.sky().expire(now);
    canLoop(now);
    netLoop(now);
    internetLoop(now);
    bleLoop(now);

    if (st.simulator && now - simLast >= 1000) {
        simLast = now;
        sim.step(now);
        char line[100];
        for (int i = 0; i < NmeaSimulator::count(); i++)
            if (sim.sentence(i, line, sizeof(line))) simNmea.feed(line, Source::Sensor, now, *store);
        static uint8_t tick = 0;
        tick++;
        for (int i = 0; i < NmeaSimulator::aisCount(); i++) {
            if ((i % 2) && (tick % 10)) continue;                   // los nombres, cada 10 s
            if (sim.aisSentence(i, line, sizeof(line)) && simNmea.feed(line, Source::Sensor, now, *store) == NmeaResult::Ais)
                tgs.update(simNmea.ais(), now);
        }
    }

    static uint32_t tCompute = 0;
    if (now - tCompute >= 1000) {
        tCompute = now;
        double la, lo, sog = 0, cog = 0;
        bool own = store->get(Key::Lat, now, la) && store->get(Key::Lon, now, lo);
        store->get(Key::Sog, now, sog);
        store->get(Key::Cog, now, cog);
        tgs.compute(own, Mover{{own ? la : 0.0, own ? lo : 0.0}, sog, cog}, now);
    }
}

}
