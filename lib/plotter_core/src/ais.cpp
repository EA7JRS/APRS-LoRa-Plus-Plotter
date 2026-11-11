/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ais.h"

#include <cstdlib>
#include <cstring>

#include "geo.h"

namespace plotter {

static int sixbit(char c) {
    int v = c - 48;
    if (v > 40) v -= 8;
    return (v < 0 || v > 63) ? -1 : v;
}

static uint32_t ubits(const uint8_t* six, int start, int len) {
    uint32_t r = 0;
    for (int i = start; i < start + len; i++) r = (r << 1) | ((six[i / 6] >> (5 - i % 6)) & 1);
    return r;
}

static int32_t sbits(const uint8_t* six, int start, int len) {
    uint32_t u = ubits(six, start, len);
    if (u & (1u << (len - 1))) u |= ~((1u << len) - 1);
    return int32_t(u);
}

static void text(const uint8_t* six, int start, int chars, char* out) {
    for (int i = 0; i < chars; i++) {
        int c = ubits(six, start + i * 6, 6);
        if (c == 0) { chars = i; break; }                         // '@' = fin del texto (como gpsd); lo que sigue es relleno
        out[i] = char(c < 32 ? c + 64 : c);
    }
    out[chars] = 0;
    for (int i = chars - 1; i >= 0 && out[i] == ' '; i--) out[i] = 0;
}

static void position(const uint8_t* six, int lonBit, AisReport& r) {
    int32_t lon = sbits(six, lonBit, 28), lat = sbits(six, lonBit + 28, 27);
    if (lon != 0x6791AC0 && lat != 0x3412140) {                    // 181 y 91 grados = sin dato
        r.lon = lon / 600000.0;
        r.lat = lat / 600000.0;
        r.hasPos = true;
    }
}

static void motion(const uint8_t* six, int sogBit, int cogBit, int hdgBit, AisReport& r) {
    uint32_t sog = ubits(six, sogBit, 10), cog = ubits(six, cogBit, 12), hdg = ubits(six, hdgBit, 9);
    if (sog != 1023) { r.sogKn = sog / 10.0; r.hasSog = true; }
    if (cog < 3600) { r.cogDeg = cog / 10.0; r.hasCog = true; }
    if (hdg < 360) { r.heading = int(hdg); r.hasHeading = true; }
}

static void dims(const uint8_t* six, int bit, AisReport& r) {
    r.toBow = int(ubits(six, bit, 9));
    r.toStern = int(ubits(six, bit + 9, 9));
    r.toPort = int(ubits(six, bit + 18, 6));
    r.toStarboard = int(ubits(six, bit + 24, 6));
    r.hasDim = (r.toBow + r.toStern) > 0 || (r.toPort + r.toStarboard) > 0;
}

// Bits que fija la norma para los mensajes de longitud fija (0 = variable)
static int nominalBits(int type) {
    switch (type) {
        case 1: case 2: case 3: case 4: case 9: case 18: return 168;
        case 5: return 424;
        case 19: return 312;
        case 27: return 96;
        default: return 0;
    }
}

AisPosVerdict aisJudgePosition(bool known, double lat0, double lon0, bool wasStopped, long seconds, double lat, double lon,
                               bool aircraft) {
    seconds = std::labs(seconds);
    if (!known || seconds > 1800) return AisPosVerdict::Unknown;             // media hora despues la anterior no dice nada
    double d = gcDistanceNm({lat0, lon0}, {lat, lon});
    double maxKn = aircraft ? 600.0 : 60.0;
    if (d > maxKn * (seconds + 1) / 3600.0 + 0.5) return AisPosVerdict::Jump;   // 0,5 mn de margen: redondeo y segundos enteros
    if (!aircraft && wasStopped && seconds <= 900 && d > 200.0 / 1852.0) return AisPosVerdict::Jump;
    return AisPosVerdict::Valid;
}

bool AisDecoder::decode(const uint8_t* six, int nbits, bool own, AisReport& r) {
    err_ = AisError::Undersized;
    if (nbits < 38) return false;
    r = AisReport();
    r.own = own;
    r.type = int(ubits(six, 0, 6));
    r.mmsi = ubits(six, 8, 30);
    if (r.mmsi == 0) { err_ = AisError::BadMmsi; return false; }
    int nominal = nominalBits(r.type);
    r.oversized = nominal > 0 && nbits > nominal;
    bool ok = false;
    switch (r.type) {
        case 1: case 2: case 3:
            if (nbits < 137) return false;
            r.navStatus = int(ubits(six, 38, 4));
            position(six, 61, r);
            motion(six, 50, 116, 128, r);
            ok = true;
            break;
        case 4:
            if (nbits < 134) return false;
            r.kind = AisKind::BaseStation;
            position(six, 79, r);
            ok = true;
            break;
        case 9: {
            if (nbits < 128) return false;
            r.kind = AisKind::SarAircraft;
            uint32_t alt = ubits(six, 38, 12), sog = ubits(six, 50, 10), cog = ubits(six, 116, 12);
            if (alt != 4095) { r.altM = int(alt); r.hasAlt = true; }
            if (sog != 1023) { r.sogKn = sog; r.hasSog = true; }              // nudos enteros
            if (cog < 3600) { r.cogDeg = cog / 10.0; r.hasCog = true; }
            position(six, 61, r);
            ok = true;
            break;
        }
        case 12: case 14: {
            int start = r.type == 12 ? 72 : 40;
            if (nbits < start) return false;
            r.kind = AisKind::Safety;
            if (r.type == 12) r.destMmsi = ubits(six, 40, 30);
            int chars = (nbits - start) / 6;
            if (chars > 161) chars = 161;
            text(six, start, chars, r.text);
            ok = true;
            break;
        }
        case 18:
            if (nbits < 133) return false;
            position(six, 57, r);
            motion(six, 46, 112, 124, r);
            ok = true;
            break;
        case 19:
            if (nbits < 271) return false;
            position(six, 57, r);
            motion(six, 46, 112, 124, r);
            text(six, 143, 20, r.name);
            r.shipType = int(ubits(six, 263, 8));
            if (nbits >= 301) dims(six, 271, r);
            r.hasStatic = true;
            ok = true;
            break;
        case 5:
            if (nbits < 240) return false;
            text(six, 70, 7, r.callsign);
            text(six, 112, 20, r.name);
            r.shipType = int(ubits(six, 232, 8));
            if (nbits >= 270) dims(six, 240, r);
            if (nbits >= 422) {
                r.draughtM = ubits(six, 294, 8) / 10.0;
                text(six, 302, 20, r.destination);
                r.hasVoyage = true;
            }
            r.hasStatic = true;
            ok = true;
            break;
        case 21: {
            if (nbits < 272) return false;
            r.kind = AisKind::Aton;
            r.atonType = int(ubits(six, 38, 5));
            text(six, 43, 20, r.name);
            position(six, 164, r);
            dims(six, 219, r);
            r.atonOffPosition = ubits(six, 259, 1) != 0;
            r.atonVirtual = ubits(six, 269, 1) != 0;
            int ext = (nbits - 272) / 6;                    // ampliacion del nombre
            if (ext > 14) ext = 14;
            if (ext > 0 && std::strlen(r.name) == 20) text(six, 272, ext, r.name + 20);
            r.hasStatic = true;
            ok = true;
            break;
        }
        case 24: {
            if (nbits < 160) return false;
            int part = int(ubits(six, 38, 2));
            if (part == 0) text(six, 40, 20, r.name);
            else if (part == 1) {
                r.shipType = int(ubits(six, 40, 8));
                text(six, 90, 7, r.callsign);
                if (nbits >= 162 && r.mmsi / 10000000 != 98) dims(six, 132, r);     // 98xxxxxxx: embarcacion auxiliar, ahi va el MMSI nodriza
            } else { err_ = AisError::Unsupported; return false; }
            r.hasStatic = true;
            ok = true;
            break;
        }
        case 27: {
            if (nbits < 96) return false;
            r.navStatus = int(ubits(six, 40, 4));
            int32_t lon = sbits(six, 44, 18), lat = sbits(six, 62, 17);      // decimas de minuto
            if (lon != 181 * 600 && lat != 91 * 600) { r.lon = lon / 600.0; r.lat = lat / 600.0; r.hasPos = true; }
            uint32_t sog = ubits(six, 79, 6), cog = ubits(six, 85, 9);
            if (sog != 63) { r.sogKn = sog; r.hasSog = true; }
            if (cog < 360) { r.cogDeg = cog; r.hasCog = true; }
            ok = true;
            break;
        }
        default:
            err_ = AisError::Unsupported;
            return false;
    }
    if (ok) err_ = AisError::None;
    return ok;
}

bool AisDecoder::feed(int total, int num, int seqId, const char* payload, int fillBits, bool own, AisReport& out) {
    err_ = AisError::Fragment;
    if (total < 1 || num < 1 || num > total || !payload) return false;
    if (num == 1) { len_ = 0; seq_ = seqId; next_ = 1; total_ = total; }
    if (num != next_ || seqId != seq_ || total != total_) { seq_ = -2; next_ = 1; len_ = 0; return false; }   // fragmento fuera de orden
    for (const char* p = payload; *p; p++) {
        int v = sixbit(*p);
        if (v < 0 || len_ >= kMaxChars) { seq_ = -2; next_ = 1; len_ = 0; err_ = AisError::BadChar; return false; }
        buf_[len_++] = uint8_t(v);
    }
    if (num < total) { next_++; err_ = AisError::Pending; return false; }
    int nbits = len_ * 6 - (fillBits >= 0 && fillBits <= 5 ? fillBits : 0);
    seq_ = -2; next_ = 1;
    int n = len_;
    len_ = 0;
    // Margen de ceros para lecturas de campos opcionales
    for (int i = n; i < kMaxChars + 8 && i < n + 8; i++) buf_[i] = 0;
    return decode(buf_, nbits, own, out);
}

}  // namespace plotter
