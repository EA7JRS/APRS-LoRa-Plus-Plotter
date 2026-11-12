/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Pantalla de bienvenida de APRS LoRa+ (la de la app Android y APRS LoRa+ Display), del tamano de cada placa.
// src/ui_assets.cpp lo genera tools/make_ui_assets.py.
#pragma once
#include <lvgl.h>

extern const lv_image_dsc_t img_splash;   // logo de bienvenida: 540 lineas en la 7B, 400 en la LCD-4
extern const uint32_t SPLASH_BG_RGB;      // su fondo (#131A22)

// Bienvenida al arrancar: el logo sobre #131A22 entra con fundido, debajo el producto y la version en el cian del logo,
// y a los 2,2 s pasa con fundido a la pantalla principal (ya montada) y se borra. Como en APRS LoRa+ Display.
inline void showSplash(lv_obj_t* mainScr, const char* text, const lv_font_t* font) {
    lv_obj_t* sp = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(sp, lv_color_hex(SPLASH_BG_RGB), 0);
    lv_obj_set_style_bg_opa(sp, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(sp, false);
    lv_obj_t* img = lv_image_create(sp);
    lv_image_set_src(img, &img_splash);
    const int h = img_splash.header.h, lh = lv_font_get_line_height(font);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -(lh + 4) / 2);
    lv_obj_t* ver = lv_label_create(sp);
    lv_label_set_text(ver, text);
    lv_obj_set_style_text_font(ver, font, 0);
    lv_obj_set_style_text_color(ver, lv_color_hex(0x28B5E1), 0);
    lv_obj_align(ver, LV_ALIGN_CENTER, 0, -(lh + 4) / 2 + h / 2 + 4 + lh / 2);
    lv_screen_load(sp);
    lv_obj_fade_in(img, 700, 0);
    lv_obj_fade_in(ver, 700, 300);
    lv_screen_load_anim(mainScr, LV_SCREEN_LOAD_ANIM_FADE_IN, 500, 2200, true);
}
