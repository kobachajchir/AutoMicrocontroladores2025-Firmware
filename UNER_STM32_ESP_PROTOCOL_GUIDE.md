# UNER STM32 <-> ESP Protocol Guide

## 1. Objetivo

Este documento describe el contrato real que hoy existe entre el firmware STM32 y el firmware ESP8266/ESP-01 para que un programador senior del lado ESP pueda implementar o mantener una libreria 100% compatible con el lado MCU.

El objetivo no es solo listar comandos. Tambien deja claro:

- como viaja cada frame;
- que payload espera cada comando;
- que devuelve cada respuesta;
- que mensajes son eventos asincronos;
- que mensajes especiales usan finalizadores `0xFE` sin `status`;
- que cosas consume hoy el STM32;
- que cosas existen en la tabla pero todavia no tienen payload funcional cerrado;
- que funcionalidades del auto existen solo en STM32 y aun no tienen contrato UNER.

## 2. Fuentes de verdad usadas para este documento

### 2.1 Lado STM32

- `Core/Inc/uner_v2.h`
- `Core/Inc/uner_app.h`
- `Core/Inc/uner_handle.h`
- `Core/Src/uner_app.c`
- `Core/Src/uner_handle.c`
- `Core/Src/uner_transport_uart1_dma.c`
- `Core/Src/main.c`
- `Core/Src/app_globals.c`
- `Core/Src/eventManagers.c`
- `Core/Inc/types/carmode_type.h`
- `Core/Inc/motor_control.h`
- `Core/Inc/mpu6050.h`
- `Core/Src/usart.c`

### 2.2 Lado ESP

- `../Firmware ESP01/FirmESP01MicroArduino2025/src/uner_cmdspec.h`
- `../Firmware ESP01/FirmESP01MicroArduino2025/src/uner_cmdspec.c`
- `../Firmware ESP01/FirmESP01MicroArduino2025/src/uner_app_handlers.h`
- `../Firmware ESP01/FirmESP01MicroArduino2025/src/uner_app_handlers.cpp`
- `../Firmware ESP01/FirmESP01MicroArduino2025/src/uner_handle.cpp`
- `../Firmware ESP01/FirmESP01MicroArduino2025/src/uner_v2.h`
- `../Firmware ESP01/FirmESP01MicroArduino2025/src/main.cpp`
- `../Firmware ESP01/FirmESP01MicroArduino2025/src/config.h`
- `../Firmware ESP01/FirmESP01MicroArduino2025/src/webserver_ws.cpp`
- `../Firmware ESP01/FirmESP01MicroArduino2025/docs/UNER_PROTOCOL_COMMANDS_ES.md`
- `../Firmware ESP01/FirmESP01MicroArduino2025/docs/UNER_v2_documentacion_tecnica.md`

### 2.3 Importante: codigo legacy que NO debe usarse como referencia principal

En el repo del ESP existe tambien una capa vieja/experimental:

- `UNERUtils.cpp`
- `uner_device.h`
- `uner_device.c`

Esa capa usa otra familia de comandos, otros limites y otra semantica:

- payload maximo de 30 bytes;
- comandos `HEARTBEAT_*`, `WIFI_*`, `TELEM_*`;
- telemetria MPU6050 y scan list distintos del contrato actual;
- no coincide con la tabla actual `uner_cmdspec`.

Para compatibilidad con el STM32 actual, la fuente de verdad es `uner_app_handlers.cpp` + `uner_cmdspec.c`, no `UNERUtils/uner_device`.

## 3. Topologia logica y transporte

### 3.1 Transporte fisico actual

- enlace serial UART1 STM32 <-> Serial ESP;
- velocidad: `115200`;
- formato: `8N1`;
- sin flow control;
- STM32 usa RX DMA circular;
- el buffer RX DMA actual del STM32 es de `512` bytes;
- el payload maximo del protocolo es `255` bytes;
- el frame maximo real es `255 + 10 = 265` bytes.

Eso implica que el parser debe ser de stream binario. No sirve un parser por lineas ni asumir lecturas completas de frame en una sola rafaga.

### 3.2 IDs de nodo

Los nodos definidos en UNER v2 son:

- `0x1` = MCU
- `0x2` = PC / Qt
- `0x3` = Web App
- `0x4` = NRF remoto
- `0xF` = broadcast

### 3.3 Identidad logica actual del ESP

El ESP actual se inicializa con:

```cpp
UNER_App_Init(&g_unerApp, 0x02, 0x01);
```

Eso significa:

- `this_node = 0x02`
- `peer_node = 0x01`

En la practica, el ESP usa el ID logico `0x02`, historicamente llamado `PC/Qt`. El STM32 ya esta programado para enviarle comandos al nodo `0x02`. Por lo tanto, si el programador del ESP cambia este ID sin coordinar ambos lados, rompe el ruteo.

### 3.4 ROUTE en wire

El byte `ROUTE` empaqueta:

- nibble alto = `src`
- nibble bajo = `dst`

Ejemplos reales:

- MCU -> ESP: `ROUTE = 0x12`
- ESP -> MCU: `ROUTE = 0x21`

## 4. Estructura exacta del frame

Cada frame UNER v2 tiene este formato:

1. `H0 = 'U' = 0x55`
2. `H1 = 'N' = 0x4E`
3. `H2 = 'E' = 0x45`
4. `H3 = 'R' = 0x52`
5. `LEN` = longitud del payload
6. `TOKEN = 0x3A`
7. `VER = 0x02`
8. `ROUTE`
9. `CMD`
10. `PAYLOAD`
11. `CHK`

Reglas:

- `LEN` cuenta solo los bytes de `PAYLOAD`.
- `CHK` es XOR de todos los bytes previos del frame.
- el overhead fijo es de `10` bytes.
- no hay separadores ASCII, saltos de linea ni terminadores de texto.

## 5. Reglas generales de operacion

### 5.1 ACK y NACK

Cuando una entrada de tabla tiene flag `UNER_SPEC_F_ACK`, el `UNER_Handle` del ESP envia automaticamente:

- `CMD = 0xE0`
- `payload = [acked_cmd, status]`

Si el comando es invalido, el `UNER_Handle` del ESP envia:

- `CMD = 0xE1`
- `payload = [cmd_rechazado, reason]`

En el lado ESP, esto si esta implementado automaticamente en `uner_handle.cpp`.

### 5.2 Respuesta normal de aplicacion

La mayoria de los handlers del ESP responden con el mismo `CMD` de la solicitud y payload con esta forma:

```text
payload[0] = status
payload[1..] = data opcional
```

Entonces, cuando este documento diga "response data", en wire real llega precedida por `status`.

### 5.3 Finalizadores asincronos raw

Hay algunos casos especiales donde el ESP envia un frame push con el mismo `CMD`, pero sin prefijo `status`.

Esos frames se reconocen porque:

- `payload[0] == 0xFE`
- el resto del payload tiene semantica especial

Hoy existen estos finalizadores raw:

- scan terminado:
  - `CMD = 0x15`
  - `payload = [0xFE]`
- STA conectada:
  - `CMD = 0x48`
  - `payload = [0xFE, ip0, ip1, ip2, ip3]`
- AP lista:
  - `CMD = 0x4B`
  - `payload = [0xFE, ip0, ip1, ip2, ip3]`

Nota de compatibilidad MCU:

- el parser STM32 actual tolera tambien `CMD = 0x14` con `payload[0] == 0xFE`;
- el ESP actual NO envia ese caso para scan;
- el contrato real del ESP hoy para fin de scan es `0x15 + [0xFE]`.

### 5.4 Limites practicos

- `UNER_MAX_PAYLOAD = 255`
- `SSID_MAX_LEN = 32`
- `PASS_MAX_LEN = 63`
- el STM32 actual guarda como maximo `8` SSIDs de scan
- el STM32 actual recorta SSID a `32` chars
- el ESP actual trackea como maximo `8` clientes WebSocket para `GET_USER_INFO`

