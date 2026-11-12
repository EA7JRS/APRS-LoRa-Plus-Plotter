# APRS LoRa+ Plotter — contexto del proyecto

Propietario: Manuel (EA7JRS). Idioma de trabajo: español, respuestas breves y directas.
Los archivos nuevos llevan la cabecera `Copyright (C) 2026 Manuel - EA7JRS` y licencia GPL-3.0-or-later.
No publicar nada ni crear repositorios sin que lo pida Manuel.

## Qué es
Plotter náutico para la Waveshare ESP32-S3-Touch-LCD-7B (1024×600), inspirado en las funciones del Garmin GPSMAP x3
(manual de usuario estudiado el 2026-11-11), con diseño propio. Repositorio aparte, `EA7JRS/APRS-LoRa-Plus-Plotter` (privado, creado por Manuel el 2026-11-11; primera subida el mismo día), para integrarlo después en
`EA7JRS/APRS-LoRa-Plus-Display`, del que reutiliza los drivers de la 7B (pantalla, táctil GT911, expansor de pines),
LVGL ~9.6, NimBLE-Arduino ^2.5.2 y ArduinoJson 7.4.3.
Maquetas (lienzo Design, cuatro páginas por fases): https://claude.ai/artifact/3rGbM86ZobibCQ2KU44DAD

## Decisiones (Manuel, 2026-11-11)
- Solo placa 7B (Manuel tiene la LCD-7 rev 1.2 de 800×480; no hay entorno para ella de momento).
- Datos: NMEA 2000 por el CAN de la placa (pasarela XIAO de `APRS-LoRa-Plus-NMEA2000`, transpondedores Garmin AIS 600 / 800),
  serie (receptor AIS Comar SLR200G a 38400; emisoras con LSD Icom IC-M423GE / IC-M330GE, Garmin, Standard Horizon GX),
  TCP en la red local (Signal K, gpsd, AIS-catcher, Navtex, SafetyNET), AISHub (servicio web, 1 consulta/min), BLE con el tracker
  APRS LoRa+ y datos de internet (OpenWeatherMap, IQAir, AEMET, Portus). Prioridad: sensores, CAN, serie, TCP, BLE, internet.
- En la 7B el USB nativo comparte pines con el CAN: sin AIS por USB.
- Mapas base: OpenSeaMap por defecto; IHM (WMS cartaENCp2..p5, solo visualización, «no válida para la navegación»), IGN, Esri con
  clave, relieve del fondo (fuente por confirmar: EMODnet; el MBAR24 del IHM es CC BY-NC).
- Capas: Portus (oleaje, viento, corrientes, nivel, temperatura, boyas), radar de lluvia de AEMET con su escala de colores,
  rayos, avisos y predicción marítima de AEMET. Windy y Windfinder descartados.
- Emergencias: la misma ventana que la app Android (`AlertOverlay.kt`, `AlertCenter.kt`), más «DSC DISTRESS» (falta en la app).
- Viento: anemómetro de cazoletas con un imán (un pulso por vuelta, radio ~4 cm): factor inicial 1,2 kn por Hz, sin calibrar,
  rebote 5 ms. Sin veleta: la dirección sale de internet y los ángulos se marcan como estimados; el modo Viento del piloto
  queda bloqueado. Beaufort junto a la velocidad.
- **Viento, corregido por Manuel (2026-11-11)**: la veleta y el anemómetro son los del **NASA Marine Target 2 Wind (versión 2,
  con NMEA)**. Según las fichas de producto (seamarknunn.com, waterworldsports.co.uk), la unidad de tope de palo de 3 hilos
  manda NMEA 0183 a 4800 baudios, sentencia MWV (viento aparente), unas 10 veces por segundo, 0 a 99,9 kn; no vale para los
  equipos analógicos antiguos de 5 hilos. Consecuencias: hay dirección de viento medida (ya no se estima con internet ni se
  bloquea el modo Viento del piloto por eso), el anemómetro de pulsos propio deja de hacer falta (el núcleo `wind` lo
  conserva) y el lector NMEA ya lee MWV relativo. **Pendiente**: cómo entra en la 7B. El único puerto serie (RS485) está a
  38400 para el AIS Comar y el viento va a 4800: hace falta un segundo puerto serie (pines libres por confirmar), un
  multiplexor NMEA, o pasarlo por la pasarela XIAO a NMEA 2000 (PGN 130306, ya leído). Sin confirmar: de qué punto se toma
  la señal (de la unidad del palo o de la pantalla Target) y su nivel eléctrico.
  **Decidido por Manuel (2026-11-11)**: entra por la **pasarela XIAO de NMEA 2000**, que se puede reconfigurar para
  enlazarse con el plotter por BLE o por la WiFi local (además del CAN). Lo que el plotter ya admite sin cambios: CAN (PGN
  130306), NMEA 0183 por UDP al puerto de Ajustes > Red, y NMEA por TCP si la XIAO hace de servidor (el plotter solo tiene
  un cliente TCP: si se usa para gpsd o Signal K, la XIAO debe usar UDP). **BLE con la XIAO: sin hacer en el plotter** (no
  hay BLE todavía; servicio y formato por definir junto con el firmware de la XIAO, repositorio `APRS-LoRa-Plus-NMEA2000`).
- Unidades: náutico (profundidad en metros), métrico, imperial y usuario.
- Piloto automático: solo se da por viable enviar datos de ruta; los mandos dependen de la marca. **Es un Raymarine ST2000+
  de caña (Manuel, 2026-11-12)**. Del manual 81130-9 (2023): entra NMEA 0183 por el pin 6 (blanco, NMEA+) y el 5 (verde,
  NMEA−) de su conector (1 +12 V marrón, 2 0 V azul, 4 SeaTalk amarillo); lee XTE de APB / APA / RMB / XTE / XTR, demora y
  distancia de RMB / APB / BWC / BWR..., velocidad en el agua de VHW y **viento aparente solo de VWR** (no MWV) para WindTrim;
  **solo lee 4 letras del nombre del waypoint** (más largos: no avanza). 4800 baudios (foro de Raymarine; el manual no lo
  dice). Mínimo para Track según Raymarine: BTW, DTW, WPID y XTE (RMB basta). Mandos a distancia: SeaTalk1 (Z101, ST600R),
  sin hacer: haría falta un interfaz SeaTalk (bus de 12 V, 9 bits, 4800) en la pasarela XIAO o en el plotter.
- Declinación: Modelo Magnético Mundial calculado a bordo, con descarga del archivo de coeficientes cuando caduque.
- Fase 4: mareas (constantes por puerto; mirar el WMS de mareas del IHM) y rectas de altura (Meeus, contrastar con el Almanaque).
- Librerías: versiones fijadas y una tarea semanal que proponga actualizaciones tras compilar.

- Tomado de OpenCPN (estudiado el 2026-11-11; ideas y formatos, no código): GPX para waypoints, rutas y tracks; línea de costa
  mundial ligera como fondo de reserva; reproducción de archivos NMEA (simulador y repaso de travesías); datos de mareas
  (`data/tcdata` de OpenCPN: `ticon-europe-global.tcd`, 1690 estaciones; confirmar puertos españoles y licencia); filtros y
  prioridad por conexión con monitor de tramas; blanco AIS perdido, ocultar amarrados y silenciar por buque. GRIB, más adelante.
- Datos de la embarcación (nombre, MMSI, distintivo, tipo, dimensiones, antena GPS, sonda, depósitos): solo para el plotter.
- AIS en la carta con iconos propios por tipo de buque (no los de MarineTraffic). AISHub: zona visible, 1 consulta/min, atenuado
  y sin alarma de colisión. Alarma de garreo con pantalla de fondeo. Tablilla de desvíos (automática con compás electrónico;
  pendiente saber si Manuel tiene uno).

- AIS Dispatcher de AISHub (estudiado el 2026-11-11): va en la Raspberry para ser estación y obtener la clave de la API (el Comar
  saca a la vez NMEA para la pantalla y RS-232 para la Raspberry). El plotter añade entrada por UDP en la fase 1; reenviar él mismo
  a AISHub queda como opción secundaria (una estación debe estar siempre encendida).

