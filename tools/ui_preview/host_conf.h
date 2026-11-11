/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Ajustes de LVGL para dibujar la interfaz en el PC: lo mismo que platformio.ini, pero la "tarjeta" S: es una carpeta.
#pragma once
#define LV_USE_LODEPNG 1
#define LV_USE_TJPGD 1
#define LV_USE_FS_POSIX 1
#define LV_FS_POSIX_LETTER 'S'
#define LV_FS_POSIX_PATH "/tmp/plotter_sd"
#define LV_CACHE_DEF_SIZE 8388608
#define LV_IMAGE_HEADER_CACHE_DEF_CNT 64