## 6. Payloads reutilizados

### 6.1 `wifi_info`

Formato:

```text
[mode, wl_status, ip0, ip1, ip2, ip3, ssid_len, ssid...]
```

Campos:

- `mode` = `WiFi.getMode()`
- `wl_status` = `WiFi.status()`
- `ip0..ip3` = IPv4
- `ssid_len` = longitud de SSID
- `ssid` = bytes ASCII/UTF-8 sin terminador

El ESP arma este bloque con `append_wifi_info(...)`.

### 6.2 `target_credentials`

Formato:

```text
[target, ssid_len, ssid..., pass_len, pass...]
```

Campos:

- `target = 0` -> AP
- `target = 1` -> STA

### 6.3 `all_credentials`

Formato:

```text
[sta_ssid_len, sta_ssid..., sta_pass_len, sta_pass..., ap_ssid_len, ap_ssid..., ap_pass_len, ap_pass...]
```

### 6.4 `network_ip` / `boot_complete`

Formato:

```text
[net_if, ip0, ip1, ip2, ip3]
```

Campos:

- `net_if = 0x01` -> STA
- `net_if = 0x02` -> AP

### 6.5 `GET_STATUS` response data

La data de `GET_STATUS` es:

```text
[ap_active, wifi_active, wifi_info..., sta_ip0, sta_ip1, sta_ip2, sta_ip3, ap_ip0, ap_ip1, ap_ip2, ap_ip3]
```

En wire real llega como:

```text
[status, ap_active, wifi_active, wifi_info..., sta_ip..., ap_ip...]
```

Cuando `ssid_len = 0`, el largo minimo total del payload es `18` bytes:

```text
1 status + 2 flags + 7 bytes base de wifi_info + 8 bytes de IPs finales
```

### 6.6 `GET_SCAN_RESULTS` response data

Cuando `status = 0`, la data es:

```text
[count, ssid1_len, ssid1..., ssid2_len, ssid2..., ...]
```

Cuando `status = 1`, la data actual es:

```text
[0]
```

Cuando `status = 2`, la data actual es:

```text
[0]
```

Entonces, el payload completo en wire queda:

- scan listo sin redes: `[0, 0]`
- scan listo con redes: `[0, count, len1, ssid1..., len2, ssid2..., ...]`
- scan en progreso: `[1, 0]`
- scan detenido por comando: `[2, 0]`

### 6.7 Screen codes STM32 reportados por UI

Los screen codes son identificadores estables de pantallas visibles. El STM32 los reporta por `EVT_SCREEN_CHANGED (0x95)` y los usa para validar comandos de UI remotos que incluyen pantalla destino.

Definicion:

```c
SCREEN_CODE(menu, submenu, page) =
    ((menu & 0xFF) << 16) | ((submenu & 0xFF) << 8) | (page & 0xFF)
```

En payload UNER viajan little-endian:

```text
[code0, code1, code2, code3, source]
```

Fuentes (`ScreenReportSource_t`):

- `0` = unknown
- `1` = menu
- `2` = render
- `3` = notification
- `4` = permission
- `5` = system

Screen codes WiFi actuales:

| Macro | Valor | Uso |
| --- | ---: | --- |
| `SCREEN_CODE_CONNECTIVITY_WIFI_MENU` | `0x020101` | submenu WiFi |
| `SCREEN_CODE_CONNECTIVITY_WIFI_STATUS` | `0x020201` | estado WiFi/AP/IP |
| `SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING` | `0x020202` | busqueda activa, emite `START_SCAN` al entrar |
| `SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS` | `0x020203` | lista de redes encontradas y acciones `Actualizar`/`Volver` |
| `SCREEN_CODE_CONNECTIVITY_WIFI_NOT_CONNECTED` | `0x020204` | notificacion sin red |
| `SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTING` | `0x020205` | notificacion conectando |
| `SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTED` | `0x020206` | notificacion conectado |
| `SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_COMPLETE` | `0x020207` | notificacion historica de busqueda completa |
| `SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_CANCELED` | `0x020208` | notificacion busqueda cancelada |
| `SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS` | `0x020209` | detalles de la red seleccionada, permite volver a resultados o pedir credenciales web |
| `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_WEB` | `0x02020A` | notificacion "Ingresar credenciales en web" |
| `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_SUCCEEDED` | `0x02020B` | notificacion de credenciales web aceptadas y WiFi conectado |
| `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_FAILED` | `0x02020C` | notificacion de credenciales web rechazadas, timeout o cancelacion |

Semantica UI WiFi STM32:

- entrar a `WIFI_SEARCHING` o pulsar `Actualizar` limpia resultados locales y vuelve a emitir `START_SCAN`;
- al llegar resultados finales desde ESP, el STM32 renderiza `WIFI_RESULTS` sin esperar rotacion del encoder;
- al salir del flujo `WIFI_SEARCHING`/`WIFI_RESULTS`/`WIFI_DETAILS`, el STM32 borra SSIDs/encriptaciones locales;
- volver desde `WIFI_DETAILS` a `WIFI_RESULTS` no borra los resultados;
- pulsar encoder en `WIFI_DETAILS` envia `SET_CREDENTIALS` con `pass = "connRequest"` y muestra `WIFI_CREDENTIALS_WEB`.
- el resultado final del flujo web llega por `0x5C WIFI_CREDENTIALS_WEB_RESULT` y dispara `WIFI_CREDENTIALS_SUCCEEDED` o `WIFI_CREDENTIALS_FAILED`.

## 7. Catalogo completo de comandos

## 0x10 `SET_MODE_AP`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload: `[status, wifi_info...]`
- `status`:
  - `0` = OK
- Efecto ESP:
  - `WiFi.mode(WIFI_AP)`
  - `WiFi.softAP(apSsid, apPass)`
- Side effects:
  - emite `EVT_MODE_CHANGED` con `wifi_info`
- Consumo STM32 hoy:
  - no parsea el response de forma dedicada;
  - si parsea `EVT_MODE_CHANGED` y actualiza flags/IP.

## 0x11 `SET_MODE_STA`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload: `[status, wifi_info...]`
- `status`:
  - `0` = OK
- Efecto ESP:
  - `WiFi.mode(WIFI_STA)`
  - deshabilita auto reconnect interno
  - `WiFi.begin(staSsid, staPass)`
- Side effects:
  - emite `EVT_MODE_CHANGED`
- Consumo STM32 hoy:
  - mismo criterio que `0x10`.

## 0x12 `SET_CREDENTIALS`

- Direccion normal: MCU -> ESP
- Request payload:

```text
[target, ssid_len, ssid..., pass_len, pass...]
```

- ACK automatico: si
- Response payload:
  - error: `[status]`
  - OK: `[0, target_credentials...]`
- `status`:
  - `0` = guardado OK
  - `1` = payload demasiado corto
  - `2` = `ssid_len` invalido o desborde
  - `3` = `pass_len` invalido o desborde
  - `4` = fallo al guardar NVS
- Notas:
  - `target = 0` -> AP
  - `target = 1` -> STA
  - desde la pantalla STM32 `SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS`, al pulsar encoder sobre "Conectar", el STM32 usa este comando con `target = 1`, el SSID seleccionado y `pass = "connRequest"`;
  - ese `pass` no es una clave WiFi real: es una senal para que el ESP/Web abra o mantenga el flujo de ingreso de credenciales desde la web;
  - despues de enviar ese comando, la STM32 muestra `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_WEB` como notificacion y vuelve a la pantalla de detalles para permitir reintento.

## 0x13 `CLEAR_CREDENTIALS`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload:
  - error: `[status]`
  - OK: `[0, target_credentials_ap...]`
- `status`:
  - `0` = OK
  - `1` = fallo NVS
- Notas:
  - restaura defaults AP y STA;
  - el response exitoso devuelve solo las credenciales AP por compatibilidad historica.