- AIS-catcher (jvde-github, GPL-3.0; estudiado el 2026-11-11). Fase 1, hecho en el núcleo: comprobación de posiciones (saltos
  imposibles), contadores de calidad, mensajes 4, 9, 12, 14, 21 y 27 además de los de buques, dimensiones / calado / destino, y
  bloques de etiquetas NMEA (hora y estación). Fase 3: entrada JSON de AIS-catcher (nivel de señal por barco) y su API
  `/api/v1/vessels` para cargar la lista al arrancar. Su tabla de PGN AIS (129038, 129039, 129040, 129041, 129802, 129809, 129810)
  es la referencia para NMEA 2000. **Fuera** (Manuel): demodulación SDR, visor web, base de datos y catálogo de puertos.

- OpenPlotter (estudiado el 2026-11-11; Raspberry + Signal K, apps en Python, no se porta código). Aceptado por Manuel: entradas
  de pulsos genéricas (corredera de paletas, cuentavueltas), sondas DS18B20, entradas digitales con alarma (sentina, puerta),
  niveles de aviso de Signal K (nominal / normal / alert / warn / alarm / emergency) y más sensores I2C (BME280 / BMP280 / BME680,
  INA260, BH1750 para el brillo). Núcleo hecho en `inputs`; los drivers I2C y 1-Wire van con el firmware. **Fuera**: gestión de
  programas de la Raspberry y recepción SDR.
- Pypilot (pypilot/pypilot, estudiado el 2026-11-11): piloto abierto en Raspberry con IMU y controlador de motor. Servidor NMEA en
  TCP 20220: manda HDM (rumbo magnético), XDR PTCH / ROLL, RSA (timón) y MWV; acepta APB para seguir la ruta (toma el rumbo a
  gobernar y el error de rumbo, que limita a 0,15 mn). Puerto de control propio en 23322 (JSON; modos, rumbo, embragar), sin
  estudiar a fondo. El núcleo ya lee HDM / HDG, XDR y RSA y genera APB (`nmeaApb`). Sirve como compás para la tablilla de desvíos.

- Signal K (SignalK/specification, signalk-server, SensESP; estudiado el 2026-11-11; Manuel: «añade al plan lo que sí encaja»).
  Fase 1: entrada nativa por TCP 8375 (deltas JSON por línea, unidades SI; suscripción a `vessels.self`), descubrimiento mDNS
  (`_signalk-tcp`, `_signalk-ws`; NMEA del servidor en 10110) y avisos `notifications.*` en la ventana de alertas. Fase 3: rutas y
  waypoints compartidos (API de recursos), piloto por la API de piloto del servidor, plotter como sensor (publica anemómetro,
  sondas, sentina). Cuentavueltas con el contador PCNT del ESP32 (idea de SensESP). Núcleo hecho en `signalk` (rutas, unidades,
  suscripción, títulos de avisos, deltas de salida); el JSON lo lee el firmware con ArduinoJson. Las API REST no se han leído
  a fondo. **Fuera**: el servidor, sus webapps y complementos. Todo depende de tener una Raspberry con Signal K a bordo.

- AvNav (wellenvogel/avnav, estudiado el 2026-11-11; Manuel: «solo lo rentable»). Hecho en el núcleo (`route`): paso al siguiente
  waypoint (pronto / por el través / tardío), ruta loxodrómica u ortodrómica, registro de la derrota (10 s y 25 m), registro de
  sonda (CSV) y sonido de los avisos por nivel. Para después de que la carta funcione con teselas sueltas: cartas en un solo
  archivo (GEMF; formato sin comprobar) si la microSD se queda corta. **Aparcado**: páginas configurables y control remoto desde
  la app. **Fuera**: su servidor, el visor web y su app Android.

- gpsd (ntpsec/gpsd, BSD-2-Clause; estudiado el 2026-11-11). Sus grabaciones de prueba sirven de referencia: el decodificador
  AIS del núcleo da lo mismo que gpsd en 1645 mensajes reales de 4 grabaciones (`tools/ais_compare/run.sh <gpsd/test/daemon>`;
  las únicas diferencias son partes sueltas del mensaje 24, que gpsd no saca hasta tener A y B). De ahí salió un cambio: el
  texto AIS acaba en la primera «@», como en gpsd. Para la fuente gpsd (TCP 2947): pedir `?WATCH={"enable":true,"nmea":true}` y
  reutilizar el lector NMEA; el JSON `SKY` (satélites) para la pantalla GPS queda para después. `drivers/driver_nmea2000.c` es
  referencia de PGN (127245 timón, 127250 rumbo, 127508 batería, 128259 corredera, 128267 sonda, 128275 distancia, 129025 /
  129026 / 129029 posición, 129284 navegación, 129539 / 129540 satélites). Hecho (Manuel: «incluye lo que encaja»): cielo GNSS desde GSV / GSA
  (`gnss`, vale para cualquier fuente NMEA, también gpsd), GGA (altitud, HDOP, satélites), orden `?WATCH` de gpsd, y el lector
  NMEA pasado por sus 201 grabaciones de receptores reales con ASan / UBSan sin fallos (`tools/nmea_soak`). Posible más adelante: meteorología por AIS (mensaje 8).

- Avisos meteorológicos (pedido 2026-11-11: «como los recibidos de internet, Google»; fase 3): avisos de AEMET por zonas
  (OpenData, formato CAP; niveles amarillo / naranja / rojo). Pantalla Meteo > Avisos (lista con filtro «mi zona y ruta», zonas
  en un mapa, vigencia, texto tal cual) y ventana de alerta como la de emergencias cuando entra uno nuevo para la zona del barco
  o de la ruta activa. Maquetas `AvisosMeteo` y `AlertaMeteo`. Sin programar. Decidido por Manuel (2026-11-11): el nivel desde el que suena
  el zumbador es seleccionable (amarillo / naranja / rojo / nunca); España con AEMET y otros países con Meteoalarm (canales CAP
  por país en `feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-<pais>`, listados en el registro de la OMM
  `alertingauthority.wmo.int`; condiciones de uso sin leer; EE. UU. tiene `api.weather.gov/alerts`). País según la posición.

- NAVGUIDE de la IALA (edición 5, 2006, en español, de puertos.es; estudiado el 2026-11-11; es antigua: contrastar con la
  vigente). Hecho en el núcleo (`lights`): alcance geográfico 2,03·(√ojo + √luz), alcance luminoso con la visibilidad real (ley
  de Allard, nominal a 10 mn de visibilidad), ritmos de luces (abreviaturas internacionales y españolas, animación), marcas IALA
  desde el tipo de ayuda del mensaje AIS 21 y «¿qué luz veo?», radio de borneo √(L² − H²) y relación cadena / fondo para la
  alarma de garreo. Maqueta `Balizamiento` (ayuda con las marcas de la región A, «¿qué luz veo?» y «¿cuándo veré el faro?»).
  **Fuera**: diseño y mantenimiento de ayudas, VTS, practicaje.

- Radioavisos náuticos del IHM (enlaces de Manuel, 2026-11-11; fase 3). Fuente pública sin cuenta:
  `https://armada.defensa.gob.es/ihm/XML/navareas_crudo.xml` (exportación de Access: `<dataroot generated="...">` con un
  `<NAVAREASVIGOR>` por aviso: `nnuna`, `nnunaf` «0086/26», `nfemi`, `nlocae` / `nlocai` zona, `ntees` / `ntein` texto,
  `nasun` / `nasuni` título; 14 avisos el 2026-11-10). Las posiciones van dentro del texto («46-04.0N 033-12.8E»,
  «42 11,718N 009 59,778E»). Núcleo hecho en `navwarn` (lector del XML, posiciones del texto —vale también para Navtex—,
  distancia al aviso), probado con avisos inventados en ese formato: falta pasarle el fichero real (desde la sesión solo se
  pudo leer resumido). Son los NAVAREA III (Mediterráneo desde 005°45′W y mar Negro): para el golfo de Cádiz hacen falta los
  costeros / NAVAREA II, sin fuente localizada. `navarea3.com`: el panel con mapa pide usuario, pero hay páginas públicas:
  `/InformacionNavarea/` (tabla de avisos en vigor con **fecha de cancelación**, zona y título, y por aviso PDF
  `InformacionNavarea.aspx?Id=...&Tipo=...`, S-124, GIS y KML; filtro por localización, incluye Estrecho de Gibraltar y mar de
  Alborán) y `/Publicos/NavareasInternacionales` (directorio de las 21 NAVAREA con su enlace de avisos en vigor; NAVAREA II =
  SHOM, `portail.ping-info-nautique.fr`, sin poder leerlo desde la sesión). Pendiente: ver las direcciones exactas de los KML /
  S-124 (traen la geometría hecha) y si se permiten descargas automáticas. Posible fuente de Portugal: ANAVnet del Instituto Hidrográfico (sin mirar).

