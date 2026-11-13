/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "aviation.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "alerts.h"
#include "minijson.h"

namespace plotter {

const char* flightCatName(FlightCat c) {
    switch (c) {
        case FlightCat::Vfr: return "VFR";
        case FlightCat::Mvfr: return "MVFR";
        case FlightCat::Ifr: return "IFR";
        case FlightCat::Lifr: return "LIFR";
        default: return "";
    }
}

uint32_t flightCatColor(FlightCat c) {
    switch (c) {
        case FlightCat::Vfr: return 0x3CE68A;
        case FlightCat::Mvfr: return 0x4DA3FF;
        case FlightCat::Ifr: return 0xFF6B6B;
        case FlightCat::Lifr: return 0xD86BFF;
        default: return 0x9DB2C7;
    }
}

FlightCat flightCategory(int ceil, double vis) {
    if (ceil < 0 && vis < 0) return FlightCat::Unknown;
    auto byCeil = [](int c) {
        if (c < 0) return FlightCat::Vfr;
        if (c < 500) return FlightCat::Lifr;
        if (c < 1000) return FlightCat::Ifr;
        if (c <= 3000) return FlightCat::Mvfr;
        return FlightCat::Vfr;
    };
    auto byVis = [](double v) {
        if (v < 0) return FlightCat::Vfr;
        if (v < 1) return FlightCat::Lifr;
        if (v < 3) return FlightCat::Ifr;
        if (v <= 5) return FlightCat::Mvfr;
        return FlightCat::Vfr;
    };
    FlightCat a = byCeil(ceil), b = byVis(vis);
    return (uint8_t)a > (uint8_t)b ? a : b;       // la peor de las dos
}

static std::string urlBox(const char* what, double a, double b, double c, double d) {
    char u[200];
    snprintf(u, sizeof(u), "%s%s?bbox=%.2f,%.2f,%.2f,%.2f&format=json", kAwcBase, what, a, b, c, d);
    return u;
}
std::string awcMetarUrl(double a, double b, double c, double d) { return urlBox("metar", a, b, c, d); }
std::string awcTafUrl(double a, double b, double c, double d) { return urlBox("taf", a, b, c, d); }
std::string awcSigmetUrl(const char* loc) { return std::string(kAwcBase) + "isigmet?format=geojson&loc=" + loc; }

const char* awcSigmetRegion(double lat, double lon) {
    if (lat > 25 && lon > -40 && lon < 50) return "eur";      // Europa, Atlantico oriental y Mediterraneo
    if (lat <= 25 && lat > -40 && lon > -25 && lon < 55) return "afr";
    if (lat > 10 && lon <= -40 && lon > -170) return "namer";
    return "all";
}

// Hora: segundos Unix (numero) o fecha ISO («2026-11-13T08:30:00Z»)
static int64_t timeOf(const JsonValue& v) {
    if (v.isNumber()) return (int64_t)v.number;
    if (v.isString() && !v.text.empty()) {
        bool digits = true;
        for (char ch : v.text) if (ch < '0' || ch > '9') digits = false;
        if (digits) return (int64_t)atoll(v.text.c_str());
        return parseIsoTime(v.text);
    }
    return 0;
}

static int intOr(const JsonValue& v, int fb) { return v.isNumber() || (v.isString() && !v.text.empty() && (isdigit((unsigned char)v.text[0]) || v.text[0] == '-')) ? (int)lround(v.num(fb)) : fb; }

// Visibilidad del parte en bruto (grupo de 4 cifras en metros, o CAVOK): mas exacta que la de la API (millas) en Europa
static int rawVisibilityM(const std::string& raw) {
    size_t i = 0;
    int idx = 0;
    while (i < raw.size()) {
        while (i < raw.size() && raw[i] == ' ') i++;
        size_t e = raw.find(' ', i);
        if (e == std::string::npos) e = raw.size();
        std::string t = raw.substr(i, e - i);
        i = e;
        idx++;
        if (idx < 3) continue;                                // METAR, indicativo, hora
        if (t == "CAVOK") return 9999;
        if (t == "RMK" || t == "TEMPO" || t == "BECMG" || t == "NOSIG") break;
        if (t.size() == 4 && isdigit((unsigned char)t[0]) && isdigit((unsigned char)t[1]) && isdigit((unsigned char)t[2]) &&
            isdigit((unsigned char)t[3]))
            return atoi(t.c_str());
        if (t.size() > 2 && t.compare(t.size() - 2, 2, "SM") == 0) return -1;   // EE. UU.: millas, ya vienen en visib
    }
    return -1;
}

bool metarParseJson(const std::string& json, std::vector<Metar>& out) {
    JsonValue v;
    if (!jsonParse(json, v) || !v.isArray()) return false;
    for (const auto& o : v.items) {
        if (!o.isObject()) continue;
        Metar m;
        m.icao = o["icaoId"].str();
        if (m.icao.empty() || !o["lat"].isNumber() || !o["lon"].isNumber()) continue;
        m.name = o["name"].str();
        m.raw = o["rawOb"].str();
        m.lat = o["lat"].num();
        m.lon = o["lon"].num();
        m.obsTime = timeOf(o["obsTime"]);
        if (!m.obsTime) m.obsTime = timeOf(o["reportTime"]);
        const JsonValue& wd = o["wdir"];
        m.wdir = wd.isString() && wd.text == "VRB" ? -2 : intOr(wd, -1);
        m.wspdKt = intOr(o["wspd"], -1);
        m.wgstKt = intOr(o["wgst"], -1);
        if (o["temp"].isNumber()) m.tempC = o["temp"].number;
        if (o["dewp"].isNumber()) m.dewpC = o["dewp"].number;
        if (o["altim"].isNumber()) m.qnhHpa = o["altim"].number;
        const JsonValue& vis = o["visib"];
        if (vis.isNumber()) m.visibSm = vis.number;
        else if (vis.isString() && !vis.text.empty()) {
            m.visibSm = atof(vis.text.c_str());
            m.visibPlus = vis.text.back() == '+';
        }
        m.wx = o["wxString"].str();
        m.vertVisFt = intOr(o["vertVis"], -1);
        for (const auto& c : o["clouds"].items) {
            CloudLayer l;
            l.cover = c["cover"].str();
            l.baseFt = intOr(c["base"], -1);
            if (l.cover.empty()) continue;
            m.clouds.push_back(l);
            if ((l.cover == "BKN" || l.cover == "OVC" || l.cover == "OVX") && l.baseFt >= 0 && (m.ceilingFt < 0 || l.baseFt < m.ceilingFt))
                m.ceilingFt = l.baseFt;
        }
        if (m.vertVisFt >= 0 && (m.ceilingFt < 0 || m.vertVisFt < m.ceilingFt)) m.ceilingFt = m.vertVisFt;
        const std::string fc = o["fltCat"].str();
        if (fc == "VFR") m.cat = FlightCat::Vfr;
        else if (fc == "MVFR") m.cat = FlightCat::Mvfr;
        else if (fc == "IFR") m.cat = FlightCat::Ifr;
        else if (fc == "LIFR") m.cat = FlightCat::Lifr;
        else {
            double vsm = m.visibSm;
            const int vm = rawVisibilityM(m.raw);
            if (vm >= 0) vsm = vm >= 9999 ? 10 : vm / 1609.344;
            m.cat = flightCategory(m.ceilingFt, vsm);
        }
        out.push_back(m);
    }
    return true;
}

bool tafParseJson(const std::string& json, std::vector<Taf>& out) {
    JsonValue v;
    if (!jsonParse(json, v) || !v.isArray()) return false;
    for (const auto& o : v.items) {
        Taf t;
        t.icao = o["icaoId"].str();
        t.raw = o["rawTAF"].str(o["rawOb"].str().c_str());
        t.issueTime = timeOf(o["issueTime"]);
        if (!t.icao.empty() && !t.raw.empty()) out.push_back(t);
    }
    return true;
}

bool sigmetParseGeoJson(const std::string& json, std::vector<Sigmet>& out) {
    JsonValue v;
    if (!jsonParse(json, v) || !v["features"].isArray()) return false;
    for (const auto& f : v["features"].items) {
        const JsonValue& p = f["properties"];
        const JsonValue& g = f["geometry"];
        Sigmet s;
        s.fir = p["firId"].str();
        s.firName = p["firName"].str();
        s.hazard = p["hazard"].str();
        s.qualifier = p["qualifier"].str();
        s.raw = p["rawSigmet"].str();
        s.id = s.fir + " " + p["seriesId"].str(p["isigmetId"].str().c_str());
        s.baseFt = intOr(p["base"], -1);
        s.topFt = intOr(p["top"], -1);
        s.from = timeOf(p["validTimeFrom"]);
        s.to = timeOf(p["validTimeTo"]);
        s.dir = p["dir"].str();
        s.spdKt = intOr(p["spd"], -1);
        const std::string type = g["type"].str();
        const JsonValue& c = g["coordinates"];
        auto add = [&](const JsonValue& pt) { if (pt.size() >= 2) s.pts.push_back({pt[1].num(), pt[0].num()}); };   // GeoJSON: lon, lat
        if (type == "Polygon" && c.size() > 0) for (const auto& pt : c[0].items) add(pt);
        else if (type == "MultiPolygon" && c.size() > 0 && c[0].size() > 0) for (const auto& pt : c[0][0].items) add(pt);
        else if (type == "LineString") for (const auto& pt : c.items) add(pt);
        else if (type == "Point") add(c);
        if (s.pts.empty() || s.hazard.empty()) continue;
        out.push_back(s);
    }
    return true;
}

static const char* coverName(const std::string& c, bool es) {
    if (c == "FEW") return es ? "escasas" : "few";
    if (c == "SCT") return es ? "dispersas" : "scattered";
    if (c == "BKN") return es ? "fragmentadas" : "broken";
    if (c == "OVC") return es ? "cubierto" : "overcast";
    if (c == "OVX") return es ? "cielo oculto" : "sky obscured";
    return nullptr;
}

std::string metarWindText(const Metar& m, bool es) {
    char b[96];
    if (m.wspdKt < 0) return es ? "Viento: sin dato" : "Wind: no data";
    if (m.wspdKt == 0) return es ? "Calma" : "Calm";
    char dir[16];
    if (m.wdir == -2) snprintf(dir, sizeof(dir), "%s", es ? "variable" : "variable");
    else snprintf(dir, sizeof(dir), "%03d\xC2\xB0", m.wdir < 0 ? 0 : m.wdir);
    if (m.wgstKt > 0) snprintf(b, sizeof(b), es ? "Viento %s %d kn, rachas %d kn" : "Wind %s %d kn, gusts %d kn", dir, m.wspdKt, m.wgstKt);
    else snprintf(b, sizeof(b), es ? "Viento %s %d kn" : "Wind %s %d kn", dir, m.wspdKt);
    return b;
}

std::string metarSkyText(const Metar& m, bool es) {
    std::string o;
    char b[64];
    const int vm = rawVisibilityM(m.raw);
    if (vm >= 9999 || (vm < 0 && m.visibPlus && m.visibSm >= 6)) o = es ? "Visibilidad 10 km o m\xC3\xA1s" : "Visibility 10 km or more";
    else if (vm >= 0) {
        if (vm < 5000) snprintf(b, sizeof(b), es ? "Visibilidad %d m" : "Visibility %d m", vm);
        else snprintf(b, sizeof(b), es ? "Visibilidad %d km" : "Visibility %d km", vm / 1000);
        o = b;
    } else if (m.visibSm >= 0) {
        snprintf(b, sizeof(b), es ? "Visibilidad %.0f km" : "Visibility %.0f km", m.visibSm * 1.609344);
        o = b;
    }
    if (!m.wx.empty()) { if (!o.empty()) o += " \xC2\xB7 "; o += m.wx; }
    std::string clouds;
    for (const auto& l : m.clouds) {
        if (l.cover == "CAVOK") { clouds = "CAVOK"; break; }
        if (l.cover == "CLR" || l.cover == "SKC" || l.cover == "NSC" || l.cover == "NCD") { clouds = es ? "sin nubes" : "no clouds"; break; }
        const char* n = coverName(l.cover, es);
        if (!n) continue;
        snprintf(b, sizeof(b), "%s%s %s %d ft", clouds.empty() ? "" : ", ", n, es ? "a" : "at", l.baseFt);
        clouds += b;
    }
    if (m.vertVisFt >= 0) { snprintf(b, sizeof(b), "%s%s %d ft", clouds.empty() ? "" : ", ", es ? "visibilidad vertical" : "vertical visibility", m.vertVisFt); clouds += b; }
    if (!clouds.empty()) {
        if (!o.empty()) o += " \xC2\xB7 ";
        o += (es ? "Nubes: " : "Clouds: ") + clouds;
    }
    return o;
}

std::string metarTempText(const Metar& m, bool es) {
    std::string o;
    char b[48];
    if (m.tempC > -999) { snprintf(b, sizeof(b), es ? "Temperatura %.0f \xC2\xB0" "C" : "Temperature %.0f \xC2\xB0" "C", m.tempC); o += b; }
    if (m.dewpC > -999) { snprintf(b, sizeof(b), es ? "%sRoc\xC3\xAD" "o %.0f \xC2\xB0" "C" : "%sDew point %.0f \xC2\xB0" "C", o.empty() ? "" : " \xC2\xB7 ", m.dewpC); o += b; }
    if (m.qnhHpa > 0) { snprintf(b, sizeof(b), "%sQNH %.0f hPa", o.empty() ? "" : " \xC2\xB7 ", m.qnhHpa); o += b; }
    return o;
}

const char* sigmetHazardName(const std::string& h, bool es) {
    if (h == "CONV" || h == "TS") return es ? "Tormentas" : "Thunderstorms";
    if (h == "TURB") return es ? "Turbulencia" : "Turbulence";
    if (h == "ICE") return es ? "Engelamiento" : "Icing";
    if (h == "MTW") return es ? "Ondas de monta\xC3\xB1" "a" : "Mountain waves";
    if (h == "VA") return es ? "Ceniza volc\xC3\xA1nica" : "Volcanic ash";
    if (h == "TC") return es ? "Cicl\xC3\xB3n tropical" : "Tropical cyclone";
    if (h == "DS" || h == "SS") return es ? "Tempestad de polvo o arena" : "Dust or sand storm";
    if (h == "RDOACT") return es ? "Nube radiactiva" : "Radioactive cloud";
    return h.c_str();
}

double sigmetDistanceNm(const Sigmet& s, LatLon p) {
    if (s.pts.size() >= 3 && pointInPolygon(p, s.pts)) return 0;
    double best = 1e9;
    for (const LatLon& q : s.pts) {
        const double d = gcDistanceNm(p, q);
        if (d < best) best = d;
    }
    return best;
}

}  // namespace plotter