## 0x14 `START_SCAN`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload:
  - exito: `[0]`
  - error: `[1]`
- `status`:
  - `0` = scan iniciado
  - `1` = no se pudo iniciar
- Efecto ESP:
  - borra resultados anteriores con `WiFi.scanDelete()`
  - lanza `WiFi.scanNetworks(true)`
  - guarda `pkt->src` como destino del finalizador asincrono
  - usa ventana maxima de `10` segundos
- Finalizador asincrono:
  - `CMD = 0x15`
  - `payload = [0xFE]`
- Flujo recomendado:
  1. enviar `0x14`
  2. esperar finalizador `0x15 + [0xFE]` o consultar periodicamente `0x15 GET_SCAN_RESULTS`
  3. pedir/seguir pidiendo `0x15 GET_SCAN_RESULTS` hasta obtener una respuesta final

## 0x15 `GET_SCAN_RESULTS`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: no
- Response payload:
  - scan listo sin redes: `[0, 0]`
  - scan en progreso: `[1, 0]`
  - scan detenido: `[2, 0]`
  - scan listo con redes: `[0, count, len1, ssid1..., len2, ssid2..., ...]`
- `status`:
  - `0` = resultados listos
  - `1` = scan en progreso
  - `2` = scan detenido por `STOP_SCAN`
- Notas de parseo:
  - cada SSID es `len + bytes`, sin terminador;
  - si `status = 1`, el STM32 mantiene la sesion de busqueda activa y vuelve a pedir resultados luego;
  - si `status = 0` o `status = 2`, el STM32 limpia el array local antes de parsear la respuesta final;
  - el STM32 actual guarda solo las primeras `8` redes y recorta cada SSID a `32` chars;
  - el payload actual del ESP no envia tipo de seguridad/encriptacion; el STM32 muestra `"[ENCRYPTION]"` como placeholder en la pantalla de detalles.

### Ejemplo real observado

Payload observado por UART:

```text
00 06 04 4B 6F 62 61 13 4D 45 47 41 46 49 42 52 41 2D 32 2E 34 47 2D 56 34 52 73 0D 62 61 69 6C 61 74 74 69 5F 77 69 66 69 13 4D 45 47 41 46 49 42 52 41 2D 32 2E 34 47 2D 65 5A 38 4D 12 50 65 72 73 6F 6E 61 6C 20 57 69 66 69 20 5A 6F 6E 65 13 49 6E 74 65 72 6E 65 74 50 6C 75 73 5F 39 33 66 37 66 38
```

Interpretacion:

- `00` -> `status = 0`
- `06` -> `count = 6`
- `04 'Koba'`
- `13 'MEGAFIBRA-2.4G-V4Rs'`
- `0D 'bailatti_wifi'`
- `13 'MEGAFIBRA-2.4G-eZ8M'`
- `12 'Personal Wifi Zone'`
- `13 'InternetPlus_93f7f8'`

## 0x16 `REBOOT_ESP`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response de aplicacion: no garantizada
- Efecto ESP:
  - emite `EVT_USB_DISCONNECTED`
  - `Serial.flush()`
  - `ESP.restart()`
- Consumo STM32 hoy:
  - usa el ACK para salir del estado de espera;
  - luego puede recibir `EVT_USB_DISCONNECTED`.
- Frame exacto STM32 -> ESP:

```text
55 4E 45 52 00 3A 02 12 16 30
```

## 0x17 `FACTORY_RESET`

- Alias funcional de `0x13 CLEAR_CREDENTIALS`
- Request, response y status: iguales a `0x13`

## 0x18 `STOP_SCAN`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload: `[0, 1]`
- `status`:
  - `0` = OK
- Data:
  - `1` = stopped-by-command
- Side effects:
  - borra resultados temporales
  - dispara finalizador asincrono `0x15 + [0xFE]`

## 0x19 `RESET_MCU`

- Direccion normal: ESP -> MCU
- Request payload: vacio
- ACK automatico: no
- ACK manual previo al reset: si, payload `[0x19, 0x00]`
- Response de aplicacion: no
- Efecto STM32:
  - envia `ACK` al `src` del comando recibido;
  - deshabilita interrupciones;
  - ejecuta `NVIC_SystemReset()`;
  - reinicia la STM32 por software.
- Notas:
  - el ESP puede usar este ACK para feedback visual, pero debe manejar timeout porque despues del ACK la MCU reinicia;
  - no hay response normal de aplicacion, solo ACK;
  - este comando reinicia solo la STM32, no necesariamente reinicia el ESP ni perifericos externos.

Frame exacto ESP -> MCU:

```text
55 4E 45 52 00 3A 02 21 19 0C
```

ACK esperado MCU -> ESP:

```text
55 4E 45 52 02 3A 02 12 E0 19 00 DD
```

## 0x30 `GET_STATUS`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: no
- Response payload:

```text
[0, ap_active, wifi_active, wifi_info..., sta_ip0, sta_ip1, sta_ip2, sta_ip3, ap_ip0, ap_ip1, ap_ip2, ap_ip3]
```

- `status`:
  - `0` = OK
- Notas:
  - `ap_active` y `wifi_active` son flags rapidos;
  - dentro de `wifi_info` vuelve a viajar `mode` y `wl_status`;
  - el STM32 actual toma los ultimos 8 bytes como `sta_ip` y `ap_ip`, sin depender del offset exacto del SSID.

## 0x31 `PING`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: no
- Response payload: `[0]`
- `status`:
  - `0` = OK
- Consumo STM32 hoy:
  - marca `ESP_PRESENT`
  - muestra notificacion de ping recibido.

## 0x40 `GET_PREFERENCES`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload:

```text
[0, ap_len, ap_ssid..., sta_len, sta_ssid...]
```

- `status`:
  - `0` = OK
- Notas:
  - no incluye passwords;
  - orden actual: AP primero, luego STA.

## 0x41 `REQUEST_FIRMWARE`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload:

```text
[0, ascii_fw...]
```

- `status`:
  - `0` = OK
- Valor actual del ESP:
  - `"esp01-2026.02"`
- Consumo STM32 hoy:
  - copia hasta `32` chars en `espFirmwareVersion`.

## 0x42 `ECHO`

- Direccion normal: MCU -> ESP
- Request payload: exactamente 4 bytes
- ACK automatico: no
- Response payload:

```text
[0, same_byte0, same_byte1, same_byte2, same_byte3]
```

- `status`:
  - `0` = OK

## 0x43 `SET_ENCODER_FAST`

- Direccion normal: MCU -> ESP
- Request payload:

```text
[accepted]
```

- ACK automatico: si
- Response payload:

```text
[0, accepted]
```

- `status`:
  - `0` = OK
- Notas:
  - hoy el ESP solo valida y hace eco del byte;
  - no lo persiste ni cambia comportamiento WiFi;
  - el contrato correcto en wire es `[status, accepted]`.

### Mismatch conocido en STM32

El STM32 actual tiene dos ramas de parseo para `0x43`:

- una toma `payload[1]` como valor;
- otra toma `payload[0]`.

Como el response normal del ESP es `[0, accepted]`, esa segunda rama puede terminar leyendo `status` en vez de `accepted`.

Recomendacion:

- NO cambiar el formato del ESP para "acomodar" este bug de parseo;
- mantener el contrato correcto `[status, accepted]`;
- corregir el parser del STM32 por separado si se quiere comportamiento consistente.

## 0x44 `GET_CONNECTED_USERS`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: no
- Response payload:

```text
[0, ws_count]
```

- `status`:
  - `0` = OK
- Notas:
  - cuenta solo clientes WebSocket activos.

## 0x45 `GET_USER_INFO`

- Direccion normal: MCU -> ESP
- Request payload:

```text
[index]
```

- ACK automatico: no
- Response payload:

```text
[status, index, id0, id1, id2, id3, ip0, ip1, ip2, ip3]
```

