/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz del plotter (LVGL 9). Fase 1, primer paso: barra superior, pestanas y pantalla de Instrumentos.
 */
#pragma once
#include "store.h"

namespace ui {
void begin(plotter::DataStore* store);
void update();              // refresca los numeros; llamar unas 3 veces por segundo
}
