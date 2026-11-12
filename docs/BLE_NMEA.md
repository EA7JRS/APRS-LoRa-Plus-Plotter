# Enlace BLE de datos NMEA (pasarela o instrumento de viento ↔ plotter)

Para llevar al plotter los datos de la pasarela (XIAO) sin cable: viento del NASA Target 2 Wind, y lo que la pasarela lea
del NMEA 2000 o del NMEA 0183. Borrador del 2026-11-11: los UUID y el formato se pueden cambiar mientras la pasarela no
esté adaptada.

## Papeles

- **Pasarela**: periférico. Anuncia el servicio y notifica los datos.
- **Plotter**: central. Busca el servicio, se conecta y se suscribe. Si se corta, vuelve a buscar solo.

## Servicio

| | UUID | Uso |
|---|---|---|
| Servicio | `4c6f5261-2b00-4ea7-b100-000000000001` | Debe ir en el anuncio (el plotter filtra por él) |
| Órdenes | `4c6f5261-2b00-4ea7-b100-000000000002` | Escritura (sin respuesta si el equipo lo permite), plotter → equipo. NMEA 0183 en texto: hoy, rumbo y velocidad del barco para el instrumento de viento |
| Datos | `4c6f5261-2b00-4ea7-b100-000000000003` | Notificación, pasarela → plotter |

Mismo esquema que el servicio de la caja del rotor (`…b000…`: órdenes en `0002`, eventos en `0003`).

## Formato

- Texto: sentencias NMEA 0183 completas, cada una acabada en `\r\n`, con su suma de comprobación (`$WIMWV,023.0,R,18.6,N,A*hh`).
- Una notificación puede llevar varias sentencias o un trozo de una: el plotter las junta por líneas. Conviene pedir un
  MTU de 247 y no partir sentencias si caben.
- Lo que el plotter entiende hoy: RMC, GLL, GGA, VTG, DPT, DBT, MWV (relativo), HDT, HDG, HDM, VHW, MTW, XDR, RSA, GSV,
  GSA y AIS (VDM / VDO).
- Ritmo: el NASA manda el viento 10 veces por segundo; por BLE basta con 2 a 5 por segundo.

## Del plotter al equipo

Si el equipo tiene la característica de órdenes (`…0002`), el plotter le escribe una vez por segundo lo que tenga de:

- `$IIHDT,ddd.d,T` rumbo verdadero;
- `$IIVTG,ccc.c,T,,M,s.s,N,k.k,K,A` rumbo y velocidad sobre el fondo;
- `$IIVHW,ddd.d,T,,M,s.s,N,k.k,K` velocidad en el agua (con el rumbo si lo hay);
- `$IIZDA,hhmmss.00,dd,mm,aaaa,00,00` hora UTC, solo si la del plotter viene del GPS o de NTP (el instrumento pone su reloj).

Cada tanda va entera, con `\r\n` tras cada sentencia, en trozos del MTU. El instrumento de viento APRS LoRa+ Wind la
usa para el viento real; la pasarela puede ignorarla. Los datos de ruta para el piloto irían por aquí más adelante.

## Prioridad y caducidad

Los datos por BLE entran en la tabla del plotter como fuente «BLE»: por debajo de sensores, CAN, puerto serie y red
local, y por encima de internet. Caducan a los 30 s sin llegar.

## Seguridad

Sin emparejamiento por ahora: son datos de solo lectura. Si se usa la característica de órdenes para el piloto, habrá que
exigir enlace cifrado y emparejado, como en el tracker.

## Compatibilidad

El plotter acepta también un equipo que anuncie el servicio UART de Nordic (`6e400001-b5a3-f393-e0a9-e50e24dcca9e`) y
notifique NMEA por `6e400003…` (o por `6e400002…`, como hace el servicio TNC2 del tracker).

## Tracker APRS LoRa+

El plotter también se conecta a un tracker APRS LoRa+ sin tocar su firmware (comprobado en el código del tracker,
`src/ble_control.cpp` y `docs/BLE_CONTROL.md`, el 2026-11-11):

- El tracker anuncia su servicio KISS (`00000001-ba2a-46c9-ae49-01b0961f68bb`) o el UART de Nordic en modo TNC2; el NMEA
  sale por la característica `4c6f5261-2b00-4ea7-a000-000000000004` de su servicio de control, en texto, tal cual.
- El servicio de control exige enlace cifrado y emparejado con el **PIN de control** (WebUI del tracker > Bluetooth >
  Control PIN, `123456` de fábrica). En el plotter: Ajustes > Red > «PIN del tracker (Bluetooth)».
- Solo las placas del tracker con salida NMEA (`NMEA_OUTPUT`, hoy la T-Beam Supreme) mandan posición: RMC, GGA, VTG, GLL y
  ZDA, además de MDA / XDR (meteorología) y `!AIVDM` (las estaciones APRS oídas, como blancos AIS, y el receptor AIS).
- Con el firmware publicado, el tracker lleva una sola conexión BLE a la vez: **o el plotter o la app del móvil**. La
  rama `ble-dos-enlaces` del firmware del tracker admite los dos a la vez (sin fusionar, pendiente de probar). Mientras
  tanto: el móvil conectado al tracker y el plotter tomando el NMEA de la app por la WiFi del móvil (puerto 10110).

## Elegir el equipo

Ajustes > Bluetooth: al encenderlo, el plotter se conecta al primer equipo que anuncie el servicio y recuerda su
dirección; desde entonces solo se conecta a ese. Apagar y encender olvida el equipo.
