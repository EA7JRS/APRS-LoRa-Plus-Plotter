/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "tiles.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace plotter {

void lonLatToWorldPx(LatLon p, int zoom, double& px, double& py) {
    double lat = p.lat;
    if (lat > kMercatorMaxLat) lat = kMercatorMaxLat;
    if (lat < -kMercatorMaxLat) lat = -kMercatorMaxLat;
    double n = double(kTileSize) * double(1u << zoom);
    px = (p.lon + 180.0) / 360.0 * n;
    double s = std::sin(deg2rad(lat));
    py = (0.5 - std::log((1 + s) / (1 - s)) / (4 * kPi)) * n;
}

LatLon worldPxToLonLat(double px, double py, int zoom) {
    double n = double(kTileSize) * double(1u << zoom);
    double lon = px / n * 360.0 - 180.0;
    double y = 0.5 - py / n;
    double lat = 90.0 - 360.0 * std::atan(std::exp(-y * 2 * kPi)) / kPi;
    return {lat, lon};
}

void lonLatToTile(LatLon p, int zoom, int& tx, int& ty, int* inX, int* inY) {
    double px, py;
    lonLatToWorldPx(p, zoom, px, py);
    int max = (1 << zoom) - 1;
    tx = int(std::floor(px / kTileSize));
    ty = int(std::floor(py / kTileSize));
    if (tx < 0) tx = 0;
    if (tx > max) tx = max;
    if (ty < 0) ty = 0;
    if (ty > max) ty = max;
    if (inX) *inX = int(px) - tx * kTileSize;
    if (inY) *inY = int(py) - ty * kTileSize;
}

double metersPerPixel(double lat, int zoom) {
    return 2 * kPi * 6378137.0 * std::cos(deg2rad(lat)) / (double(kTileSize) * double(1u << zoom));
}

int zoomForWidth(double lat, double widthNm, int widthPx, int maxZoom) {
    for (int z = maxZoom; z > 0; z--)
        if (metersPerPixel(lat, z) * widthPx >= widthNm * kMetersPerNm) return z;
    return 0;
}

int tileUrl(char* out, int size, const char* t, int zoom, int tx, int ty) {
    if (!t || !std::strstr(t, "{x}")) return 0;
    int n = 0;
    for (const char* p = t; *p;) {
        char num[16];
        const char* rep = nullptr;
        int skip = 0;
        if (!std::strncmp(p, "{z}", 3)) { std::snprintf(num, sizeof(num), "%d", zoom); rep = num; skip = 3; }
        else if (!std::strncmp(p, "{x}", 3)) { std::snprintf(num, sizeof(num), "%d", tx); rep = num; skip = 3; }
        else if (!std::strncmp(p, "{y}", 3)) { std::snprintf(num, sizeof(num), "%d", ty); rep = num; skip = 3; }
        else if (!std::strncmp(p, "{-y}", 4)) { std::snprintf(num, sizeof(num), "%d", tmsY(ty, zoom)); rep = num; skip = 4; }
        if (rep) {
            int l = int(std::strlen(rep));
            if (n + l >= size) return 0;
            std::memcpy(out + n, rep, size_t(l));
            n += l;
            p += skip;
        } else {
            if (n + 1 >= size) return 0;
            out[n++] = *p++;
        }
    }
    out[n] = 0;
    return n;
}

int tilePath(char* out, int size, const char* folder, int zoom, int tx, int ty, const char* ext) {
    int n = std::snprintf(out, size_t(size), "%s/%d/%d/%d.%s", folder ? folder : "", zoom, tx, ty, ext ? ext : "png");
    return (n > 0 && n < size) ? n : 0;
}

}  // namespace plotter
