# Enlace BLE de datos NMEA (pasarela ↔ plotter)

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
| Órdenes | `4c6f5261-2b00-4ea7-b100-000000000002` | Escritura sin respuesta, plotter → pasarela. Reservado (datos de ruta para el piloto); el plotter todavía no escribe |
| Datos | `4c6f5261-2b00-4ea7-b100-000000000003` | Notificación, pasarela → plotter |

Mismo esquema que el servicio de la caja del rotor (`…b000…`: órdenes en `0002`, eventos en `0003`).

## Formato

- Texto: sentencias NMEA 0183 completas, cada una acabada en `\r\n`, con su suma de comprobación (`$WIMWV,023.0,R,18.6,N,A*hh`).
- Una notificación puede llevar varias sentencias o un trozo de una: el plotter las junta por líneas. Conviene pedir un
  MTU de 247 y no partir sentencias si caben.
- Lo que el plotter entiende hoy: RMC, GLL, GGA, VTG, DPT, DBT, MWV (relativo), HDT, HDG, HDM, VHW, MTW, XDR, RSA, GSV,
  GSA y AIS (VDM / VDO).
- Ritmo: el NASA manda el viento 10 veces por segundo; por BLE basta con 2 a 5 por segundo.

## Prioridad y caducidad

Los datos por BLE entran en la tabla del plotter como fuente «BLE»: por debajo de sensores, CAN, puerto serie y red
local, y por encima de internet. Caducan a los 30 s sin llegar.

## Seguridad

Sin emparejamiento por ahora: son datos de solo lectura. Si se usa la característica de órdenes para el piloto, habrá que
exigir enlace cifrado y emparejado, como en el tracker.

## Compatibilidad

El plotter acepta también un equipo que anuncie el servicio UART de Nordic (`6e400001-b5a3-f393-e0a9-e50e24dcca9e`) y
notifique NMEA por `6e400003…` (o por `6e400002…`, como hace el servicio TNC2 del tracker).

## Elegir el equipo

Ajustes > Bluetooth: al encenderlo, el plotter se conecta al primer equipo que anuncie el servicio y recuerda su
dirección; desde entonces solo se conecta a ese. Apagar y encender olvida el equipo.
