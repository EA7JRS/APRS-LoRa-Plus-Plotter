# Placa: Waveshare ESP32-S3-Touch-LCD-7B

Arranque de panel, táctil y expansor tomado de APRS LoRa+ Display (`src/board_lcd7b.cpp`, `src/board_common.*`), que ya
funciona en la 7B. Lo que el plotter añade está sin probar en la placa.

| Función | Pines |
|---|---|
| Panel RGB 1024×600 | VSYNC 3, HSYNC 46, DE 5, PCLK 7; datos en `board_lcd7b.cpp` |
| Táctil GT911 | SDA 8, SCL 9 (I2C compartido), INT 4, dirección 0x5D |
| Expansor de E/S | I2C 0x24 (mismo bus) |
| Tarjeta TF | MOSI 11, SCK 12, MISO 13, CS por el expansor (EXIO4) |
| CAN (NMEA 2000) | TX 20, RX 19, **compartidos con el USB nativo**; EXIO5 alto = CAN |
| RS485 | RXD 16, TXD 15 |
| Consola | UART0 por el CH343 (115200) |

## Entradas de datos (estado)

| Entrada | Estado |
|---|---|
| Simulador | Hecho. Encendido al arrancar; `SIM 0` / `SIM 1` por la consola |
| RS485 a 38400 (AIS) o 4800 (NMEA) | Código hecho (`src/links.cpp`), **sin probar en la placa** |
| NMEA 2000 por CAN | Solo escucha, 250 kbit/s (`links.cpp`, núcleo `n2k`); Ajustes > NMEA 2000. Sin probar en la placa |
| WiFi (NMEA por TCP / UDP, gpsd, Signal K) | Hecho (`src/net.cpp`, `src/sk_link.cpp`); Ajustes > Red. Sin probar en la placa |
| Bluetooth (pasarela y tracker APRS LoRa+) | Hecho (`src/ble.cpp`, `docs/BLE_NMEA.md`). Sin probar con ningún equipo |
| Internet (OpenWeatherMap) | Hecho: datos de reserva si falta un sensor. Sin probar contra el servidor |

Dudas a comprobar con la placa delante:
- Tarjeta TF: `board::sdBegin` deja el CS activo por el expansor y da a la librería SD el GPIO 6 como CS de pega. Si la
  tarjeta no monta, probar otro pin libre con `-DSD_DUMMY_CS=n` o bajar la velocidad del SPI (20 MHz).
- Un receptor AIS con salida RS-232 o RS-422 (Comar SLR200G) necesita adaptador a RS485 / nivel correcto: no conectarlo directo.
- Pines libres para el anemómetro de pulsos y las sondas DS18B20: mirar el conector de expansión de la 7B.
- Con el CAN activo no hay USB nativo; la consola y la carga del firmware van por el puerto del CH343.

# Placa del instrumento de viento: Waveshare ESP32-S3-Touch-LCD-4 (V4.0)

`src/board_lcd4.cpp`. Pines del BSP de Waveshare (`waveshareteam/Waveshare-ESP32-components`, `bsp/esp32_s3_touch_lcd_4`)
y del modelo `WAVESHARE-4-480X480` de ESPHome, que coinciden. **Sin probar en la placa.**

| Función | Pines |
|---|---|
| Panel ST7701, RGB 480×480 | HSYNC 38, VSYNC 39, DE 40, PCLK 41; datos B 5 45 48 47 21, G 14 13 12 11 10 9, R 46 3 8 18 17 |
| Configuración del ST7701 | SPI de 3 hilos por software: CS 42, SCL 2, SDA 1 (solo al arrancar) |
| I2C | SDA 15, SCL 7: expansor, táctil y reloj |
| Expansor CH32V003 | 0x24 (el de la 7B): EXIO1 reset del táctil, EXIO2 INT del táctil, EXIO3 reset del LCD, EXIO5 SYS_EN, EXIO6 zumbador, EXIO7 INT del reloj; registro 0x05 PWM de la luz (al revés) |
| Táctil GT911 | 0x5D o 0x14 (el INT va por el expansor: se busca) |
| CAN (TJA1051) | TX 6, RX 0; no comparte con el USB |
| RS485 | GPIO 43 / 44; la tabla de Waveshare da 43 = RX, otra fuente al revés: Ajustes los intercambia |
| Consola | USB nativo (CDC), GPIO 19 / 20 |

Sin verificar: tiempos del panel (los de `ST7701_480_480_PANEL_60HZ_RGB_TIMING` de esp_lcd_st7701, de memoria: pulso
10, porches 10 / 20 horizontales y 10 / 10 verticales, 16 MHz), qué GPIO es la recepción del RS485, y si el PWM de la
luz tiene el mismo límite que en la 7B. La tarjeta TF (SDMMC en GPIO 1, 2, 4) no se usa. Revisiones anteriores a la V4.0:
expansor TCA9554 en 0x20, no soportadas.