- `status`:
  - `0` = indice valido
  - `1` = indice invalido o slot no ocupado
- Notas:
  - `id` viaja little-endian;
  - el bloque de data siempre ocupa 9 bytes;
  - la implementacion actual del ESP trackea hasta 8 clientes.

## 0x46 `GET_INTERFACES_CONNECTED`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: no
- Response payload:

```text
[0, PC, WEB, RF, ESP]
```

- `status`:
  - `0` = OK
- Semantica actual:
  - `PC = 1` mientras exista el bridge serial
  - `WEB = 1` si hay algun WS conectado
  - `RF = 0` no implementado
  - `ESP = 1` siempre

## 0x47 `GET_CREDENTIALS`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: no
- Response payload:

```text
[0, sta_ssid_len, sta_ssid..., sta_pass_len, sta_pass..., ap_ssid_len, ap_ssid..., ap_pass_len, ap_pass...]
```

- `status`:
  - `0` = OK
- Nota:
  - contiene passwords en claro.

## 0x48 `CONNECT_WIFI`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload:

```text
[0, wifi_info...]
```

- `status`:
  - `0` = intento iniciado
- Efecto ESP:
  - `WiFi.mode(WIFI_STA)`
  - deshabilita auto reconnect interno
  - `WiFi.begin(staSsid, staPass)`
- Finalizador asincrono:

```text
CMD = 0x48
payload = [0xFE, ip0, ip1, ip2, ip3]
```

- Side effects al conectar realmente:
  - `EVT_STA_CONNECTED`
  - `EVT_WIFI_CONNECTED`
  - `EVT_NETWORK_IP`
  - `CMD_NETWORK_IP`
  - `EVT_BOOT_COMPLETE` una sola vez
  - `CMD_BOOT_COMPLETE` una sola vez
- Consumo STM32 hoy:
  - el response normal no se usa funcionalmente;
  - el finalizador raw si se usa para guardar IP STA, marcar `WIFI_ACTIVE` y mostrar notificacion.

## 0x49 `DISCONNECT_WIFI`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload:

```text
[0, wifi_info...]
```

- `status`:
  - `0` = OK
- Efecto ESP:
  - deshabilita auto reconnect
  - `WiFi.disconnect(true)`

## 0x4A `SET_AP_CONFIG`

- Direccion normal: MCU -> ESP
- Request payload:

```text
[ssid_len, ssid..., pass_len, pass...]
```

- ACK automatico: si
- Response payload:
  - error: `[status]`
  - OK: `[0, target_credentials_ap...]`
- `status`:
  - `0` = guardado OK
  - `1` = payload corto
  - `2` = SSID invalido
  - `3` = PASS invalida
  - `4` = fallo NVS

## 0x4B `START_AP`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload:

```text
[status, wifi_info...]
```

- `status`:
  - `0` = AP levantado
  - `1` = fallo `WiFi.softAP(...)`
- Finalizador asincrono:

```text
CMD = 0x4B
payload = [0xFE, ip0, ip1, ip2, ip3]
```

- Side effects:
  - cuando el ESP detecta modo AP emite tambien:
    - `EVT_MODE_CHANGED`
    - `EVT_NETWORK_IP`
    - `CMD_NETWORK_IP`
    - `EVT_BOOT_COMPLETE` una sola vez
    - `CMD_BOOT_COMPLETE` una sola vez
- Consumo STM32 hoy:
  - usa el finalizador raw para guardar IP AP y marcar `AP_ACTIVE`.

## 0x4C `STOP_AP`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: si
- Response payload:

```text
[0, wifi_info...]
```

- `status`:
  - `0` = OK

## 0x4D `GET_CONNECTED_USERS_MODE`

- Direccion normal: MCU -> ESP
- Request payload: vacio
- ACK automatico: no
- Response payload:

```text
[0, ap_mode, ap_users, ws_users]
```

- `status`:
  - `0` = OK
- Semantica:
  - `ap_mode = 1` si el ESP esta en `WIFI_AP` o `WIFI_AP_STA`
  - `ap_users = softAPgetStationNum()`
  - `ws_users = WebServerWS_GetConnectedCount()`

## 0x4E `SET_AUTO_RECONNECT`

- Direccion normal: MCU -> ESP
- Request payload:

```text
[enable]
```

- ACK automatico: si
- Response payload:

```text
[0, enabled_now]
```

- `status`:
  - `0` = OK

## 0x4F `CMD_BOOT_COMPLETE`

- Este no es un request clasico.
- El ESP actual lo envia como mensaje raw por compatibilidad, usando `CMD` y no solo `EVT`.
- Payload:

```text
[net_if, ip0, ip1, ip2, ip3]
```

- `net_if`:
  - `0x01` = STA
  - `0x02` = AP

### Compatibilidad STM32

El STM32 actual lo tiene en tabla y lo procesa con la misma logica que `EVT_NETWORK_IP`:

- actualiza IP STA/AP segun `net_if`;
- si corresponde a STA y la pantalla/notificacion lo requiere, refresca UI de red.

## 0x50 `CMD_NETWORK_IP`

- Igual criterio que `0x4F`, pero para notificar IP actual.
- Payload:

```text
[net_if, ip0, ip1, ip2, ip3]
```

### Compatibilidad STM32

El STM32 actual lo tiene en tabla y lo procesa con la misma logica que `EVT_NETWORK_IP`.

## 0x51 `AUTH_VALIDATE_PIN`

- Direccion normal: MCU -> ESP
- Request payload:

```text
[request_id, permission_id, pin0, pin1, pin2, pin3]
```

- `request_id`:
  - contador local del STM32 para correlacionar respuesta;
  - nunca debe responderse con otro valor.
- `permission_id`:
  - `1` = `PERMISSION_ESP_RESET`
  - `2` = `PERMISSION_FACTORY_RESET`
  - `3` = `PERMISSION_WIFI_CREDENTIALS`
  - `4` = `PERMISSION_MOTOR_TEST`
  - `5` = `PERMISSION_SYSTEM_CONFIG`
  - `6` = `PERMISSION_CAR_MODE_TEST`
- `pin0..pin3`:
  - digitos numericos `0..9`, un byte por digito.
- ACK automatico: si
- Response payload:

```text
[0, request_id, permission_id, granted, ttl0, ttl1, ttl2, ttl3, attempts_left]
```

- `granted`:
  - `0` = PIN invalido o permiso denegado
  - `1` = PIN valido, permiso concedido
- `ttl0..ttl3`:
  - TTL en milisegundos, little-endian;
  - puede omitirse; si se omite o vale `0`, el STM32 usa el TTL default de la politica local.
- `attempts_left`:
  - intentos restantes;
  - puede omitirse; si se omite, el STM32 descuenta localmente.
- Consumo STM32 hoy:
  - valida que `request_id` y `permission_id` coincidan con la solicitud pendiente;
  - si `granted=1`, autoriza temporalmente la accion pendiente;
  - si `granted=0`, muestra denegado o bloqueado segun `attempts_left`.

Ejemplo request MCU -> ESP para validar PIN `1234`, `request_id=1`, permiso `PERMISSION_ESP_RESET`:

```text
55 4E 45 52 06 3A 02 12 51 01 01 01 02 03 04 75
```

Ejemplo response ESP -> MCU concedido por `30000 ms`, `attempts_left=3`:

```text
55 4E 45 52 09 3A 02 21 51 00 01 01 01 30 75 00 00 03 0A
```

## 0x52 `GET_CURRENT_SCREEN`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload: vacio
- ACK automatico: no
- Response payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3, source]
```

- `screen_code` viaja little-endian.
- `source` usa `ScreenReportSource_t`.

## 0x53 `MENU_ITEM_CLICK`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3, visible_item]
```