- IDE del IHM (`ideihm.covam.es`, visualizador estudiado el 2026-11-11; es una app JavaScript, desde la sesión no se ven sus
  capas). Servicios confirmados por otras fuentes: WMS de cartas ENC (`/wms/cartaENCp2..p5`, ya en el plan), WMS de estaciones
  de mareas `/wms/mareas`, superficie de referencia vertical `/wms/srvh` y `/wcs/srvh`, límites marítimos
  `/ihm-inspire/wms-unidadesmaritimas` (CC-BY 4.0), catálogo `/catalogo`. El visualizador tiene un complemento de **faros y
  señales de niebla**: si esos datos (característica, alcance, altura) se pueden pedir por WFS o GetFeatureInfo, alimentan
  `lights` (animar luces, «¿cuándo veré el faro?»). Sin comprobar; pendiente de que Manuel mire las capas en el visor.
  Añadido (Manuel: «añade lo que encaja»): lista de luces propia en la microSD (`nombre;lat;lon;característica;altura;alcance`,
  `parseLightRecord`, `lightSightingRangeNm`), independiente de dónde salgan los datos, y maqueta `CartaFaro` (ficha de faro
  con el ritmo dibujado y «se verá a», y capa «Límites» con el mar territorial). Los límites marítimos van como capa WMS (fase 3).

- API-CNIG del IGN (`plataforma.idee.es/cnig-api`, repositorio IGN-CNIG/API-CNIG, EUPL 1.2; estudiado el 2026-11-11): es una
  librería JavaScript sobre OpenLayers para visores web: **no encaja** en el ESP32. Lo aprovechable son los servicios que usa,
  con teselas ya hechas (numeración TMS, `{-y}`): `https://tms-ign-base.idee.es/1.0.0/IGNBaseTodo/{z}/{x}/{-y}.jpeg` (mapa
  base del IGN) y `https://tms-pnoa-ma.idee.es/1.0.0/pnoa-ma/{z}/{x}/{-y}.jpeg` (ortofoto PNOA); también WMTS `ign-base`,
  `pnoa-ma`, `mapa-raster` y el geocodificador inverso de CartoCiudad. Añadidas como bases «IGN» y «Ortofoto» (fase 3; la app
  Android ya usa PNOA y MTN, CC BY 4.0). Núcleo: `tiles` (posición ↔ tesela y píxel, metros por píxel, zoom para una anchura,
  direcciones con `{z}{x}{y}{-y}`, rutas en la microSD). Sin comprobar: condiciones de uso exactas y zoom máximo de cada servicio.
  De sus complementos (Manuel: «sí», 2026-11-11), a la fase 1: **medir** en la carta (tramos encadenados, demora e inversa,
  tiempo a la velocidad actual, pasar a ruta), **datos del punto tocado** (posición, distancia y demora desde el barco) e
  historial de vistas; núcleo en `chartui`, maqueta `CartaMedir`. Fuera: comparador de capas; perfil de fondo, más adelante.

- API de mareas del IHM (`https://ideihm.covam.es/portal/api-mareas/`, enlace de Manuel, 2026-11-11): oficial, «predicciones
  de marea por puerto y fecha», página modificada el 2026-02-27. Desde la sesión no se pudo leer la documentación (la página
  se rellena con JavaScript y la versión inglesa dio error). De memoria, **sin verificar**: `https://ideihm.covam.es/api-ihm/getmarea`
  con `request=getlist&format=json` (puertos) y `request=gettide&id=<puerto>&format=json&date=AAAAMMDD` (o `month=AAAAMM`).
  Plan para la fase 4: esta API como fuente principal de mareas en España (con internet, guardando en la microSD los días
  siguientes para usarlos sin cobertura) y el cálculo armónico propio como reserva y para otros países. Pendiente: que Manuel
  pegue el texto de la página o una respuesta de ejemplo antes de escribir el lector.

- WMS del IHM `https://ideihm.covam.es/wms/CartaOF` y `.../wms/mareas` (enlaces de Manuel, 2026-11-11): son servidores
  MapServer; sin parámetros dan error y la herramienta de lectura de la sesión pierde los parámetros, así que **no se han
  podido ver las capas** (hace falta el `GetCapabilities`, que puede abrir Manuel). `CartaOF` parece la carta oficial de papel
  en ráster: candidata a mapa base «Carta IHM» junto a la ENC. El núcleo ya pide a cualquier WMS la imagen de cada tesela
  (`tileBounds3857`, `wmsTileUrl`, EPSG:3857, 256 px): vale para las cartas del IHM, los límites marítimos y Portus.
  `https://ideihm.covam.es/wms/lucesIHM` (enlace de Manuel): WMS de **luces** del IHM, la fuente que faltaba para los faros;
  tampoco se han podido ver sus capas ni el formato de la ficha. El núcleo ya arma la consulta del punto tocado
  (`wmsFeatureInfoUrl`, GetFeatureInfo); falta el nombre de capa y un ejemplo de respuesta para leerla y rellenar `LightRecord`.
  `https://ideihm.covam.es/wms/catalogoENC` (enlace de Manuel): por el nombre, el catálogo de celdas ENC (qué carta
  electrónica cubre cada zona y a qué escala); tampoco leído. Poco útil en el plotter: como mucho, para saber hasta qué zoom
  hay carta en un sitio. `https://ideihm.covam.es/wms/enc` (enlace de Manuel): la carta
  electrónica en una sola dirección (antes teníamos `cartaENCp2..p5`, una por escala); tampoco leído.
  **Los cinco WMS del IHM (enc, CartaOF, lucesIHM, mareas, catalogoENC) esperan lo mismo: el GetCapabilities con los nombres
  de capa, que tiene que abrir Manuel.** Maqueta `CartaBase` actualizada: OpenSeaMap, IHM electrónica, IHM de papel, IGN, ortofoto PNOA y relieve.

- Catálogo de datos abiertos del MITECO (`catalogo.datosabiertos.miteco.gob.es`, enlace de Manuel, 2026-11-11): no deja
  leerse de forma automática (robots). Por otras páginas del ministerio: espacios marinos protegidos (SHP / KMZ), red OSPAR
  (SHP / GeoJSON), ZEPIM por WMS (`https://wms.mapama.gob.es/sig/Biodiversidad/ZEPIM`), ordenación del espacio marítimo
  (POEM, WMS) y praderas de fanerógamas marinas. Posible capa «Zonas protegidas» de la carta (fase 3) y aviso al fondear
  sobre pradera o reserva; propuesto a Manuel, sin decidir. Sin comprobar: direcciones de las capas de posidonia y licencia.

- Catálogo de la IDEE (`www.idee.es/csw-inspire-idee`, enlace de Manuel, 2026-11-11): buscador nacional de datos y servicios
  geográficos (GeoNetwork, CSW); su interfaz es JavaScript, pero las fichas `.../srv/api/records/<id>` sí se leen. Fichas del
  IHM encontradas, todas CC-BY 4.0 y «sin limitaciones al acceso público»: límites marítimos (`spaihmwms_unidadesmaritimas`,
  y los datos en GML `spaihmMU`), **línea de costa** por WMS (`spaihmwms_hidrografia_costa`,
  `http://ideihm.covam.es/ihm-inspire/wms-hidrografia-p`, pleamar y bajamar, «no válido para la navegación») y por **WFS**
  (`spaihmwfs_regionesmarinas_costa`). La costa en vectores del IHM es candidata a la costa de reserva de España en la
  microSD (la carta sin teselas), mejor que una costa mundial genérica; pendiente de ver tamaño y formato. También está la ficha
  de zonas de servicio portuarias de Puertos del Estado (`spa_puertos_estado`). El IHM tiene una herramienta «Faros y Señales
  de Niebla» con los datos del libro de faros (misma fuente que `lucesIHM`).

