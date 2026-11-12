/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Waveshare ESP32-S3-Touch-LCD-4 (SKU 28154), revision V4.0: ST7701 con bus RGB de 16 bits (480x480) que se configura por
 * SPI de 3 hilos (9 bits; CS GPIO42, SCL GPIO2, SDA GPIO1), tactil GT911 y expansor CH32V003 en 0x24 (el mismo de la 7B)
 * que lleva los reset, el zumbador y el PWM de la luz de fondo. I2C en GPIO15 (SDA) y GPIO7 (SCL).
 * Pines y secuencia del ST7701 del BSP de Waveshare (waveshareteam/Waveshare-ESP32-components, bsp/esp32_s3_touch_lcd_4);
 * los pines del bus RGB coinciden con el modelo WAVESHARE-4-480X480 de ESPHome. Los tiempos del panel son los de
 * ST7701_480_480_PANEL_60HZ_RGB_TIMING de esp_lcd_st7701, puestos de memoria: sin verificar.
 * Las revisiones anteriores de la placa llevan otro expansor (TCA9554 en 0x20) y no estan soportadas.
 * Sin probar en la placa.
 */
#include "board.h"
#if defined(BOARD_LCD4)
#include "board_common.h"
#include <Wire.h>

#ifndef LCD_PCLK_HZ
#define LCD_PCLK_HZ 16000000
#endif

namespace board {
using namespace common;

constexpr int PIN_SDA = 15, PIN_SCL = 7;
constexpr int PIN_SPI_CS = 42, PIN_SPI_SCK = 2, PIN_SPI_SDA = 1;
constexpr int PIN_HSYNC = 38, PIN_VSYNC = 39, PIN_DE = 40, PIN_PCLK = 41;
// data[0..4] = B0..B4, data[5..10] = G0..G5, data[11..15] = R0..R4
constexpr int DATA_PINS[16] = {5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17};

// Expansor: 0x02 modo (1 = salida), 0x03 salidas, 0x05 PWM de la luz (al reves: 0 = maximo). EXIOn = bit n.
// EXIO1 reset tactil, EXIO2 INT del tactil (entrada), EXIO3 reset LCD, EXIO5 SYS_EN, EXIO6 zumbador, EXIO7 INT del reloj.
constexpr uint8_t IOX = 0x24, IOX_MODE = 0x02, IOX_OUT = 0x03, IOX_PWM = 0x05;
constexpr uint8_t EXIO_TP_RST = 1 << 1, EXIO_LCD_RST = 1 << 3, EXIO_SYS_EN = 1 << 5, EXIO_BUZZER = 1 << 6;
static uint8_t ioxOut = 0;
static bool ioxSet(uint8_t mask, bool on) {
    if (on) ioxOut |= mask; else ioxOut &= ~mask;
    return i2cWrite(IOX, IOX_OUT, ioxOut);
}

// Secuencia del ST7701 del BSP de Waveshare: {orden, n, datos}; n = 0xFF es una pausa de d[0] * 10 ms
struct St7701Cmd { uint8_t cmd, n; uint8_t d[16]; };
static const St7701Cmd ST7701[] = {
    {0x11, 0, {0}}, {0x00, 0xFF, {12}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x10}}, {0xC0, 2, {0x3B, 0x00}}, {0xC1, 2, {0x0D, 0x02}}, {0xC2, 2, {0x21, 0x08}}, {0xCD, 1, {0x08}},
    {0xB0, 16, {0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18}},
    {0xB1, 16, {0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x11}},
    {0xB0, 1, {0x60}}, {0xB1, 1, {0x30}}, {0xB2, 1, {0x87}}, {0xB3, 1, {0x80}}, {0xB5, 1, {0x49}}, {0xB7, 1, {0x85}}, {0xB8, 1, {0x21}},
    {0xC1, 1, {0x78}}, {0xC2, 1, {0x78}}, {0x00, 0xFF, {2}},
    {0xE0, 3, {0x00, 0x1B, 0x02}},
    {0xE1, 11, {0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44}},
    {0xE2, 12, {0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00}},
    {0xE3, 4, {0x00, 0x00, 0x11, 0x11}}, {0xE4, 2, {0x44, 0x44}},
    {0xE5, 16, {0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0}},
    {0xE6, 4, {0x00, 0x00, 0x11, 0x11}}, {0xE7, 2, {0x44, 0x44}},
    {0xE8, 16, {0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0}},
    {0xEB, 7, {0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40}},
    {0xEC, 2, {0x3C, 0x00}},
    {0xED, 16, {0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}},
    {0x36, 1, {0x00}},                                            // MADCTL: sin giros
    {0x3A, 1, {0x66}},                                            // formato de pixel, como el BSP
    {0x21, 0, {0}}, {0x00, 0xFF, {12}},                           // colores invertidos (panel IPS)
    {0x29, 0, {0}},
};