- ACK/response de estado manual: `[0]` si se acepto, `[1]` si se rechazo.
- Reglas STM32:
  - `screen_code` debe coincidir con la pantalla actual;
  - la pantalla actual debe ser un menu;
  - `visible_item` es indice visible, no necesariamente indice absoluto interno;
  - en `SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS`, click sobre una red abre `SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS`; click sobre `Actualizar` reinicia scan.

## 0x54 `TRIGGER_ENCODER_BUTTON`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3, press_kind]
```

- `press_kind = 0` -> short press
- `press_kind = 1` -> long press
- En `SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS`, short press envia `SET_CREDENTIALS` con `"connRequest"` y long press vuelve a resultados.

## 0x55 `TRIGGER_USER_BUTTON`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3, press_kind]
```

- `press_kind = 0` -> short press
- `press_kind = 1` -> long press
- En `SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS`, short press vuelve a resultados sin borrar redes.

## 0x56 `REQUEST_SCREEN_PAGE`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3, direction]
```

- `direction = 0` -> page up
- `direction = 1` -> page down
- STM32 mueve `MENU_VISIBLE_ITEMS` pasos y re-renderiza.

## 0x57 `TRIGGER_ENCODER_ROTATE_LEFT`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3]
```

- Genera `UE_ROTATE_CCW` si la pantalla coincide.

## 0x58 `TRIGGER_ENCODER_ROTATE_RIGHT`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3]
```

- Genera `UE_ROTATE_CW` si la pantalla coincide.

## 0x59 `AUTH_PIN_GRANTED`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3]
```

- ACK/response de estado manual:
  - `0` = OK
  - `1` = payload invalido
  - `2` = screen mismatch
  - `3` = la pantalla no requiere grant
  - `4` = no hay permiso pendiente
- Solo aplica a pantallas de PIN/permiso que el STM32 marque como grantables.
- Este comando sigue siendo `grant only`:
  - concede la solicitud pendiente;
  - no permite forzar `denied`, `timeout` ni `blocked`.

## 0x5D `AUTH_REMOTE_RESULT`

- Direccion normal: ESP/Web/PC -> MCU
- Request payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3, result_code, attempts_left]
```

- `screen_code`:
  - viaja little-endian;
  - debe coincidir con la pantalla actual del STM32.
- `result_code`:
  - `0x00` = denied
  - `0x01` = timeout
  - `0x02` = blocked
- `attempts_left`:
  - para `denied`, si vale `0xFF`, el STM32 descuenta localmente;
  - para `blocked`, el STM32 fuerza `0` y muestra/ejecuta la semantica de bloqueo;
  - para `timeout`, el STM32 no necesita usarlo.
- ACK/response de estado manual:
  - `0` = OK
  - `1` = payload invalido
  - `2` = screen mismatch
  - `3` = la pantalla actual no pertenece al flujo de PIN/permiso
  - `4` = no hay permiso pendiente aplicable
- Reglas STM32:
  - solo se acepta si la pantalla actual coincide con `screen_code`;
  - solo se acepta en `SCREEN_CODE_WARNING_PIN_ENTRY` o `SCREEN_CODE_WARNING_PIN_WAITING`;
  - solo se acepta si hay una solicitud de permiso activa;
  - reutiliza la misma logica interna de errores que la validacion local de permisos.
- Efecto STM32:
  - `denied`:
    - actualiza `attempts_left`;
    - si quedan intentos, pasa a `AUTH_DENIED` y reporta `SCREEN_CODE_WARNING_PIN_DENIED`;
    - si no quedan, pasa a `AUTH_LOCKED` y reporta `SCREEN_CODE_WARNING_PIN_BLOCKED`;
  - `timeout`:
    - pasa a `AUTH_TIMEOUT` y reporta `SCREEN_CODE_WARNING_PIN_TIMEOUT`;
  - `blocked`:
    - fuerza `attempts_left = 0`;
    - pasa a `AUTH_LOCKED` y reporta `SCREEN_CODE_WARNING_PIN_BLOCKED`.
- Compatibilidad:
  - no rompe el flujo local `0x51 AUTH_VALIDATE_PIN`;
  - no reemplaza `0x59 AUTH_PIN_GRANTED`;
  - existe para que el bridge ESP/Web pueda disparar las pantallas reales STM de error de permiso.

## 0x5A `BINARY_ASSET_STREAM`

- Direccion normal: MCU -> ESP
- Request payload:

```text
[asset_id]
```

- Response payload por chunks:

```text
[status, asset_id, width, height, total_len0, total_len1, offset0, offset1, data_len, data...]
```

- Uso actual:
  - `asset_id = 1` -> QR del repositorio GitHub;
  - se consume desde `SCREEN_CODE_SETTINGS_ABOUT_REPO`.

## 0x5C `WIFI_CREDENTIALS_WEB_RESULT`

- Direccion normal: ESP -> MCU
- Uso: resultado final de credenciales ingresadas desde la web luego de una solicitud STM con `SET_CREDENTIALS` y `pass = "connRequest"`.
- Request payload:

```text
[status, ssid_len, ssid..., ip0, ip1, ip2, ip3]
```

- Los 4 bytes de IP son obligatorios cuando `status = 0`.
- Para `status = 1`, `status = 2` o `status = 3`, el payload termina despues del SSID y no debe incluir IP.
- `status`:
  - `0` = conexion exitosa con credenciales web
  - `1` = credenciales rechazadas / conexion fallida
  - `2` = timeout de conexion
  - `3` = cancelado desde web
- ACK/response de estado manual STM32:
  - `0` = OK
  - `1` = payload invalido
  - `5` = argumento invalido, por ejemplo `status` desconocido
- Efecto STM32:
  - si `status = 0`, guarda SSID/IP, marca `WIFI_ACTIVE` y muestra `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_SUCCEEDED`;
  - si `status = 1`, limpia `WIFI_ACTIVE`, limpia IP STA y muestra `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_FAILED` con motivo de falla;
  - si `status = 2`, limpia `WIFI_ACTIVE`, limpia IP STA y muestra `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_FAILED` con motivo timeout;
  - si `status = 3`, limpia `WIFI_ACTIVE`, limpia IP STA y muestra `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_FAILED` con texto de cancelacion desde web;
  - en todos los casos validos refresca dashboard/status WiFi si corresponde.

## 0xE0 `ACK`

- Direccion: cualquier nodo que deba acusar recibo
- Payload:

```text
[acked_cmd, status]
```

- En el flujo actual:
  - el ESP lo envia automaticamente para comandos con flag ACK;
  - el STM32 lo usa para salir del estado de espera de validacion.

## 0xE1 `NACK`

- Direccion: cualquier nodo que rechace un comando
- Payload:

```text
[cmd, reason]
```

- `reason` suele representar:
  - comando desconocido
  - argumentos invalidos

## 8. Catalogo completo de eventos

## 0x80 `EVT_BOOT`

- Emisor actual: ESP
- Payload: `wifi_info`
- Cuando se emite:
  - al iniciar `UNER_App_Init(...)`
- Consumo STM32 hoy:
  - muestra notificacion de boot ESP.

## 0x81 `EVT_MODE_CHANGED`

- Emisor actual: ESP
- Payload: `wifi_info`
- Flag ACK en tabla ESP: si
- Cuando se emite:
  - al cambiar `WiFi.getMode()`
  - despues de `SET_MODE_AP`
  - despues de `SET_MODE_STA`
  - en boot si el ESP arranca con AP activa
- Consumo STM32 hoy:
  - toma `payload[0]` como `mode`
  - toma `payload[2..5]` como IP
  - actualiza flags de `WIFI_ACTIVE` y `AP_ACTIVE`
  - muestra notificacion de cambio de modo.

## 0x82 `EVT_STA_CONNECTED`

- Emisor actual: ESP
- Payload: `wifi_info`
- Cuando se emite:
  - cuando `WiFi.status()` pasa a `WL_CONNECTED`
- Consumo STM32 hoy:
  - marca `WIFI_ACTIVE`.

## 0x83 `EVT_STA_DISCONNECTED`

- Emisor actual: ESP
- Payload: `wifi_info`
- Cuando se emite:
  - cuando `WiFi.status()` deja `WL_CONNECTED`
- Consumo STM32 hoy:
  - limpia `WIFI_ACTIVE`.

## 0x84 `EVT_AP_CLIENT_JOIN`

- Emisor actual: ESP
- Payload:

```text
[ap_clients_total]
```

- Cuando se emite:
  - si sube la cantidad de STAs conectadas al SoftAP
  - tambien por `UNER_App_NotifyApClientJoined`
- Consumo STM32 hoy:
  - handler stub, sin logica.

## 0x85 `EVT_AP_CLIENT_LEAVE`

- Emisor actual: ESP
- Payload:

```text
[ap_clients_total]
```

- Consumo STM32 hoy:
  - handler stub.

## 0x86 `EVT_APP_USER_CONNECTED`

- Emisor actual: ESP
- Payload:

```text
[id0, id1, id2, id3]
```

- `id` es little-endian
- Cuando se emite:
  - al conectar un WS
  - cuando se sirve el index HTTP
- Consumo STM32 hoy:
  - handler stub.

## 0x87 `EVT_APP_USER_DISCONNECTED`

- Emisor actual: ESP
- Payload:

```text
[id0, id1, id2, id3]
```

- `id` little-endian
- Cuando se emite:
  - al desconectar un WS
- Consumo STM32 hoy:
  - handler stub.

## 0x88 `EVT_WEBSERVER_UP`

- Emisor actual: ESP
- Payload: vacio
- Cuando se emite:
  - cuando se llama `UNER_App_NotifyWebserverUp(...)`
- Consumo STM32 hoy:
  - muestra notificacion "webserver up".

## 0x89 `EVT_WEBSERVER_CLIENT_CONNECTED`

- Emisor actual: ESP
- Payload:

```text
[id0, id1, id2, id3]
```

- puede ser ID de cliente WS o tag de cliente HTTP
- Consumo STM32 hoy:
  - handler stub.

## 0x8A `EVT_WEBSERVER_CLIENT_DISCONNECTED`

- Emisor actual: ESP
- Payload:

```text
[id0, id1, id2, id3]
```

- Consumo STM32 hoy:
  - handler stub.

## 0x8B `EVT_LASTWIFI_NOTFOUND`

- Figura en la tabla del protocolo
- Flag ACK en tabla ESP: si
- Emision ESP actual:
  - no hay un handler de negocio activo que la emita explicitamente
- Payload:
  - no hay contrato cerrado hoy
- Consumo STM32 hoy:
  - handler stub.

## 0x8C `EVT_CONTROLLER_CONNECTED`

- Figura en la tabla del protocolo
- Emision ESP actual:
  - no hay emision explicita actual
- Payload:
  - no hay contrato cerrado hoy
- Consumo STM32 hoy:
  - marca `RF_ACTIVE`
  - muestra notificacion de control conectado.

## 0x8D `EVT_CONTROLLER_DISCONNECTED`

- Igual criterio que `0x8C`
- Consumo STM32 hoy:
  - limpia `RF_ACTIVE`
  - muestra notificacion de control desconectado.

## 0x8E `EVT_USB_CONNECTED`

- Emisor actual: ESP
- Payload: vacio
- Cuando se emite:
  - en boot
- Consumo STM32 hoy:
  - marca `USB_ACTIVE`
  - muestra notificacion de USB conectado.

## 0x8F `EVT_USB_DISCONNECTED`

- Emisor actual: ESP
- Payload: vacio
- Cuando se emite:
  - justo antes de `ESP.restart()` por `REBOOT_ESP`
- Consumo STM32 hoy:
  - limpia `USB_ACTIVE`
  - muestra notificacion de USB desconectado.

## 0x90 `EVT_APP_GET_MPU_READINGS`

- Figura en la tabla del protocolo
- Flag ACK en tabla ESP: si
- Emision ESP actual:
  - no hay emision funcional hoy
- Payload:
  - no hay contrato definido hoy
- Consumo STM32 hoy:
  - handler stub.

## 0x91 `EVT_APP_GET_TCRT_READINGS`

- Figura en la tabla del protocolo
- Flag ACK en tabla ESP: si
- Emision ESP actual:
  - no hay emision funcional hoy
- Payload:
  - no hay contrato definido hoy
- Consumo STM32 hoy:
  - handler stub.

## 0x92 `EVT_WIFI_CONNECTED`

- Emisor actual: ESP
- Payload:

```text
[1, ssid_len, ssid..., pass_len, pass...]
```

- Es decir, `target_credentials` de STA
- Cuando se emite:
  - cuando STA llega a `WL_CONNECTED`
- Consumo STM32 hoy:
  - marca `WIFI_ACTIVE`
  - no parsea el SSID ni el password.

## 0x93 `EVT_NETWORK_IP`

- Emisor actual: ESP
- Payload:

```text
[net_if, ip0, ip1, ip2, ip3]
```

- `net_if = 0x01` STA
- `net_if = 0x02` AP
- Cuando se emite:
  - al obtener o actualizar IP
- Consumo STM32 hoy:
  - actualiza IP STA/AP segun `net_if`;
  - refresca UI/notificaciones de red cuando aplica.

## 0x94 `EVT_BOOT_COMPLETE`

- Emisor actual: ESP
- Payload:

```text
[net_if, ip0, ip1, ip2, ip3]
```

- Se emite una sola vez por boot logico
- Consumo STM32 hoy:
  - se procesa igual que `EVT_NETWORK_IP`.

## 0x95 `EVT_SCREEN_CHANGED`

- Emisor actual: STM32
- Payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3, source]
```

