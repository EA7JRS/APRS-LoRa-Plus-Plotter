/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz del plotter (LVGL 9). Fase 1: barra superior, pestanas, Instrumentos, Carta (sin teselas todavia), Radar AIS y ventana de alerta de abordaje.
 */
#pragma once
#include "store.h"

namespace ui {
void begin(plotter::DataStore* store);
void update();              // refresca los numeros; llamar unas 3 veces por segundo
void closeWindow();         // cierra la ventana centrada, como el boton CLOSE / Cerrar
void showPage(int tab);
void testTapChart(int x, int y);   // toque en la carta, en pixeles de la carta
void testPanelButton(int b);       // boton 0, 1 o 2 del panel de la carta
void testMob();
void testMark();     // 0 Instrumentos ... 7 Ajustes (para las pruebas en el PC)
}
