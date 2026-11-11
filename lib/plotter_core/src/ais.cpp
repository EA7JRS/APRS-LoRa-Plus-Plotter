/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ais.h"

#include <cstring>

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
        out[i] = char(c < 32 ? c + 64 : c);
    }
    out[chars] = 0;
    for (int i = chars - 1; i >= 0 && (out[i] == '@' || out[i] == ' '); i--) out[i] = 0;      // relleno
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

bool AisDecoder::decode(const uint8_t* six, int nbits, bool own, AisReport& r) {
    if (nbits < 38) return false;
    r = AisReport();
    r.own = own;
    r.type = int(ubits(six, 0, 6));
    r.mmsi = ubits(six, 8, 30);
    switch (r.type) {
        case 1: case 2: case 3:
            if (nbits < 137) return false;
            r.navStatus = int(ubits(six, 38, 4));
            position(six, 61, r);
            motion(six, 50, 116, 128, r);
            return true;
        case 18:
            if (nbits < 133) return false;
            position(six, 57, r);
            motion(six, 46, 112, 124, r);
            return true;
        case 19:
            if (nbits < 271) return false;
            position(six, 57, r);
            motion(six, 46, 112, 124, r);
            text(six, 143, 20, r.name);
            r.shipType = int(ubits(six, 263, 8));
            r.hasStatic = true;
            return true;
        case 5:
            if (nbits < 240) return false;
            text(six, 70, 7, r.callsign);
            text(six, 112, 20, r.name);
            r.shipType = int(ubits(six, 232, 8));
            r.hasStatic = true;
            return true;
        case 24: {
            if (nbits < 160) return false;
            int part = int(ubits(six, 38, 2));
            if (part == 0) text(six, 40, 20, r.name);
            else if (part == 1) {
                r.shipType = int(ubits(six, 40, 8));
                if (nbits >= 132) text(six, 90, 7, r.callsign);
            } else return false;
            r.hasStatic = true;
            return true;
        }
        default:
            return false;
    }
}

bool AisDecoder::feed(int total, int num, int seqId, const char* payload, int fillBits, bool own, AisReport& out) {
    if (total < 1 || num < 1 || num > total || !payload) return false;
    if (num == 1) { len_ = 0; seq_ = seqId; next_ = 1; total_ = total; }
    if (num != next_ || seqId != seq_ || total != total_) { seq_ = -2; next_ = 1; len_ = 0; return false; }   // fragmento fuera de orden
    for (const char* p = payload; *p; p++) {
        int v = sixbit(*p);
        if (v < 0 || len_ >= kMaxChars) { seq_ = -2; next_ = 1; len_ = 0; return false; }
        buf_[len_++] = uint8_t(v);
    }
    if (num < total) { next_++; return false; }
    int nbits = len_ * 6 - (fillBits >= 0 && fillBits <= 5 ? fillBits : 0);
    seq_ = -2; next_ = 1;
    int n = len_;
    len_ = 0;
    // Margen de ceros para lecturas de campos opcionales
    for (int i = n; i < kMaxChars && i < n + 8; i++) buf_[i] = 0;
    return decode(buf_, nbits, own, out);
}

}  // namespace plotter