// SPI de 3 hilos por software: bit D/C (0 orden, 1 dato) y 8 bits, el mas alto primero; se lee en el flanco de subida
static void spi9(bool data, uint8_t v) {
    uint16_t w = (data ? 0x100 : 0) | v;
    for (int i = 8; i >= 0; i--) {
        digitalWrite(PIN_SPI_SCK, LOW);
        digitalWrite(PIN_SPI_SDA, (w >> i) & 1);
        delayMicroseconds(1);
        digitalWrite(PIN_SPI_SCK, HIGH);
        delayMicroseconds(1);
    }
}

static void st7701Init() {
    pinMode(PIN_SPI_CS, OUTPUT); pinMode(PIN_SPI_SCK, OUTPUT); pinMode(PIN_SPI_SDA, OUTPUT);
    digitalWrite(PIN_SPI_CS, HIGH); digitalWrite(PIN_SPI_SCK, HIGH);
    for (const St7701Cmd& c : ST7701) {
        if (c.n == 0xFF) { delay(c.d[0] * 10); continue; }
        digitalWrite(PIN_SPI_CS, LOW);                            // CS por orden, como el SPI de 3 hilos de esp_lcd
        spi9(false, c.cmd);
        for (int i = 0; i < c.n; i++) spi9(true, c.d[i]);
        digitalWrite(PIN_SPI_CS, HIGH);
        delayMicroseconds(5);
    }
}

static uint8_t gtAddr = 0x5D;
static void (*doneCb)() = nullptr;

bool begin() {
    Wire.begin(PIN_SDA, PIN_SCL, 400000);
    delay(10);
    i2cScan();                                                    // deben salir 0x24 (expansor), 0x5D o 0x14 (tactil) y 0x51 (reloj)

    if (!i2cProbe(IOX)) {
        if (i2cProbe(0x20)) Serial.println("[board] esta placa lleva el expansor TCA9554 (revision anterior a la V4.0): no soportada todavia");
        else Serial.println("[board] el expansor (0x24) no responde");
        return false;
    }
    // salidas: reset del tactil y del LCD, SYS_EN y zumbador; el resto, entradas
    i2cWrite(IOX, IOX_MODE, EXIO_TP_RST | EXIO_LCD_RST | EXIO_SYS_EN | EXIO_BUZZER);
    ioxOut = 0;                                                   // zumbador callado, LCD y tactil en reset
    i2cWrite(IOX, IOX_OUT, ioxOut);
    delay(200);
    ioxSet(EXIO_SYS_EN | EXIO_LCD_RST | EXIO_TP_RST, true);
    delay(200);

    // El INT del tactil va por el expansor: no se puede fijar la direccion en el reset, asi que se busca
    if (!i2cProbe(0x5D)) {
        if (i2cProbe(0x14)) gtAddr = 0x14;
        else Serial.println("[board] aviso: el tactil GT911 no responde");
    }

    st7701Init();

    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    cfg.timings.pclk_hz = LCD_PCLK_HZ;
    cfg.timings.h_res = W;
    cfg.timings.v_res = H;
    cfg.timings.hsync_pulse_width = 10;
    cfg.timings.hsync_back_porch = 10;
    cfg.timings.hsync_front_porch = 20;
    cfg.timings.vsync_pulse_width = 10;
    cfg.timings.vsync_back_porch = 10;
    cfg.timings.vsync_front_porch = 10;
    cfg.timings.flags.pclk_active_neg = 0;
    cfg.data_width = 16;
    cfg.bits_per_pixel = 16;
    cfg.num_fbs = 1;
    cfg.bounce_buffer_size_px = W * 10;
    cfg.hsync_gpio_num = PIN_HSYNC;
    cfg.vsync_gpio_num = PIN_VSYNC;
    cfg.de_gpio_num = PIN_DE;
    cfg.pclk_gpio_num = PIN_PCLK;
    cfg.disp_gpio_num = -1;
    for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = DATA_PINS[i];
    cfg.flags.fb_in_psram = 1;
    if (!rgbPanelBegin(cfg)) return false;

    setBacklight(100);
    Serial.printf("[board] LCD-4: panel listo (pclk %u Hz), tactil en 0x%02X\n", (unsigned)LCD_PCLK_HZ, gtAddr);
    return true;
}

void* allocDrawBuffer(size_t bytes) { return heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM); }
int drawLines() { return 60; }
bool swapBytes() { return false; }
void setDrawDone(void (*cb)()) { doneCb = cb; }
void draw(int x1, int y1, int x2, int y2, void* px) {
    rgbPanelDraw(x1, y1, x2, y2, px);
    if (doneCb) doneCb();
}
int touch(int& x, int& y) { return gt911Read(gtAddr, x, y, W, H); }

void setBacklight(int percent) {
    percent = constrain(percent, 0, 100);
    int off = 100 - percent;                                      // PWM al reves, como bsp_display_brightness_set()
    if (off > 97) off = 97;                                       // margen de seguridad que usa Waveshare en la 7B
    i2cWrite(IOX, IOX_PWM, (uint8_t)(off * 255 / 100));
}

void buzzer(bool on) { ioxSet(EXIO_BUZZER, on); }

}
#endif
