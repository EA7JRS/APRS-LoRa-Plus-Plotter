/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "net.h"

#ifdef ESP32
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include "gnss.h"
#include "signalk.h"
#endif

namespace net {

static Status st;
const Status& status() { return st; }

#ifdef ESP32

static Config cfg;
static volatile uint32_t cfgVersion = 0;
static SemaphoreHandle_t cfgLock = nullptr;
static StreamBufferHandle_t sbTcp = nullptr, sbUdp = nullptr, sbOut = nullptr;
static double posLat = 0, posLon = 0;
static bool posValid = false;
static Weather wx;
static bool wxNew = false;
static const int kWarnMax = 16;
static wxw::Warning warnList[kWarnMax];
static int warnN = 0;
static volatile uint32_t warnSeqN = 0;
static char warnMsg[96] = "";

// Avisos meteorologicos: recibe el cuerpo segun llega, corta los avisos y se queda con los de la zona
struct WarnSink : public Stream {
    wxw::Scanner sc;
    wxw::Warning keep[kWarnMax];
    int n = 0;
    bool havePos = false, byName = false;
    double lat = 0, lon = 0;
    const char* zones = "";
    int64_t nowS = 0;
    size_t bytes = 0;
    void take(const std::string& blk) {
        wxw::Warning w;
        if (!wxw::parse(blk, w, havePos, lat, lon) || wxw::expired(w, nowS)) return;
        const bool mine = w.hasArea ? (w.inside || w.distKm <= 30) : (byName && wxw::areaMatches(w.area, zones));
        if (!mine) return;
        for (int i = 0; i < n; i++) if (keep[i].event == w.event && keep[i].area == w.area && keep[i].onset == w.onset) return;
        if (n < kWarnMax) keep[n++] = w;
    }
    size_t write(const uint8_t* b, size_t len) override {
        bytes += len;
        std::string blk;
        bool r = sc.feed(b, len, blk);
        while (r) { take(blk); r = sc.feed(nullptr, 0, blk); }
        return len;
    }
    size_t write(uint8_t c) override { return write(&c, 1); }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
static void warnSay(const char* t) { xSemaphoreTake(cfgLock, portMAX_DELAY); snprintf(warnMsg, sizeof(warnMsg), "%s", t); xSemaphoreGive(cfgLock); }
// AEMET (con clave, por la posicion) o el canal de Meteoalarm del pais (por nombres de zona). true si se bajo bien.
static bool fetchWarnings(const Config& c, bool havePos, double lat, double lon) {
    static WarnSink sink;                                           // grande: fuera de la pila
    sink = WarnSink();
    String url;
    char zones[80];
    snprintf(zones, sizeof(zones), "%s", c.warnZones);
    char* colon = strchr(zones, ':');
    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(15000);
    if (c.aemetKey[0]) {
        if (!havePos) { warnSay("AEMET: esperando la posici\xC3\xB3n del barco."); return false; }
        // AEMET contesta con la direccion de los datos: un archivo con todos los avisos CAP en vigor de Espana
        if (!http.begin(tls, String("https://opendata.aemet.es/opendata/api/avisos_cap/ultimoelaborado/area/esp?api_key=") + c.aemetKey)) return false;
        int code = http.GET();
        if (code != 200) { http.end(); warnSay(code == 401 || code == 403 ? "AEMET no admite la clave." : "AEMET no contesta."); return false; }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        http.end();
        const char* datos = doc["datos"] | "";
        if (err || !datos[0]) { warnSay("AEMET: respuesta inesperada."); return false; }
        url = datos;
    } else {
        if (!colon) { warnSay("Avisos: falta \"pa\xC3\xADs: zonas\" (Ajustes > Red)."); return false; }
        *colon = 0;
        String country(zones);
        country.trim(); country.toLowerCase();
        url = "https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-" + country;
        sink.byName = true;
        sink.zones = colon + 1;
    }
    sink.havePos = havePos; sink.lat = lat; sink.lon = lon;
    time_t t = time(nullptr);
    sink.nowS = t > 1700000000 ? (int64_t)t : 0;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(tls, url)) return false;
    int code = http.GET();
    if (code == 200 && http.writeToStream(&sink) < 0) code = -1;
    http.end();
    if (code != 200) { warnSay("Avisos: la descarga ha fallado."); return false; }
    if (sink.sc.gzip()) { warnSay("Avisos: el archivo viene comprimido (a\xC3\xBAn no se lee)."); return false; }
    for (int i = 1; i < sink.n; i++)                                // pocos: ordenacion por insercion
        for (int j = i; j > 0 && wxw::before(sink.keep[j], sink.keep[j - 1]); j--) { wxw::Warning x = sink.keep[j]; sink.keep[j] = sink.keep[j - 1]; sink.keep[j - 1] = x; }
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    warnN = sink.n;
    for (int i = 0; i < sink.n; i++) warnList[i] = sink.keep[i];
    snprintf(warnMsg, sizeof(warnMsg), "%s: %d en tu zona de %u en vigor.", c.aemetKey[0] ? "AEMET" : "Meteoalarm", sink.n, (unsigned)sink.sc.blocks());
    warnSeqN++;
    xSemaphoreGive(cfgLock);
    return true;
}

// El tiempo en la posicion del barco. Bloquea unos segundos como mucho: va en la tarea de red, no en la de la pantalla.
static bool fetchWeather(const char* key, double lat, double lon) {
    WiFiClientSecure tls;
    tls.setInsecure();                                             // cifrado, sin comprobar el certificado (no hay reloj fiable al arrancar)
    HTTPClient http;
    char url[200];
    snprintf(url, sizeof(url), "https://api.openweathermap.org/data/2.5/weather?lat=%.4f&lon=%.4f&units=metric&appid=%s", lat, lon, key);
    http.setConnectTimeout(6000);
    http.setTimeout(6000);
    if (!http.begin(tls, url)) return false;
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return false;
    Weather w;
    w.windMs = doc["wind"]["speed"] | NAN;
    w.windDeg = doc["wind"]["deg"] | NAN;
    w.gustMs = doc["wind"]["gust"] | NAN;
    w.pressureHpa = doc["main"]["pressure"] | NAN;
    w.tempC = doc["main"]["temp"] | NAN;
    w.humidityPct = doc["main"]["humidity"] | NAN;
    w.visibilityM = doc["visibility"] | NAN;
    snprintf(w.place, sizeof(w.place), "%s", (const char*)(doc["name"] | ""));
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    wx = w;
    wxNew = true;
    xSemaphoreGive(cfgLock);
    return true;
}

static void task(void*) {
    Config c;
    uint32_t seen = 0xFFFFFFFFu, lastBegin = 0, lastTry = 0, lastInfo = 0, nextWx = 0, nextWarn = 0;
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
            nextWx = now;
            nextWarn = now + 8000;
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
        // Tiempo de internet: cada 15 min (a los 2 min si ha fallado), con clave y posicion
        if (c.owmKey[0] && (int32_t)(now - nextWx) >= 0) {
            double la, lo;
            xSemaphoreTake(cfgLock, portMAX_DELAY);
            bool ok = posValid;
            la = posLat; lo = posLon;
            xSemaphoreGive(cfgLock);
            if (!ok) nextWx = now + 5000;                           // aun sin posicion
            else if (fetchWeather(c.owmKey, la, lo)) { st.wxOk++; nextWx = now + 15 * 60000u; }
            else { st.wxFail++; nextWx = now + 2 * 60000u; }
        }
        // Avisos meteorologicos: cada 15 min (a los 3 si ha fallado)
        if ((c.aemetKey[0] || c.warnZones[0]) && (int32_t)(now - nextWarn) >= 0) {
            double la, lo;
            xSemaphoreTake(cfgLock, portMAX_DELAY);
            bool ok = posValid;
            la = posLat; lo = posLon;
            xSemaphoreGive(cfgLock);
            nextWarn = now + (fetchWarnings(c, ok, la, lo) ? 15 : 3) * 60000u;
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
        if (c.tcpKind != TCP_OFF && c.tcpPort > 0) {
            if (!client.connected()) {
                st.tcp = false;
                if (lastTry == 0 || now - lastTry > 10000) {       // reintento cada 10 s
                    lastTry = now ? now : 1;
                    // Sin servidor escrito: el equipo que da la WiFi (el telefono con la app, que sirve NMEA en el 10110)
                    bool ok = c.host[0] ? client.connect(c.host, (uint16_t)c.tcpPort, 3000) : client.connect(WiFi.gatewayIP(), (uint16_t)c.tcpPort, 3000);
                    if (ok) {
                        client.setNoDelay(true);
                        if (c.tcpKind == TCP_GPSD) client.print(plotter::kGpsdWatchNmea);   // gpsd: que mande el NMEA tal cual
                        if (c.tcpKind == TCP_SIGNALK) {                 // Signal K: todo lo del barco propio, una vez por segundo
                            char sub[120];
                            int n = plotter::skSubscribe(sub, sizeof(sub), 1000);
                            if (n) client.write((const uint8_t*)sub, (size_t)n);
                        }
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
    xTaskCreatePinnedToCore(task, "net", 12288, nullptr, 1, nullptr, 0);    // la pila, holgada por el TLS
}

void apply(const Config& c) {
    if (!cfgLock) begin();
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    cfg = c;
    cfgVersion = cfgVersion + 1;
    xSemaphoreGive(cfgLock);
}

void setPosition(double lat, double lon) {
    if (!cfgLock) return;
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    posLat = lat; posLon = lon; posValid = true;
    xSemaphoreGive(cfgLock);
}

bool takeWeather(Weather& w) {
    if (!cfgLock || !wxNew) return false;
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    w = wx;
    wxNew = false;
    xSemaphoreGive(cfgLock);
    return true;
}

int warnings(wxw::Warning* out, int max) {
    if (!cfgLock) return 0;
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    int n = warnN < max ? warnN : max;
    for (int i = 0; i < n; i++) out[i] = warnList[i];
    xSemaphoreGive(cfgLock);
    return n;
}
uint32_t warnSeq() { return warnSeqN; }
const char* warnText() { return warnMsg; }
void demoWarnings(int) {}

int readTcp(uint8_t* b, int max) { return sbTcp ? (int)xStreamBufferReceive(sbTcp, b, (size_t)max, 0) : 0; }
void sendTcp(const char* d, int len) { if (sbOut && st.tcp && len > 0) xStreamBufferSend(sbOut, d, (size_t)len, 0); }
int readUdp(uint8_t* b, int max) { return sbUdp ? (int)xStreamBufferReceive(sbUdp, b, (size_t)max, 0) : 0; }

#else   // en el PC no hay red

void begin() {}
void apply(const Config&) {}
int readTcp(uint8_t*, int) { return 0; }
void sendTcp(const char*, int) {}
void setPosition(double, double) {}
bool takeWeather(Weather&) { return false; }
static wxw::Warning demo[4];
static int demoN = 0;
static uint32_t demoSeq = 0;
int warnings(wxw::Warning* out, int max) { int n = demoN < max ? demoN : max; for (int i = 0; i < n; i++) out[i] = demo[i]; return n; }
uint32_t warnSeq() { return demoSeq; }
const char* warnText() { return demoN ? "AEMET: 3 en tu zona de 37 en vigor." : ""; }
void demoWarnings(int n) {
    auto mk = [](int lvl, const char* ev, const char* area, const char* phen, const char* on, const char* ex, const char* desc, const char* prob, bool inside, float d) {
        wxw::Warning x; x.level = lvl; x.event = ev; x.area = area; x.phen = phen; x.onset = on; x.expires = ex; x.desc = desc; x.prob = prob; x.source = "AEMET";
        x.hasArea = true; x.inside = inside; x.distKm = d; return x; };
    demo[0] = mk(3, "Aviso de fen\xC3\xB3menos costeros de nivel rojo", "Estrecho", "Costeros", "2026-11-12T06:00:00+01:00", "2026-11-13T23:59:59+01:00",
                 "Viento de levante fuerza 9. Olas de 5 a 6 metros.", ">70%", true, 0);
    demo[1] = mk(2, "Aviso de lluvias de nivel naranja", "Litoral gaditano", "Lluvias", "2026-11-13T00:00:00+01:00", "2026-11-13T03:59:59+01:00",
                 "Precipitaci\xC3\xB3n acumulada en una hora: 30 mm.", "40%-70%", true, 0);
    demo[2] = mk(1, "Aviso de vientos de nivel amarillo", "Campi\xC3\xB1" "a gaditana", "Vientos", "2026-11-12T12:00:00+01:00", "2026-11-12T20:59:59+01:00",
                 "Rachas m\xC3\xA1ximas de 70 km/h.", "40%-70%", false, 14);
    demoN = n < 3 ? n : 3;
    demoSeq++;
}
int readUdp(uint8_t*, int) { return 0; }

#endif

}