- Faros y luces: de dónde salen los datos (Manuel, 2026-11-11: «los faros y luces»). Tres vías, por orden:
  1. **OpenStreetMap / OpenSeaMap** (ODbL, hecho en el núcleo): las luces llevan etiquetas `seamark:light:character`, `group`,
     `period`, `colour`, `height`, `range`. `overpassLightsUrl` pide las de un recuadro a la API Overpass (JSON),
     `lightRecordFromSeamark` las convierte y `formatLightRecord` las guarda en la lista de la microSD. Cobertura mundial y
     automática; la calidad depende de lo que haya cartografiado. Sin probar contra el servidor real (bloqueado en la sesión).
  2. **WMS `lucesIHM`** del IHM (oficial): pendiente del nombre de capa y de un ejemplo de ficha.
  3. **Libro de Faros y Señales de Niebla** del IHM (columnas: número, nombre, posición, característica y periodo, altura en
     metros, alcance en millas, estructura, observaciones; PDF públicos de 2017 en publicaciones.defensa.gob.es; aplicación web
     `armada.mde.es/ihm/Aplicaciones/LibroFaros/V3/`, sin poder leerla). Es © IHM: se puede usar para una lista personal, pero
     **no se mete en el repositorio**.

- Observaciones de la estación de AEMET más cercana (Manuel: «sí», 2026-11-11; fase 3). El catálogo de datos abiertos del
  MITECO no aporta meteorología propia: la de España es AEMET OpenData (misma clave que el radar y los avisos): avisos,
  predicción marítima (XML, diaria), radar y **observación horaria de estaciones** (últimas 12 h, todas o una por `idema`).
  Núcleo `wxstation`: estación más cercana con viento (30 mn por defecto), datos a la tabla como fuente Internet y calidad
  «estimado» (viento, dirección, racha, presión, temperatura, humedad, visibilidad; nada con más de 90 min; un dato medido a
  bordo manda), texto «Estación de tierra a 4,2 mn al NE · hace 25 min». Da dirección de viento real al anemómetro sin veleta.
  Los nombres de campo del JSON de AEMET están puestos de memoria: comprobar con una respuesta real. Maqueta `Meteo` actualizada.

- Dos conjuntos de AEMET en el catálogo del MITECO (enlaces de Manuel, 2026-11-11; el catálogo no deja leerse, identificados
  por búsqueda):
  - `471b4014-…` **Datos de radares**: «Observación Radar» (API con descarga de datos georreferenciados), imagen del radar
    regional e imagen de la composición nacional. Los datos georreferenciados resolverían el encaje del radar sobre la carta y
    los colores (hasta ahora, imagen con escala aproximada). Formato y tamaño sin ver.
  - `46e25cfb-…` **Modelo HARMONIE-AROME**: 2,5 km de resolución, hasta 48 h; campos temperatura, presión, nubosidad, viento,
    precipitación, descargas eléctricas y racha máxima; descarga en GeoTIFF o GeoJSON:
    `https://www.aemet.es/es/api-eltiempo/modelos/download/harmonie/PB` (Península y Baleares) y `.../CAN` (Canarias).
    Candidato a la capa de **previsión de viento y racha** de la carta (lo que se descartó hacer con Windy). Duda: una malla de
    2,5 km de toda la Península es demasiado para que la baje y la lea el ESP32; habrá que recortar (en la app Android, que
    pasa la zona por BLE o WiFi, o pidiendo solo un recuadro si la API lo permite). Sin comprobar parámetros, tamaño ni licencia.
  **Aceptado por Manuel (2026-11-11)**: radar georreferenciado en lugar de la imagen aproximada, y capa «Previsión de viento»
  con control de tiempo de 48 h (viento, racha, lluvia, rayos, presión, temperatura, nubes) y rayos como aviso. Maqueta
  `CartaPrevision`. Fase 3, sin programar.

## Estado (2026-11-12)
- `lib/plotter_core`: geo, motion, wind, units, astro, store, nmea, ais, gpx, stream (líneas y tramas repetidas), inputs (pulsos, interruptores, DS18B20, niveles de aviso), signalk, route (seguimiento de ruta, derrota, sonda), gnss (satélites), lights (luces, balizamiento, borneo), anchor (vigilancia de fondeo), n2k (NMEA 2000), navwarn (radioavisos), tiles (teselas de mapa), chartui (medir, formato de posición, vistas), wxstation (estación meteo cercana), sim (simulador NMEA y blancos AIS), aistargets (tabla de blancos AIS). 1466 comprobaciones en `test/host/test_core.cpp`
  (`tools/run_host_tests.sh`; 1517 desde el 2026-11-12 con `n2ktx` y `windalarm`), con valores de referencia publicados (Vincenty: Flinders Peak - Buninyong; AIS: ejemplos de gpsd).
- **Firmware, primer paso (2026-11-11)**: `platformio.ini` (misma base que Display: pioarduino `espressif32 @ 8.0.0`, LVGL
  ~9.6.0, ArduinoJson 7.4.3), placa 7B copiada de Display (`src/board.h`, `board_lcd7b.cpp`, `board_common.*`, `lv_defs.h`,
  fuentes DejaVu en `src/fonts`; los números grandes regenerados con coma y dos puntos: `tools/make_fonts.sh`) más
  `setCanMode` y `sdSelect`; `src/links.*` (simulador + RS485 a 38400 con el lector NMEA / AIS), `src/ui.*` (barra superior,
  8 pestañas, Instrumentos con 8 casillas, fuente y antigüedad de cada dato, viento real calculado), `src/main.cpp`
  (consola: `APRS-INFO?`, `SIM 0` / `SIM 1`). Simulador en el núcleo (`sim`). **Compila en la CI** desde el 2026-11-11 (`.github/workflows/tests.yml`, trabajo `firmware`; el `firmware.bin` queda
  como artefacto de cada ejecución); en la sesión no hay acceso al registro de PlatformIO. La fuente de datos del puerto serie
  se llama `Source::Uart` porque Arduino define `Serial` como macro. Sí comprobado: `ui.cpp`, `links.cpp` y `main.cpp` compilan contra LVGL 9.6.0 con `-Werror`, y la interfaz se
  ha dibujado en el PC con LVGL real (`tools/ui_preview/build.sh <lvgl> salida.ppm`). Sin probar en la placa. `docs/HARDWARE.md`.
- **Radar AIS (2026-11-11, rama `radar-ais`)**: `aistargets` en el núcleo (un blanco por MMSI, posiciones imposibles
  descartadas salvo que la siguiente las confirme, distancia, demora, CPA / TCPA, peligroso con CPA ≤ 0,5 mn y TCPA ≤ 20 min,
  perdido a los 6 min y fuera a los 12, silenciar por buque, orden «peligrosos primero», 100 blancos), codificador AIS para el
  simulador (`aisEncodePosition`, `aisEncodeName`), 5 blancos simulados con nombres y MMSI inventados, y la pestaña Radar AIS
  (`src/ui.cpp`: norte arriba, tres anillos, alcance 0,75 a 12 mn con − / +, vectores de 6 min, lista de 8 con el riesgo
  resaltado). Dibujada en el PC: `tools/ui_preview/build.sh <lvgl> salida.ppm 3 300`. Fusionado en `main` el 2026-11-11.
- **Alerta de abordaje, ficha de blanco y Carta básica (2026-11-11, rama `carta-alarmas`)**: ventana centrada sobre cualquier
  pantalla (`lv_layer_top`), como la de emergencias de la app: «COLLISION RISK» con SILENCE ALARM (silencia ese buque) y CLOSE
  (ese buque no la reabre en 10 min); se cierra sola si deja de haber riesgo. Tocar una fila del radar abre la ficha del
  blanco (tipo, MMSI, clase, distancia, CPA, velocidad, eslora, silenciar / activar alarma). Pestaña **Carta**, primer paso,
  **sin teselas**: proyección de `tiles`, barco con vector de 6 min, derrota (`TrackSampler`, 240 puntos que se van
  aclarando), blancos AIS con nombre y vector, escala, zoom 5 a 18 con + / −, arrastrar para mover y «Centrar barco».
  Dibujadas en el PC (`tools/ui_preview/build.sh <lvgl> salida.ppm <pestaña> <segundos> [cerrar]`). Fusionado en `main` el 2026-11-11.
