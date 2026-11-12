/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Interfaz del instrumento de viento (480x480).
#pragma once
#include "store.h"

namespace wui {
void begin(plotter::DataStore* store);
void update();                 // unas 3 veces por segundo
void showPage(int i);          // 0 Rosa, 1 Viento, 2 Numeros, 3 Historial, 4 Ajustes
void testStyle(int s);
void tick();                // zumbador de las alarmas: llamar en cada vuelta del bucle
void testNight();           // dibujo en el PC: modo noche
void testAlarm();           // dibujo en el PC: alarma de viento real a 10 kn
void testAbout();           // dibujo en el PC: "Acerca de"
void testWifi();            // abre la pantalla de WiFi (dibujo en el PC)         // para el dibujo en el PC: reparto de la pagina Rosa
}
