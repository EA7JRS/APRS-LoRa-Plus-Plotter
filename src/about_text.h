/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Textos y QR de la pagina "Acerca de" del plotter y del instrumento de viento (los mismos en los dos).
#pragma once

#include <lvgl.h>
#include <string.h>

#ifndef PLOTTER_VERSION                 // la pone platformio.ini; en el dibujo del PC no esta
#define PLOTTER_VERSION "5.4.5"
#endif

namespace about {
constexpr const char* kCopyright = "APRS LoRa+ \xC2\xB7 \xC2\xA9 2026 Manuel - EA7JRS";
constexpr const char* kLicense = "Software libre: GPL-3.0-or-later. Sin garant\xC3\xAD" "a. Es una ayuda a la navegaci\xC3\xB3n: no sustituye a la carta oficial ni a la vigilancia.";
constexpr const char* kProject = "ea7jrs.github.io/APRS-LoRa-Plus";
constexpr const char* kDonate = "https://paypal.me/ea7jrs";
constexpr const char* kDonateText = "Donativo: paypal.me/ea7jrs";
constexpr const char* kScan = "Escanea el c\xC3\xB3" "digo QR para donar";
constexpr const char* kCredits =
    "LVGL (MIT) \xC2\xB7 NimBLE-Arduino (Apache-2.0) \xC2\xB7 ArduinoJson (MIT) \xC2\xB7 Arduino-ESP32 (LGPL-2.1) y "
    "ESP-IDF (Apache-2.0) \xC2\xB7 lodepng (zlib) \xC2\xB7 TJpgDec (ChaN) \xC2\xB7 QR Code generator (MIT) \xC2\xB7 fuentes "
    "DejaVu (licencia de Bitstream Vera) y Montserrat (OFL)";
constexpr const char* kChartCredits =
    "Cartas: \xC2\xA9 OpenStreetMap y OpenSeaMap (ODbL) \xC2\xB7 IGN y PNOA, scne.es (CC BY 4.0) \xC2\xB7 IHM (solo para "
    "ver, no v\xC3\xA1lida para la navegaci\xC3\xB3n) \xC2\xB7 tiempo: OpenWeatherMap. Formatos NMEA 2000 y AIS contrastados con "
    "canboat y gpsd.";

// El QR del donativo, como en APRS LoRa+ Display: marco con el degradado del logo (verde -> cian) por fuera del margen
// blanco que piden los lectores, y los modulos del color oscuro de la pantalla sobre blanco. Devuelve el marco.
inline lv_obj_t* donateQr(lv_obj_t* parent, int size, uint32_t dark) {
    const int ring = size >= 120 ? 5 : 4;
    lv_obj_t* qw = lv_obj_create(parent);
    lv_obj_remove_style_all(qw);
    lv_obj_set_size(qw, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(qw, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(qw, lv_color_hex(0x3CE68A), 0);
    lv_obj_set_style_bg_grad_color(qw, lv_color_hex(0x28B5E1), 0);
    lv_obj_set_style_bg_grad_dir(qw, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_pad_all(qw, ring, 0);
    lv_obj_set_style_radius(qw, ring, 0);
    lv_obj_t* qr = lv_qrcode_create(qw);
    lv_qrcode_set_size(qr, size);
    lv_qrcode_set_dark_color(qr, lv_color_hex(dark));
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_update(qr, kDonate, (uint32_t)strlen(kDonate));
    lv_obj_set_style_border_color(qr, lv_color_white(), 0);
    lv_obj_set_style_border_width(qr, size >= 120 ? 6 : 4, 0);
    return qw;
}
}