- **Teselas de la tarjeta en la Carta (2026-11-11, rama `carta-teselas`)**: fondo con las teselas PNG de
  `/charts/osm/<z>/<x>/<y>.png` (`docs/CARTAS.md`): 24 imágenes de LVGL recolocadas al mover o cambiar el zoom, lista de
  teselas que faltan (se olvida cada 30 s), decodificador PNG (lodepng) y JPEG (tjpgd) de LVGL, caché de imágenes de 8 MB,
  tarjeta por el sistema de ficheros de LVGL (`S:`; Arduino SD en la placa, carpeta `/tmp/plotter_sd` en el PC). Sobre mapa
  claro las líneas pasan a oscuras y las etiquetas llevan fondo. `board::sdBegin` (CS por el expansor, GPIO 6 de pega para
  la librería SD: **sin verificar**). Comprobado en el PC con teselas de prueba (`tools/ui_preview/fake_tiles.py`): encajan
  entre sí y con el barco. Fusionado en `main` el 2026-11-11.
- **Navegar a un punto, waypoints, MOB y Ajustes (2026-11-11, rama `navegar`)**: tocar la carta elige un punto (o el waypoint
  que haya debajo) y abre un panel con posición, distancia, demora y tiempo, y botones «Ir a», «Marcar» y «Cerrar». «Ir a»
  usa `RouteFollower` (loxodrómica, llegada a 0,05 mn): línea al destino, panel con distancia, demora, tiempo y desvío,
  «Parar» y «Destino». Waypoints (50, «WPT 01»...) con el botón «Marcar» de la barra o desde el punto tocado; se guardan en
  GPX en la tarjeta (`S:/waypoints.gpx`, por el sistema de ficheros de LVGL) y se cargan al arrancar. **MOB**: marca la
  posición, pone rumbo de vuelta y abre la ventana «MAN OVERBOARD» con distancia, demora y tiempo; al cerrarla sigue la
  navegación al punto en la carta. Pestaña **Ajustes**: simulador, velocidad del RS485 (38400 / 4800), brillo y estado; se
  guardan en NVS (`src/settings.*`). Dibujado en el PC con acciones simuladas (`build.sh ... "cerrar,marcar,tocar:x:y,boton:n,
  mob,esperar:s"`). Falta: rutas de varios puntos y su pestaña, borrar y renombrar waypoints, medir, elegir mapa base,
  bajar teselas por WiFi, costa de reserva, alarma sonora (zumbador externo, pin por decidir), rumbo arriba, zona de guarda
  y estelas en el radar.
- **Rutas y medir (2026-11-11, rama `rutas`)**: pestaña **Rutas** (izquierda: waypoints con distancia y demora, páginas de 7,
  «Ir a», «Ver», «A la ruta», «Borrar»; derecha: la ruta, hasta 12 puntos, con tramo y demora de cada uno, «Navegar» / «Parar»,
  «Quitar último», «Vaciar»). La ruta se guarda con los waypoints en el GPX. Navegación de varios puntos (`RouteFollower`,
  cambio por el través, botón «Siguiente», «A P2 (2 de 4)», ruta dibujada en la carta). **Medir** en la carta (botón «Medir»
  del punto tocado: tramos encadenados, total, último tramo con demora, tiempo a la velocidad actual; «Deshacer», «A ruta»,
  «Salir»). Dibujado en el PC (acciones `wp:n`, `rutas:n`). Falta: renombrar waypoints, varias rutas con nombre, invertir ruta.
- **Fondeo y alarma de garreo (2026-11-11, rama `fondeo`)**: núcleo `anchor` (`AnchorWatch`: círculo alrededor del ancla,
  garreo tras 10 s seguidos fuera —un salto suelto del GPS no avisa—, sin posición a los 60 s, distancia máxima, radio
  recomendado = borneo con la cadena y la sonda + 10 m). Carta: botón «Fondeo» → panel (cadena ± 5 m, fondo, relación,
  «Fondear» en la posición del barco; fondeado: distancia y demora al ancla, radio ± 5 m, «Levar»), círculo verde / rojo,
  zoom 17 al fondear. Ventana «ANCHOR DRAG» / «ANCHOR WATCH · NO GPS FIX» (SILENCE 10 MIN, CLOSE = 2 min). El fondeo se guarda
  en NVS y sigue tras un reinicio. Acción `fondeo` en el dibujo del PC. Falta: marcar el ancla tocando la carta, sonido.
- **Unidades y mapa base (2026-11-11, rama `unidades`)**: Ajustes > *Unidades* (náutico / métrico / imperial, `units` del
  núcleo; se aplican a Instrumentos, carta, escala, rutas, radar y ventanas; el viento en métrico va en m/s; el fondeo sigue
  en metros) y Ajustes > *Mapa base* (carpetas de la tarjeta `/charts/osm` PNG, `/charts/ihm` PNG, `/charts/ign` y
  `/charts/pnoa` JPEG, cada una con su atribución en la carta). Se guardan en NVS. Acción `ajuste:n` en el dibujo del PC.
  Falta: sistema «usuario» (cada magnitud por separado) y probar teselas JPEG reales.
- **NMEA 2000 por el CAN (2026-11-11, rama `nmea2000`)**: núcleo `n2k` propio (sin la librería NMEA2000): identificador →
  PGN, montaje de mensajes de varias tramas y lectura de 127245 timón, 127250 rumbo, 127488 rpm, 127508 batería, 128259
  corredera, 128267 sonda, 129025 / 129026 / 129029 posición, 130306 viento, 130310 / 130311 / 130312 / 130314 temperatura,
  humedad y presión, y AIS 129038, 129039, 129794, 129809, 129810 (a `AisTargets`). Campos contrastados con la base de datos
  de canboat (Apache-2.0, clonado en la sesión) y probados con sus ejemplos y salidas esperadas. Firmware: controlador TWAI
  de ESP-IDF a 250 kbit/s en `links.cpp`; **solo escucha**: modo normal para dar el acuse de recibo, pero nunca transmite ni
  reclama dirección (no es un nodo certificable; si hiciera falta emitir —piloto, waypoints— habrá que reclamar dirección).
  Ajustes > *NMEA 2000* (encendido por defecto; quita el USB nativo de los pines 19 / 20), contadores en Estado, orden de
  consola `N2K <id hex> <bytes hex>` para meter una trama sin bus. Con el simulador encendido sus datos mandan sobre el CAN.
  Sin probar en la placa ni con una red real. Falta: 129040 / 129041 (AIS B ampliado y ayudas), 129540 satélites, 129284
  navegación, 127489 motor, 127505 depósitos, LSD (129808).
- **Red: WiFi, TCP y UDP (2026-11-11, rama `red`)**: `src/net.*` (tarea aparte en el núcleo 0: WiFi cliente con reintento
  cada 20 s, un servidor TCP —NMEA 0183 o gpsd con `?WATCH` en NMEA, reintento cada 10 s— y escucha UDP; los bytes pasan al
  bucle principal por `StreamBuffer`, así una red caída no para la pantalla) y `links.cpp` (líneas → lector NMEA / AIS como
  fuente `Tcp`). Ajustes > *Red: WiFi, TCP y UDP*: pantalla a toda la pantalla con teclado de LVGL (red, clave, tipo de
  servidor, dirección, puerto, puerto UDP), estado arriba; se guarda en NVS. Acción `red` en el dibujo del PC. Sin probar en
  la placa. Falta: lista de redes al alcance, Signal K nativo (JSON, TCP 8375), mDNS, bajar teselas.
- **Pestaña Viento (2026-11-11, rama `viento`, sobre `red`)**: esfera con la proa arriba (sectores de ceñida rojo / verde,
  aguja A del aparente y R del real, velocidad en el centro), casillas de viento aparente, real (con fuerza Beaufort y su
  nombre), dirección real («viene del O») y ángulo real (ceñida / través / largo / empopada, banda), e historial de 10 min
  (una muestra cada 10 s, mínimo y máximo). El real se calcula con el aparente, la velocidad y el rumbo; si no, se usa el
  que llegue ya calculado (NMEA 2000). La fuente real será el NASA Target 2 Wind (MWV a 4800; ver Decisiones); falta decidir por dónde entra.
