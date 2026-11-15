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
void testRs485();                   // abre el aviso de RS485 compartido
void testInternet();                // mete una observacion de internet de prueba
void testNet();                     // abre la pantalla de red
void testAbout();                   // abre "Acerca de"
void testAstroList();                                  // Astro > Visibles (dibujo del PC)
void testBearings(int mode);                            // Carta > Demoras: 0 simultaneas a dos luces, 1 trasladada a una
void closeBearings();
void testCurrent(int mode, int mins);                   // Carta > Corriente: 0 rumbo a gobernar, 1 corriente por la estima (salida hace mins)
void testAstro(const char* star, double errArcmin, bool latitude = false);   // latitude: boton Latitud en vez de trazar   // abre Carta > Astro y traza una recta ("" = el Sol)
void closeAstro();
void testCt(const char* star, double dev, int step);   // Astro > Desvio (Ct) con un compas simulado; step > 0 llena la tablilla
void testCtCard();
void testLights(int zoom, int pick);                   // vuelve a leer S:/luces.txt, centra y elige una luz
void testDsc(bool list);                               // llamadas LSD de ejemplo; list abre la lista
void testNavtex(bool list);                            // mensajes NAVTEX de ejemplo; list abre la lista                                     // abre la tablilla
void testVessel();                  // abre Ajustes > Embarcacion
void testSetting(int what);         // 4 cambia de unidades, 5 de mapa base
void testAnchor();                  // boton Fondeo de la carta
void testRoutes(int action);       // botones de la pestana Rutas (4 a la ruta, 6 navegar, 8 vaciar)
void testSelectWaypoint(int i);
void testMark();     // 0 Instrumentos ... 7 Ajustes (para las pruebas en el PC)
}