- `screen_code` viaja little-endian y usa la macro `SCREEN_CODE(menu, submenu, page)`.
- `source` usa `ScreenReportSource_t`.
- Se emite cuando cambia la pantalla visible.
- `GET_CURRENT_SCREEN (0x52)` devuelve el mismo formato de payload como response directa, no como evento.
- Para WiFi, ver tabla de `SCREEN_CODE_CONNECTIVITY_WIFI_*` en la seccion 6.7.

## 0x96 `EVT_MENU_SELECTION_CHANGED`

- Emisor actual: STM32
- Payload:

```text
[screen_code0, screen_code1, screen_code2, screen_code3, selected_index, item_count, source]
```

- `selected_index` es el indice visible seleccionado dentro del menu actual.
- `item_count` es la cantidad de items visibles/navegables reportados.
- En `SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS`, los primeros indices son redes encontradas, luego `Actualizar` y `Volver`.
- Al seleccionar una red, el STM32 cambia a `SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS`.

## 0x97 `EVT_CAR_MODE_CHANGED`

- Emisor actual: STM32
- Payload:

```text
[mode]
```

- `mode`:
  - `0` = `IDLE_MODE`
  - `1` = `FOLLOW_MODE`
  - `2` = `TEST_MODE`
- Se emite cuando el dashboard confirma un cambio de modo local.

## 9. Flujos operativos importantes

### 9.1 Boot del ESP

Secuencia tipica al boot:

1. `EVT_BOOT`
2. `EVT_USB_CONNECTED`
3. si STA ya esta conectada:
   - `EVT_STA_CONNECTED`
   - `EVT_WIFI_CONNECTED`
   - `EVT_NETWORK_IP`
   - `CMD_NETWORK_IP`
   - `EVT_BOOT_COMPLETE`
   - `CMD_BOOT_COMPLETE`
4. si AP esta activa:
   - `EVT_MODE_CHANGED`
   - `EVT_NETWORK_IP`
   - `CMD_NETWORK_IP`
   - `EVT_BOOT_COMPLETE` solo si aun no se envio
   - `CMD_BOOT_COMPLETE` solo si aun no se envio

### 9.2 Scan WiFi