- **Pestaña Piloto (2026-11-11, rama `piloto`)**: rumbo a gobernar (demora al destino) y cuánto caer, rumbo del barco, barra
  de desvío de la ruta (escala ± 0,1 / 0,5 / 2 mn), timón (si llega RSA o el PGN 127245), destino con distancia, tiempo y rumbo
  del tramo, y **salida de datos de ruta**: Apagado / RS485 / servidor TCP (`settings.pilotOut`), APB y RMB (`nmeaRmb`, nuevo
  en el núcleo) una vez por segundo con ruta activa, demoras verdaderas. Por TCP usa la misma conexión de Ajustes > Red
  (Pypilot: puerto 20220). Solo datos: no embraga ni manda el piloto. El RS485 es el mismo puerto del AIS (el transceptor
  conmuta solo): emitir por él con un receptor AIS conectado al mismo par puede chocar; sin probar. Falta: demoras
  magnéticas para pilotos que las pidan, XTE / BWC, y los mandos según la marca del piloto de Manuel.
  **ST2000+ (2026-11-12)**: `settings.pilotModel` (0 genérico, 1 ST2000+ de serie, NVS `pilotM`) y salida 3 = Bluetooth
  (`ble::write` a la pasarela). Con el ST2000+: RMB + APB con `pilotWaypointId` («W001»...), VWR (`nmeaVwr`) y VHW cada
  segundo aunque no haya ruta (WindTrim); aviso si sale por el RS485 sin estar a 4800. Acción `piloto:n` en el dibujo del PC.
- **Relevo por internet y pestaña Meteo (2026-11-11, rama `internet`; Manuel: «si falla cualquier sensor físico podrá
  alternar con los datos obtenidos de internet»)**: la tabla de datos ya lo hacía por prioridad (a los 10 s sin un sensor
  entra la fuente siguiente y vuelve sola al reaparecer); faltaba traer los datos. `net.cpp`: OpenWeatherMap
  `data/2.5/weather` (HTTPS sin comprobar certificado, clave del usuario en Ajustes > Red, en la posición del barco, cada
  15 min; a los 2 min si falla) → `links.cpp` los mete como fuente `Internet`, calidad «estimado», válidos 2 h: viento real
  (velocidad y dirección), racha, presión, temperatura del aire, humedad y visibilidad. Pestaña **Meteo**: seis casillas con
  fuente, antigüedad y «estimado», en ámbar lo que viene de internet, y nota con el estado de la consulta. Viento e
  Instrumentos usan el viento real de internet si falta el de a bordo. **Sin sustituto en internet**: posición, rumbo,
  velocidad, sonda, viento aparente (el AIS tendrá AISHub). Sin probar contra el servidor real (nombres de campo de memoria,
  los mismos que usa la app Android). Falta: AEMET (estación más cercana, `wxstation`), IQAir, Portus (temperatura del agua,
  oleaje), y elegir fuente.
- **Aviso de RS485 compartido (2026-11-11, pedido por Manuel)**: si la salida al piloto está en «Por el RS485» y por ese
  puerto han entrado datos en el último minuto, ventana emergente «RS485 COMPARTIDO» (una vez; vuelve a avisar si se apaga y
  se enciende la salida) con «Apagar salida» y «Seguir así». Las alertas de emergencia la sustituyen si saltan. Acción
  `rs485` en el dibujo del PC.
- **BLE con la pasarela (2026-11-11, rama `ble`; Manuel: «empieza por el BLE, la pasarela XIAO estoy pensando en una
  readaptación»)**: `src/ble.*` (NimBLE-Arduino 2.5.2, tarea aparte): el plotter es **central**, busca el servicio de datos
  NMEA `4c6f5261-2b00-4ea7-b100-000000000001` (datos por notificación en `…0003`, órdenes reservadas en `…0002`; mismo
  esquema que el rotor) o el UART de Nordic, se conecta, se suscribe y vuelve a buscar si se corta. Sentencias NMEA 0183 en
  texto → `links.cpp` (`bleLoop`, fuente `Ble`). Ajustes > botón *Bluetooth* (apagado por defecto; al conectar recuerda la
  dirección del equipo y solo vuelve a ese; apagar y encender lo olvida), contador en Estado. Protocolo en
  `docs/BLE_NMEA.md` (**borrador**: UUID y formato propuestos por Claude, a confirmar cuando Manuel readapte la XIAO). Sin
  emparejamiento (solo lectura). Compila en la CI; **sin probar con ningún equipo**: no existe todavía el otro extremo.
  Falta: lista de equipos para elegir, escribir órdenes (piloto por la pasarela), y el enlace BLE con el tracker APRS LoRa+.
- **Fuentes de GPS (Manuel, 2026-11-11)**: la posición puede venir del punto de acceso WiFi del teléfono, del tracker
  APRS LoRa+ (BLE / WiFi), de gpsd (TCP / UDP), de la salida NMEA de las emisoras VHF y del receptor AIS Comar SLR200G.
  Estado: **ya entran** el Comar y cualquier NMEA por el RS485 (RMC, GLL, GGA, GSV / GSA), gpsd por TCP, NMEA por UDP, NMEA
  2000 y la pasarela por BLE; **teléfono**: con el servidor de Ajustes > Red vacío, el plotter se conecta al equipo que da
  la WiFi (puerta de enlace) en el puerto 10110, que es donde la app Android sirve NMEA (`NmeaServer`; sin probar);
  **emisoras VHF**: van a 4800 y el RS485 está a 38400 para el AIS: por la pasarela XIAO, como el viento; **tracker
  directo: por BLE con su PIN** (ver «Tracker APRS LoRa+ como fuente»). Si llegan varias,
  gana la de más prioridad (CAN, puerto serie, red, BLE) y a los 10 s sin ella entra la siguiente; Ajustes > Estado dice
  de dónde sale la posición. Sin hacer: elegir a mano la fuente preferida, y evitar saltos entre dos fuentes de la misma
  clase (gpsd por TCP y otra por UDP).
- **Tracker APRS LoRa+ como fuente (2026-11-11; Manuel: «consulta el repositorio tracker»)**: leído `EA7JRS/aprs-lora-plus`
  (público, clonado en `/home/claude/ea7jrs/aprs-lora-plus`, commit 09e3d92). Lo que hay en el tracker: (1) **BLE**:
  característica NMEA `4c6f5261-2b00-4ea7-a000-000000000004` (NOTIFY, texto NMEA 0183) del servicio de control, solo con
  enlace emparejado con el PIN (`authed`) y suscrito; anuncia el servicio KISS o el UART de Nordic, no el de control; estado
  de conexión único (un central a la vez); (2) **WiFi**: servidor TCP 10110 (`nmea_link.cpp`), pero **de compilación**
  (`NMEA_TCP` 0 por defecto en la Supreme; red fija en `NMEA_WIFI_SSID` o punto de acceso propio «APRSLoRa+tracker-NMEA»;
  WiFi siempre encendido = batería); (3) **UART** a 38400 hacia un conversor RS-422, limitada a `!AIVDM` y MDA
  (`NMEA_UART_PLOTTER_ONLY` 1, pensada para el Garmin): no saca la posición. Sentencias: RMC, GGA, VTG, GLL, ZDA, MDA, XDR y
  `!AIVDM`; solo placas con `NMEA_OUTPUT` (T-Beam Supreme). **Hecho en el plotter (rama `ble-tracker`)**: el BLE busca
  también el servicio KISS, y si el equipo tiene el servicio de control se empareja con el PIN (`settings.blePin`, Ajustes >
  Red) y se suscribe a su NMEA; las estaciones APRS oídas por el tracker entran como blancos AIS. Sin tocar el firmware del
  tracker. Sin probar con un tracker. Límite: plotter y app del móvil no pueden estar conectados a la vez al tracker.
  **Móvil y plotter a la vez (Manuel, 2026-11-12: «1 y 2»)**: (1) *por el móvil*, sin cambios: la app Android sirve el NMEA
  del tracker por TCP 10110 en todas sus interfaces (`NmeaServer.kt`, hay que encenderlo en la app) y el plotter, unido al
  punto de acceso del móvil, lo toma con el servidor vacío en Ajustes > Red; (2) *los dos por BLE*: rama
  **`ble-dos-enlaces` del firmware del tracker** (`EA7JRS/aprs-lora-plus`, compila en su CI, **sin fusionar** hasta que
  Manuel la pruebe en la Supreme): tabla de 2 enlaces en `ble_control.cpp` (emparejamiento, MTU y suscripción NMEA por
  conexión; notificaciones solo a los emparejados), el tracker sigue anunciándose con un equipo conectado, y las
  suscripciones / OTA / configuración son del enlace que mandó la última orden. El plotter no necesita cambios.
