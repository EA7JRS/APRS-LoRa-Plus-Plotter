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
| NMEA 2000 por CAN | Pendiente. `board::setCanMode(true)` conmuta los pines al transceptor |
| WiFi (TCP, UDP, Signal K, gpsd) | Pendiente |

Dudas a comprobar con la placa delante:
- Un receptor AIS con salida RS-232 o RS-422 (Comar SLR200G) necesita adaptador a RS485 / nivel correcto: no conectarlo directo.
- Pines libres para el anemómetro de pulsos y las sondas DS18B20: mirar el conector de expansión de la 7B.
- Con el CAN activo no hay USB nativo; la consola y la carga del firmware van por el puerto del CH343.