1. MCU entra a `SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING`.
2. MCU limpia resultados locales (`SSID`, encriptacion y seleccion).
3. MCU envia `0x14 START_SCAN`.
4. ESP devuelve ACK y response `[0]`.
5. ESP hace scan asincrono hasta 10 s.
6. al terminar o ser detenido envia `0x15 + [0xFE]`.
7. MCU tambien consulta `0x15 GET_SCAN_RESULTS` periodicamente mientras la busqueda sigue activa, para no depender solo del finalizador ni del redibujado del OLED.
8. ESP responde:
   - `[1,0]` si sigue corriendo
   - `[2,0]` si se detuvo por comando
   - `[0,0]` si no hay redes
   - `[0,count,len,ssid...]` si hay redes
9. con respuesta final, MCU renderiza `SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS` y muestra la notificacion breve "Resultados WiFi".

Acciones locales:

- `Actualizar` desde resultados vuelve a `SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING` y repite desde el paso 2;
- `Volver`, boton usuario o salida global desde busqueda/resultados/detalles limpia los resultados de memoria;
- una respuesta final vieja de un scan ya cancelado se ignora si la sesion local ya no esta activa.

### 9.2.1 Seleccionar red y pedir credenciales por web

1. Usuario selecciona un SSID desde `SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS`.
2. MCU renderiza `SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS`.
3. Boton usuario o encoder long press vuelve a `SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS` sin borrar resultados.
4. Encoder short press en "Conectar" envia:

```text
CMD = 0x12 SET_CREDENTIALS
payload = [1, ssid_len, ssid..., 11, "connRequest"]
```

5. MCU muestra `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_WEB` con "Ingresar credenciales en web".
6. ESP debe emitir a la web un evento de solicitud de credenciales para ese SSID.
7. La web debe devolver al ESP el SSID y password ingresados.
8. ESP guarda esas credenciales reales, intenta conectar y emite el estado final:
   - hacia web: evento de resultado con `success`/`failed`/`timeout`/`canceled`;
   - hacia STM32: `0x5C WIFI_CREDENTIALS_WEB_RESULT`.
9. STM32 muestra `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_SUCCEEDED` o `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_FAILED`.
10. Al cerrar/restaurar la notificacion inicial, queda de nuevo en detalles para reintentar si hace falta.

El ESP debe tratar `"connRequest"` como solicitud de captura de credenciales desde la interfaz web, no como password WiFi definitiva.

### 9.2.2 Contrato ESP-Web para credenciales

Cuando ESP recibe `SET_CREDENTIALS` con `pass = "connRequest"`:

1. No debe persistir `"connRequest"` ni intentar conectar con esa password.
2. Debe guardar una solicitud pendiente con el SSID.
3. Debe emitir a la web un evento, siguiendo el estilo JSON actual del bridge:

```json
{
  "type": "device.event",
  "event": "wifi.credentials.requested",
  "ssid": "NombreRed"
}
```

La web debe responder con un comando JSON al ESP:

```json
{
  "type": "device.command",
  "command": "wifi.credentials.submit",
  "ssid": "NombreRed",
  "password": "clave_ingresada"
}
```

La web tambien puede cancelar el modal con:

```json
{
  "type": "device.command",
  "command": "wifi.credentials.cancel",
  "ssid": "NombreRed"
}
```

El ESP debe validar que el `ssid` coincida con la solicitud pendiente, guardar las credenciales reales, intentar `WiFi.begin(ssid, password)` y luego emitir:

```json
{
  "type": "device.event",
  "event": "wifi.credentials.result",
  "ssid": "NombreRed",
  "status": "success",
  "ip": "192.168.1.40"
}
```

o, ante error:

```json
{
  "type": "device.event",
  "event": "wifi.credentials.result",
  "ssid": "NombreRed",
  "status": "failed",
  "reason": "auth_failed"
}
```

Ademas, el ESP debe notificar a STM32 con `0x5C WIFI_CREDENTIALS_WEB_RESULT`:

- exito: `[0, ssid_len, ssid..., ip0, ip1, ip2, ip3]`
- falla: `[1, ssid_len, ssid...]`
- timeout: `[2, ssid_len, ssid...]`
- cancelado: `[3, ssid_len, ssid...]`

La cancelacion desde web nace solo en Web -> ESP con `wifi.credentials.cancel`; STM32 no envia un comando de cancelacion. STM32 se entera unicamente por `0x5C` con `status = 3`.

### 9.3 Conectar WiFi STA

1. MCU envia `0x48 CONNECT_WIFI`
2. ESP devuelve ACK y response `[0,wifi_info...]`
3. cuando STA conecta, el ESP envia:
   - `EVT_STA_CONNECTED`
   - `EVT_WIFI_CONNECTED`
   - `EVT_NETWORK_IP`
   - `CMD_NETWORK_IP`
   - `0x48 + [0xFE, ip...]`
   - `EVT_BOOT_COMPLETE` y `CMD_BOOT_COMPLETE` solo la primera vez del boot

### 9.4 Levantar AP

1. MCU envia `0x4B START_AP`
2. ESP devuelve ACK y response `[status,wifi_info...]`
3. cuando el modo AP queda activo, el ESP envia:
   - `EVT_MODE_CHANGED`
   - `EVT_NETWORK_IP`
   - `CMD_NETWORK_IP`
   - `0x4B + [0xFE, ip...]`
   - `EVT_BOOT_COMPLETE` y `CMD_BOOT_COMPLETE` solo la primera vez del boot

## 10. Lo que el STM32 consume hoy y lo que hoy ignora

### 10.1 Consumido funcionalmente hoy

El STM32 hoy parsea y usa de verdad:

- `REQUEST_FIRMWARE`
- `GET_STATUS`
- `GET_SCAN_RESULTS`
- finalizador raw de `CONNECT_WIFI`
- finalizador raw de `START_AP`
- finalizador raw de scan
- `RESET_MCU`
- `AUTH_VALIDATE_PIN`
- `AUTH_PIN_GRANTED`
- `AUTH_REMOTE_RESULT`
- `EVT_MODE_CHANGED`
- `EVT_STA_CONNECTED`
- `EVT_STA_DISCONNECTED`
- `EVT_WEBSERVER_UP`
- `EVT_CONTROLLER_CONNECTED`
- `EVT_CONTROLLER_DISCONNECTED`
- `EVT_USB_CONNECTED`
- `EVT_USB_DISCONNECTED`
- `EVT_WIFI_CONNECTED`
- `CMD_BOOT_COMPLETE`
- `CMD_NETWORK_IP`
- `EVT_NETWORK_IP`
- `EVT_BOOT_COMPLETE`

### 10.2 Definido pero stub del lado STM32

Hoy el STM32 recibe pero no explota funcionalmente:

- `EVT_AP_CLIENT_JOIN`
- `EVT_AP_CLIENT_LEAVE`
- `EVT_APP_USER_CONNECTED`
- `EVT_APP_USER_DISCONNECTED`
- `EVT_WEBSERVER_CLIENT_CONNECTED`
- `EVT_WEBSERVER_CLIENT_DISCONNECTED`
- `EVT_LASTWIFI_NOTFOUND`
- `EVT_APP_GET_MPU_READINGS`
- `EVT_APP_GET_TCRT_READINGS`

### 10.3 Mensajes del ESP que hoy el STM32 ignora por no existir en su tabla

Actualmente no hay mensajes de conectividad ESP documentados en esta guia que el STM32 ignore por falta de entrada en `uner_commands`. Los mensajes legacy `0x4F`, `0x50`, `0x93` y `0x94` ya estan contemplados.

## 11. Funcionalidades del auto que existen hoy en STM32 pero NO tienen contrato UNER cerrado

Esta seccion es importante para el programador del ESP. Estas funciones existen en el auto, pero hoy no tienen un comando/evento UNER formal y estable entre STM32 y ESP.

## 11.1 Cambio de modo del auto