- **Signal K nativo (2026-11-12, rama `signalk`)**: tipo de servidor «TCP: Signal K» en Ajustes > Red (puerto 8375): al
  conectar se suscribe a `vessels.self` (1 s); `src/sk_link.*` lee cada línea JSON con ArduinoJson (saludo con `self`,
  deltas del barco propio, `navigation.position` como objeto) y la pasa a la tabla con `skApply` del núcleo (fuente `Tcp`).
  Los avisos `notifications.*` en estado `alarm` o `emergency` abren la ventana «SIGNAL K ALARM / EMERGENCY» con el título
  en español del núcleo; se cierra sola cuando el servidor los baja de nivel. Líneas de hasta 4 KB. Compila en la CI; el
  lector JSON **no se ha ejecutado nunca** (en el PC no hay ArduinoJson) ni se ha probado con un servidor. Falta: otros
  barcos del servidor como blancos AIS, mDNS para encontrarlo solo, y publicar datos del plotter.
- **Primera versión 5.4.1 y manuales (2026-11-12, pedido por Manuel)**: `CHANGELOG.md`, notas en `docs/releases/v5.4.1.md`,
  y en `docs/`: `GUIA_RAPIDA.md` / `QUICK_GUIDE_EN.md`, `MANUAL_USUARIO.md` / `USER_MANUAL_EN.md` (con capturas de
  `docs/img`, dibujadas en el PC: rehacerlas con `tools/ui_preview/build.sh` si cambia la interfaz) e `INSTALACION.md` /
  `INSTALLATION_EN.md` (instalación y funcionamiento). **Cada función nueva se documenta en los seis.** La interfaz está
  solo en español; los manuales en inglés dan cada texto en español con su significado.
- **Instalador web, grupo «Náutica» (Manuel, 2026-11-12)**: el plotter se instala con el instalador web de APRS LoRa+
  (`https://ea7jrs.github.io/APRS-LoRa-Plus/#nautica`), junto a Tracker, Rotor, Gateway y Display. Contrato en
  `docs/WEB_INSTALLER.md` del tracker: `.github/workflows/release.yml` (al subir la etiqueta `vX.Y.Z`, **la misma que la
  del tracker**, o una rama `release/vX.Y.Z`: desde la sesión de Claude no se pueden subir etiquetas, así que se sube esa
  rama y la release crea la etiqueta) compila, comprueba que la etiqueta coincide con `PLOTTER_VERSION` y crea la release (preliminar) con
  `web-site-plotter.zip` (`tools/make_web_site.py`: `latest.json` con `device: plotter`, manifiesto de ESP Web Tools sin
  tocar la NVS, imágenes y `ota/`), la imagen completa (0x0) y el firmware (0x10000). En el tracker: lanzador «Náutica»
  en `web/index.html` y `plotter` en `pages.yml`. Para que salga en el sitio: el secreto `FLASHER_TOKEN` del tracker debe
  poder leer este repositorio, y hay que volver a desplegar las Pages del tracker con esa etiqueta.
- **Versión 5.4.2 (2026-11-12; Manuel: «actualiza todos los repositorios, si es necesario subimos el release al siguiente
  nivel»)**: `PLOTTER_VERSION` 5.4.2 (y `library.json` de las dos librerías), notas en `docs/releases/v5.4.2.md`, release
  por la rama `release/v5.4.2` a la vez que tracker, rotor, Display y app.
