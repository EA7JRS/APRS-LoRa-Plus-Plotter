/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz del plotter (LVGL 9). Fase 1: barra superior, pestanas, Instrumentos, Carta, Rutas, Radar AIS, Ajustes y ventanas de alerta.
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
void testNet();                     // abre la pantalla de red
void testSetting(int what);         // 4 cambia de unidades, 5 de mapa base
void testAnchor();                  // boton Fondeo de la carta
void testRoutes(int action);       // botones de la pestana Rutas (4 a la ruta, 6 navegar, 8 vaciar)
void testSelectWaypoint(int i);
void testMark();     // 0 Instrumentos ... 7 Ajustes (para las pruebas en el PC)
}