El auto tiene `CarMode_t`:

- `0 = IDLE_MODE`
- `1 = FOLLOW_MODE`
- `2 = TEST_MODE`

Comportamiento actual:

- se cambia localmente con el encoder desde dashboard;
- la primera rotacion entra en modo "modificando";
- rotaciones siguientes recorren el enum;
- pulsacion corta confirma;
- user button o ciertas salidas cancelan.

Estado actual respecto a ESP:

- no hay `CMD` UNER para setear `CarMode_t`;
- si hay `EVT_CAR_MODE_CHANGED (0x97)` para anunciar el modo confirmado localmente;
- si el ESP necesita comandar este estado remotamente, primero hay que definir un `CMD` nuevo y su politica de permisos.

## 11.2 Motores

El STM32 ya tiene control local de motores:

- seleccion de motor:
  - `0` = izquierdo
  - `1` = derecho
  - `2` = ambos
- direccion:
  - `0` = forward
  - `1` = backward
- velocidad: `uint8_t`
- flag `ENABLE_MOVEMENT`

Semantica local actual en pantalla de test:

- girar CW -> suma `+10`
- girar CCW -> resta `-10`
- short press -> invierte direccion del motor seleccionado
- encoder long press -> rota seleccion `izq -> der -> ambos`
- user button -> enable/disable movimiento

Estado actual respecto a ESP:

- no hay `CMD` UNER vigente para controlar motores;
- no hay `EVT` UNER vigente para informar estado de motor;
- cualquier API ESP sobre motores hoy seria inventada si no se acuerda primero el contrato.

## 11.3 Sensores IR / TCRT5000

El STM32 ya tiene stack local de 8 canales TCRT por ADC DMA.

Mapeo actual de canales:

- canal 0 = linea central
- canal 1 = linea lateral izquierda
- canal 2 = linea lateral derecha
- canal 3 = obstaculo diagonal izquierda
- canal 4 = obstaculo frente izquierda
- canal 5 = obstaculo centro
- canal 6 = obstaculo frente derecha
- canal 7 = obstaculo diagonal derecha

Notas locales:

- `sensor_raw_data[]` tiene 8 valores crudos
- rango esperado de ADC: `0..4095`
- hay calibracion local por fases
- se generan flags locales como `too_left`, `too_right`, `obs_center`, `light_on`

Estado actual respecto a ESP:

- existe `EVT_APP_GET_TCRT_READINGS (0x91)` en la tabla, pero sin payload formal ni emision real;
- el ESP actual no recibe una telemetria TCRT funcional desde STM32;
- si se quiere una libreria ESP completa para IR/TCRT, primero hay que definir exactamente:
  - tipo de mensaje;
  - frecuencia;
  - unidades;
  - orden de canales;
  - si viajan crudos, flags o ambos.

## 11.4 MPU6050

El STM32 ya tiene stack local para MPU6050 en I2C direccion `0x68`.

Unidades locales actuales:

- aceleracion: `mg`
- temperatura: `celsius x100`
- gyro: `mdps`
- angulos: `mdeg`

Estructura local relevante:

- `accel_x_mg`
- `accel_y_mg`
- `accel_z_mg`
- `temperature_celsius_x100`
- `gyro_x_mdps`
- `gyro_y_mdps`
- `gyro_z_mdps`
- `angle_x_md`
- `angle_y_md`
- `angle_z_md`

Estado actual respecto a ESP:

- existe `EVT_APP_GET_MPU_READINGS (0x90)` en la tabla, pero sin payload formal ni emision real;
- no hay hoy un contrato UNER que transporte estas lecturas al ESP;
- si el ESP necesita exponer MPU por WebSocket o app, primero hay que acordar el payload exacto.

## 11.5 WebSocket de sensores actual del ESP

El ESP actual ya envia por WS texto JSON cada 3 segundos, pero hoy ese canal NO representa datos del STM32.

El contenido actual es dummy:

```json
{"temperature":25.1,"humidity":45.0,"pressure":1013.2}
```

Eso implica:

- no viene del MPU del STM32;
- no viene de los TCRT/IR del STM32;
- no viene del estado de motores;
- no debe tomarse como contrato de telemetria del auto.

## 12. Mismatches y advertencias de compatibilidad

## 12.1 Nodo logico del ESP

Aunque semanticamente sea "ESP", en wire hoy el ESP debe seguir usando `0x02`.

## 12.2 Finalizadores raw

No agregar `status` delante de estos mensajes:

- `0x15 + [0xFE]`
- `0x48 + [0xFE, ip...]`
- `0x4B + [0xFE, ip...]`

El STM32 actual depende de eso.

## 12.3 `0x4F/0x50/0x93/0x94`

El ESP actual puede emitirlos y el STM32 actual los consume para refrescar IP/estado de red. Mantenerlos con payload `[net_if, ip0, ip1, ip2, ip3]` para no romper compatibilidad.

## 12.4 Eventos marcados con ACK

La tabla del ESP marca algunos eventos con flag ACK:

- `EVT_MODE_CHANGED`
- `EVT_LASTWIFI_NOTFOUND`
- `EVT_APP_GET_MPU_READINGS`
- `EVT_APP_GET_TCRT_READINGS`

Sin embargo, el `UNER_Handle` del STM32 actual no implementa auto-ACK para frames entrantes como lo hace el ESP. Por lo tanto:

- no confiar en ACK del MCU como requisito de entrega para estos eventos;
- tratar esos flags como parte de la tabla compartida, no como garantia de handshake real hoy.

## 12.5 `SET_ENCODER_FAST`

El contrato correcto del ESP es `[status, accepted]`. No introducir hacks del lado ESP para compensar el parser duplicado del STM32.

## 12.6 Funciones del auto fuera del protocolo

Car mode, motores, MPU y TCRT existen en STM32, pero hoy no tienen API UNER cerrada. Si el senior del ESP necesita cubrirlas en una libreria completa, primero hay que agregar una especificacion nueva y versionada del protocolo.

## 13. Checklist de implementacion para la libreria ESP

- Respetar el frame UNER v2 exacto: `UNER + LEN + 0x3A + 0x02 + ROUTE + CMD + PAYLOAD + XOR`.
- Mantener `this_node = 0x02`, `peer_node = 0x01`.
- Implementar parser de stream binario, no parser por lineas.
- Soportar payloads de hasta `255` bytes.
- Mantener `SSID_MAX_LEN = 32` y `PASS_MAX_LEN = 63`.
- Para scan, usar finalizador raw `0x15 + [0xFE]` y luego response length-prefixed.
- Para `CONNECT_WIFI` y `START_AP`, emitir finalizador raw `[0xFE, ip0..3]`.
- Para resetear la STM32 desde el ESP, enviar `CMD=0x19` con payload vacio, esperar el ACK `[0x19, 0x00]` con timeout corto, y no esperar response normal.
- Mantener `GET_STATUS` con flags rapidos + `wifi_info` + `sta_ip` + `ap_ip`.
- Mantener `GET_USER_INFO` con `id` little-endian y `ip` de 4 bytes.
- No usar `UNERUtils/uner_device` como base de compatibilidad con este STM32.
- No inventar comandos para car mode, motores, MPU o TCRT sin cerrar primero la especificacion.

## 14. Resumen ejecutivo

Hoy el contrato estable y realmente interoperable STM32 <-> ESP cubre:

- modo WiFi;
- credenciales;
- scan de redes;
- estado de conectividad;
- firmware/version;
- clientes WS/AP;
- eventos de boot y conectividad.

Hoy NO existe un contrato UNER funcional cerrado para:

- cambio de modo del auto (`IDLE/FOLLOW/TEST`);
- control de motores;
- telemetria MPU;
- telemetria TCRT/IR.

Si la libreria del ESP debe ser "completa" respecto del auto entero, no alcanza con implementar UNER actual. Tambien hace falta definir una extension formal del protocolo para esas areas.