- README: botón de donativo PayPal.me (`paypal.me/ea7jrs`), como en el resto de repositorios de Manuel.
- **Accesorio: instrumento de viento de 4″ (pedido 2026-11-12)**: equipo aparte sobre la **Waveshare
  ESP32-S3-Touch-LCD-4** (SKU 28154, 480×480, táctil, 16 MB + 8 MB; confirmada por Manuel, no la 4.3C) que lee el NASA
  Target 2 Wind v2 (MWV a 4800 por el RS485), lo muestra y lo pasa al plotter por BLE (`docs/BLE_NMEA.md`), CAN (PGN
  130306) o WiFi (UDP). Manuel pidió replicar la interfaz del Garmin GMI 20: **no se copia**; diseño propio con el lenguaje
  del plotter y los mismos datos (AWA, AWS, TWA, TWS, TWD, deriva). Maquetas en la página «Accesorio» del lienzo
  (`Inst4Viento`, `Inst4Numeros`, `Inst4Historial`, `Inst4Enlace`; generador `gen4.py`). Página «Rosa» en tres
  repartos (`Inst4Rosa` banda única, `Inst4RosaBandas`, `Inst4RosaColumna`): a Manuel le gustan los tres (2026-11-12), así
  que serán estilos elegibles en Ajustes. Pidió también los valores en las esquinas como en el GMI 20: rechazado, sería la
  pantalla de Garmin. Del manual del GMI 20, solo
  funciones: páginas configurables de 1 a 4 datos, paso automático, vistas de aguja / gráfica / número, alarmas de viento,
  modo noche. Wiki de Waveshare (con contradicciones internas, contrastar con el esquema): CAN TX 6 / RX 0, RS485 TX 44 /
  RX 43, USB 19 / 20 (no comparte con el CAN, al revés que la 7B), expansor TCA9554 (zumbador en EXIO5, SD_CS en EXIO3),
  RTC, cargador SW6106, entrada 7–36 V. Las fuentes GMI40 de Garmin no se han leído. Sin confirmar: nivel eléctrico de la
  salida NMEA del NASA (¿vale directo al RS485?).
  **Firmware (2026-11-12, rama `instrumento-viento-fw`; Manuel eligió «lo recomendado»: segundo entorno en este
  repositorio)**: entorno `waveshare_lcd4_wind` (`build_src_filter`: `src/wind/`, `board_common.cpp`, `board_lcd4.cpp`,
  fuentes; el del plotter excluye `src/wind/`). `src/board_lcd4.cpp` (revisión **V4.0**: expansor CH32V003 en 0x24 como la
  7B, ST7701 por SPI de 3 hilos con la secuencia del BSP de Waveshare, pines del BSP y de ESPHome; tiempos del panel de
  memoria, sin verificar; con TCA9554 en 0x20 = revisión anterior, no soportada). `src/wind/`: `wui` (Rosa en 3 repartos,
  Viento, Números, Historial 30 min, Ajustes y enlace desplazable, pantalla de WiFi con teclado), `wlinks` (veleta por el
  RS485 a 4800 como fuente `Sensor`, simulador que se calla con veleta, NMEA 2000 escucha / emite con `n2ktx`,
  salida 4 Hz de MWV R y T), `wble` (periférico con el servicio de `docs/BLE_NMEA.md`, troceo por MTU), `wnet` (WiFi
  STA, MWV por UDP de difusión al puerto 10110; el plotter tiene el UDP apagado de serie: hay que poner 10110),
  `wsettings` (NVS «wind»), `wmain` (consola `APRS-INFO?` → `device=wind`). Núcleo `n2ktx`: `n2kMakeId`, `n2kWind`
  (130306), NAME (fabricante 2046 sin registrar, clase 85 / función 130) y `N2kAddressClaim` (60928, petición 59904,
  cambio de dirección; empieza en la 35). Vista previa: `tools/ui_preview/build_wind.sh <lvgl> salida.ppm [página]
  [segundos] [reparto] [wifi]`. CI y release compilan los dos entornos; `make_web_site.py` junta las dos placas en el
  mismo `latest.json` (grupo Náutica). Instalador del tracker: `ALIAS` `wind` → `plotter` para que la detección lleve
  al grupo Náutica. Manual: `docs/INSTRUMENTO_VIENTO.md` / `docs/WIND_INSTRUMENT_EN.md` (+ apartado en los seis).
  **Modo noche y alarmas (2026-11-12, «Avanza»)**: paleta cambiable en `wui` (`palette`, `build()` rehace toda la
  interfaz con `lv_async_call`; noche = rojos sobre negro), núcleo `windalarm` (viento real / aparente fuerte con 5 s para
  saltar e histéresis del 5 % y 10 s para apagarse; rolada del viento real suavizada 30 s, referencia a los 30 s,
  salta a los 10 s; silenciar 10 min / cerrar), ventana STRONG WIND / WIND SHIFT en `lv_layer_top` y zumbador EXIO6
  (`wui::tick`, 0,15 s cada segundo). Ajustes: 12 botones (+ Noche, Alarma real 15–40 kn, aparente 20–45 kn, rolada
  10–45°). Vista previa: quinto argumento con `wifi`, `night`, `alarm`. 1517 comprobaciones.
  **Acerca de y rumbo del plotter (2026-11-12; Manuel: «Sí», «debe incluir el QR como los otros dispositivos»)**:
  `src/about_text.h` (textos comunes y `about::donateQr`, el QR de paypal.me/ea7jrs con el marco degradado verde → cian
  de APRS LoRa+ Display; `LV_USE_QRCODE` en `lv_defs.h`), pantalla «Acerca de» en el plotter (botón en Ajustes > Estado,
  acción `acerca` del dibujo) y en el instrumento (botón 13 de Ajustes, opción `about`). Plotter → instrumento por BLE:
  `ble::write` (cola de 1 KB, se manda en el bucle de la tarea en trozos del MTU a `…0002`), `links.cpp` cada segundo
  HDT / VTG / VHW con lo que haya; el instrumento (`wble` con `onWrite` y `read`) lo mete como fuente `Ble`, por debajo
  del NMEA 2000. Por WiFi no (el instrumento no escucha UDP).
  **Bienvenida (2026-11-12, Manuel: «el logo de bienvenida tras el arranque utilizado en otras pantallas»)**: la de APRS
  LoRa+ Display (`tools/assets/welcome_logo.jpg` copiado de allí, `tools/make_ui_assets.py` → `src/ui_assets.cpp`, una
  imagen I8 por placa dentro de su `#if`: 397×540 en la 7B, 294×400 en la LCD-4; `showSplash` en `ui_assets.h`: fundido,
  «Plotter» / «Wind» y la versión en cian #28B5E1, a los 2,2 s pasa a la pantalla principal). En el instrumento, `build()`
  usa `mainScr` (durante la bienvenida la activa es otra). Ocupa 215 KB / 119 KB de flash.
  Falta: probar en la placa y con el NASA, páginas configurables, rumbo y velocidad del plotter por WiFi, que las
  alarmas lleguen al plotter, PGN 126996, guardar la dirección reclamada.
- **Conectividad común para el grupo Display (2026-11-12; Manuel: «el sistema de conectividad ble/wifi/tcp/UDP del plotter
  puede integrarse en el resto de pantallas del grupo Display… y en la app Android»; empezar por el Display; «el repositorio
  display ya existe»: **no hay repositorio común aparte**)**: `src/net.*`, `src/ble.*` y `src/sk_link.*` pasan a
  `lib/aprs_links` (`library.json` con NimBLE 2.5.2 y ArduinoJson 7.4.3) y `lib/plotter_core` lleva su `library.json`.
  **El plotter es el origen**: aquí se cambian y se prueban; `EA7JRS/APRS-LoRa-Plus-Display` lleva copias en `lib/`
  (`tools/sync_links.sh <checkout del plotter>` de la Display, `lib/ORIGEN.txt` con el commit) y su pestaña **Network**
  (rama `conectividad` de la Display): WiFi, servidor TCP NMEA / gpsd / Signal K, UDP, OpenWeatherMap, datos del barco con
  origen y AIS con CPA, en las cuatro placas. Tras cambiar `lib/` aquí, volver a copiar en la Display. BLE NMEA en la
  Display sin activar (choca con su cliente NimBLE del tracker). App Android: en Kotlin, se porta el comportamiento
  (cliente del servicio BLE NMEA, gpsd, Signal K, UDP, prioridad de fuentes) con los mismos casos de prueba del núcleo.
- **Rotor y hora (2026-11-12)**: `lib/aprs_links` también va al rotor (`EA7JRS/APRS-LoRa-Plus-Rotor`, `firmware/lib`, su
  `tools/sync_links.sh`): `net::Config.ownWifi = false` (la WiFi la lleva el programa, también como punto de acceso),
  `aprs_net.h` (otro nombre de `net.h`) y `ble` sin hacer nada si NimBLE va sin central. **Hora en todos los equipos, también el
  tracker** (5.4.2, `include/time_sync.h` del tracker; Manuel: «los dispositivos APRS LoRa+ tracker no son fork de CA2RXU»): núcleo `timesync` (`nmeaUtc` RMC / ZDA, `isoUtc`, `n2kUtc`, `ClockSync`:
  GPS con dos horas que cuadren, manda 6 h sobre NTP; luego la de otro equipo y el RTC) y PGN 126992 / 129029
  (`N2kReader::takeUtc`); `lib/aprs_links/tsync` (`startNtp`: `ntp.roa.es`, `hora.roa.es`, `pool.ntp.org`; `offer`,
  `offerNmea`, `status`). Plotter: hora de RMC / ZDA de serie, red y BLE, y del CAN (no del simulador); `net` arranca NTP al
  conectar; Ajustes > Estado > *Hora*; ZDA al instrumento por BLE (solo con hora de GPS o NTP). Instrumento: ZDA del plotter
  (fuente `Peer`), CAN, y NTP al conectar su WiFi; fila Bluetooth con la hora. Manuel preguntó por NTPsec: en los ESP32 solo
  hay SNTP (decenas de ms); NTPsec va en la Raspberry del rotor (`install.sh`, NTS con `nts1.roa.es`, GPS / PPS de gpsd).
  Tras cambiar `lib/`, volver a copiar en la Display y en el rotor.
- Garmin GMI 20 (enlace de Manuel, 2026-11-11, `garmin.com/es-ES/p/126694`): la página solo dejó leer el nombre (instrumento
  náutico) y las referencias; sin especificaciones. Propuesto a Manuel, sin decidir: páginas de Instrumentos configurables
  (1 / 2 / 3 / 4 datos grandes) y gráficas de historial (viento, profundidad, velocidad), con diseño propio.
- `astro` usa la serie de NOAA (~1 min); las pruebas del sol son de coherencia, no contra un almanaque.

## Siguiente
1. Hechos Instrumentos, Carta (teselas, ir a, waypoints, MOB), Radar AIS, alertas y Ajustes básicos (ver Estado). Rutas y
   medir hechos. Fondeo, unidades, mapa base, NMEA 2000 (escucha) y red (WiFi, NMEA por TCP / UDP, gpsd) hechos. Pestaña Viento hecha. Pestaña Piloto hecha. Relevo por internet y pestaña Meteo hechos. BLE con la pasarela hecho (lado plotter). Signal K nativo hecho. Lo siguiente: zumbador (pin por decidir), AEMET, lista de equipos BLE, y probar en la placa.
2. NMEA 2000: hecho en escucha con decodificador propio; más PGN según lo que haya en la red de Manuel.
3. Carta con teselas en microSD y costa de reserva, waypoints y rutas (GPX); radar AIS; alarmas; zumbador externo; reproducción NMEA.
4. Piloto: Raymarine ST2000+ (hecho el perfil de datos). Salida a 4800 (Manuel, 2026-11-12: «2 y 3 pendientes»): **pasarela XIAO por Bluetooth** (falta su firmware: recibir por `…0002` y sacarlo a 4800 por su NMEA 0183) y **adaptador WiFi-NMEA** (salida «Red (TCP)»), las dos pendientes; el RS485 sigue para el AIS a 38400. Mandos por SeaTalk1, sin decidir. Viento: entra por la pasarela XIAO (CAN, UDP por WiFi o BLE); el plotter ya tiene el BLE (docs/BLE_NMEA.md); falta readaptar el firmware de la XIAO.
