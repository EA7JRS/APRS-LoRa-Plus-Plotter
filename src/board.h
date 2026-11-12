/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Placas: Waveshare ESP32-S3-Touch-LCD-7B (plotter; 7", RGB 1024x600, GT911) y ESP32-S3-Touch-LCD-4 (instrumento de
 * viento; 4", RGB 480x480, ST7701, GT911). La 7B, tomada de APRS LoRa+ Display
 * (src/board.h, board_lcd7b.cpp, board_common.*), con lo que el plotter anade: seleccion USB / CAN y pines de CAN y RS485.
 * Pines y lo que esta sin verificar: docs/HARDWARE.md.
 */
#pragma once
#include <Arduino.h>

#if defined(BOARD_LCD7B)
#define BOARD_ENV "waveshare_lcd7b"
#define BOARD_W 1024
#define BOARD_H 600
#elif defined(BOARD_LCD4)
// Instrumento de viento: Waveshare ESP32-S3-Touch-LCD-4 (4", 480x480), revision V4.0. Ver src/board_lcd4.cpp.
#define BOARD_ENV "waveshare_lcd4_wind"
#define BOARD_W 480
#define BOARD_H 480
#else
#error "Falta la placa: -DBOARD_LCD7B (plotter) o -DBOARD_LCD4 (instrumento de viento); ver platformio.ini"
#endif

namespace board {
constexpr int W = BOARD_W, H = BOARD_H;
constexpr const char* ENV = BOARD_ENV;

// Puertos de la placa
#if defined(BOARD_LCD7B)
constexpr int PIN_CAN_TX = 20, PIN_CAN_RX = 19;       // compartidos con el USB nativo: ver setCanMode()
constexpr int PIN_RS485_RX = 16, PIN_RS485_TX = 15;   // el transceptor conmuta solo el sentido
constexpr int PIN_SD_MOSI = 11, PIN_SD_SCK = 12, PIN_SD_MISO = 13;   // CS por el expansor: sdSelect()
#else
constexpr int PIN_CAN_TX = 6, PIN_CAN_RX = 0;         // transceptor TJA1051; no comparte pines con el USB
// La tabla de Waveshare da GPIO43 como RX y GPIO44 como TX del RS485; sin verificar en la placa (Ajustes los intercambia)
constexpr int PIN_RS485_RX = 43, PIN_RS485_TX = 44;
// Zumbador de la placa (por el expansor)
void buzzer(bool on);
#endif

// Arranca alimentacion, expansor, panel, tactil y luz de fondo. false si algo falla (el motivo sale por el puerto serie).
bool begin();

// Buffer de dibujo de LVGL (PSRAM). Lineas por buffer: drawLines().
void* allocDrawBuffer(size_t bytes);
int drawLines();
bool swapBytes();

// Vuelca un rectangulo RGB565 al panel y avisa con la funcion de setDrawDone() cuando el buffer se puede reutilizar.
void setDrawDone(void (*cb)());
void draw(int x1, int y1, int x2, int y2, void* pixels);

// Tactil: 1 = dedo puesto (x, y en pixeles de la pantalla), 0 = soltado, -1 = sin dato nuevo.
int touch(int& x, int& y);

// Luz de fondo, 0..100 (0 = apagada).
void setBacklight(int percent);

#if defined(BOARD_LCD7B)
// Los pines 19 / 20 van al USB nativo o al transceptor CAN segun EXIO5 del expansor (alto = CAN).
bool setCanMode(bool can);
// CS de la tarjeta TF (activo a nivel bajo) por el expansor.
bool sdSelect(bool selected);
// Monta la tarjeta TF (SPI). El CS va por el expansor y se deja activo; la libreria SD necesita un pin propio y se le da
// uno sin usar (SD_DUMMY_CS). false si no hay tarjeta o no se puede leer.
bool sdBegin();
#endif
}
