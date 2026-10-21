# UNER v2 command contract

Este documento describe el contrato binario real que usa el STM32 para comunicarse por UNER v2 con ESP, Web y Qt. La fuente de verdad de esta version es `Core/Inc/uner_v2.h`, `Core/Inc/uner_app.h`, `Core/Src/uner_core.c`, `Core/Src/uner_handle.c` y `Core/Src/uner_app.c`.

## Frame binario

Formato de frame:

| Offset | Campo | Tamano | Valor |
|---:|---|---:|---|
| 0 | Header[0] | 1 | ASCII `U` |
| 1 | Header[1] | 1 | ASCII `N` |
| 2 | Header[2] | 1 | ASCII `E` |
| 3 | Header[3] | 1 | ASCII `R` |
| 4 | LEN | 1 | cantidad de bytes de payload |
| 5 | TOKEN | 1 | `0x3A` (`:`) |
| 6 | VER | 1 | `0x02` |
| 7 | ROUTE | 1 | `(src << 4) | dst` |
| 8 | CMD | 1 | comando o evento |
| 9.. | PAYLOAD | LEN | binario, little-endian donde aplique |
| 9+LEN | CHK | 1 | XOR de todos los bytes anteriores |

El overhead fijo es 10 bytes. El parser acepta `dst=MCU` y `dst=BROADCAST`; valida header, token, version, longitud maxima y checksum. El payload maximo por transporte se limita por `UNER_Transport.max_payload`.

Nodos:

| Nodo | ID | Uso |
|---|---:|---|
| MCU | `0x1` | STM32 |
| PC_QT | `0x2` | aplicacion Qt por USB o bridge futuro |
| WEB_APP | `0x3` | Web UI |
| REMOTE_NRF | `0x4` | control remoto NRF |
| BROADCAST | `0xF` | eventos para todos |

Transportes:

| Transporte | ID | Estado actual |
|---|---:|---|
| UART1_ESP | `0` | activo, DMA circular RX y DMA TX |
| NRF24_SPI2 | `1` | registrado en el protocolo, no central en este flujo |
| USB_CDC | `2` | registrado como opcion futura; el transporte actual es stub y no envia todavia |

## Estados comunes de respuesta

Cuando un handler STM responde con `payload[0] = status`, se usa este significado local:

| Status | Nombre | Significado |
|---:|---|---|
| `0` | OK | comando aceptado o ejecutado |
| `1` | BAD_PAYLOAD | longitud o formato invalido |
| `2` | SCREEN_MISMATCH | el screen code recibido no coincide con la pantalla actual |
| `3` | PIN_NOT_REQUIRED | el screen actual no es flujo de PIN |
| `4` | NO_PENDING_PERMISSION / BUSY_OWNER | no hay permiso pendiente, o el recurso pertenece a otro requester |
| `5` | BAD_ARGUMENT / UNSUPPORTED_TRANSPORT | argumento invalido o transporte no soportado para la operacion |

Los comandos de I2C usan estados compatibles pero especificos: `0=OK`, `1=payload invalido`, `2=modo invalido`, `3=manager no inicializado/error interno`.

## ACK, response, evento y finalizador

La tabla de comandos declara flags:

| Flag | Significado |
|---|---|
| ACK | el flujo espera ACK/NACK del peer o un comando de confirmacion |
| RESP | el flujo espera respuesta con el mismo CMD y `payload[0] = status` cuando aplique |
| EVT | el frame representa evento asincronico; normalmente se envia a broadcast |

Hay finalizadores raw aceptados aunque no cumplan longitud nominal: `CMD=0x15`, `CMD=0x48` y `CMD=0x4B` con `payload[0]=0xFE`. Esto se usa para operaciones asincronicas de scan, connect y AP start.

Pseudo frame de ejemplo, Web -> MCU, consultar pantalla actual:

```text
55 4E 45 52 00 3A 02 31 52 CHK
Header       len token ver route cmd chk
route 0x31 = src WEB_APP(3), dst MCU(1)
```

Pseudo frame, Web -> MCU, iniciar streaming MPU cada 100 ms:

```text
CMD 0x61 payload: 01 64 00
enable=1, period_ms=0x0064
```

## Tabla completa de comandos y eventos

| ID | Nombre | Dir principal | Req | Resp/evento | Flags | Descripcion real |
|---:|---|---|---|---|---|---|
| `0x10` | SET_MODE_AP | STM -> ESP | vacio | status por mismo CMD, eventos de modo/AP | ACK, RESP | pide al ESP cambiar a modo AP |
| `0x11` | SET_MODE_STA | STM -> ESP | vacio | status por mismo CMD, eventos de modo/STA | ACK, RESP | pide al ESP cambiar a modo STA |
| `0x12` | SET_CREDENTIALS | STM/Web -> ESP | `[ssid_len, pass_len, connect_flag, ssid..., pass...]` en flujo STM; otros payloads hasta 255 | status por mismo CMD | ACK, RESP | guarda credenciales y opcionalmente dispara conexion |
| `0x13` | CLEAR_CREDENTIALS | STM -> ESP | vacio | status | ACK, RESP | borra credenciales persistidas |
| `0x14` | START_SCAN | STM -> ESP | vacio | status y luego 0x15 | ACK, RESP | inicia scan WiFi asincronico |
| `0x15` | GET_SCAN_RESULTS | STM -> ESP / ESP -> STM | vacio | `[status,count, ssid_len,ssid...]` o raw `[FE,...]` | RESP | consulta resultados; `status=1` significa scan en progreso |
| `0x16` | REBOOT_ESP | STM -> ESP | `[boot_mode]` | ACK y eventos boot | ACK | reinicia ESP; `boot_mode=0 normal`, `1 AP` |
| `0x17` | FACTORY_RESET | STM -> ESP | vacio | status | ACK, RESP | reset de fabrica del ESP |
| `0x18` | STOP_SCAN | STM -> ESP | vacio | status | ACK, RESP | cancela scan WiFi |
| `0x19` | RESET_MCU | ESP/Web/Qt -> STM | vacio | ACK manual si llega por UART1_ESP, luego NVIC reset | none | reinicia STM |
| `0x1A` | WIFI_GET_DETAIL | STM -> ESP | `[ssid_len, ssid...]` | `[status, enc_type, rssi, channel]` o equivalente | RESP | pide detalle de red listada |
| `0x30` | GET_STATUS | STM -> ESP | vacio | status WiFi/IP/mode | RESP | consulta estado general del ESP |
| `0x31` | PING | bidireccional | vacio | status/eco segun peer | RESP | test de vida; STM muestra notificacion PING recibido |
| `0x40` | GET_PREFERENCES | STM -> ESP | vacio | preferencias | ACK, RESP | lee preferencias remotas |
| `0x41` | REQUEST_FIRMWARE | STM -> ESP | vacio o args peer | `[status, ascii_version...]` | ACK, RESP | consulta version de firmware ESP |
| `0x42` | ECHO | bidireccional | 4 bytes | eco | none | prueba integridad; STM notifica ECHO recibido |
| `0x43` | SET_ENCODER_FAST | peer -> STM | `[0/1]` | status por peer | ACK, RESP | activa/desactiva scroll rapido del encoder |
| `0x44` | GET_CONNECTED_USERS | STM -> ESP | vacio | lista de usuarios | RESP | consulta usuarios conectados |
| `0x45` | GET_USER_INFO | STM -> ESP | `[user_index]` | info usuario | RESP | consulta un usuario |
| `0x46` | GET_INTERFACES_CONNECTED | STM -> ESP | vacio | mascara o lista | RESP | consulta interfaces activas |
| `0x47` | GET_CREDENTIALS | STM -> ESP | vacio | credenciales/metadatos | RESP | consulta credenciales guardadas |
| `0x48` | CONNECT_WIFI | STM -> ESP | vacio | status y raw finalizador `[FE,...]` | ACK, RESP | conecta usando credenciales actuales |
| `0x49` | DISCONNECT_WIFI | STM -> ESP | vacio | status | ACK, RESP | desconecta STA |
| `0x4A` | SET_AP_CONFIG | STM -> ESP | 2..96 bytes | status | ACK, RESP | configura AP |
| `0x4B` | START_AP | STM -> ESP | vacio | status y raw finalizador `[FE,...]` | ACK, RESP | inicia AP |
| `0x4C` | STOP_AP | STM -> ESP | vacio | status | ACK, RESP | detiene AP |
| `0x4D` | GET_CONNECTED_USERS_MODE | STM -> ESP | vacio | modo/lista | RESP | consulta usuarios en modo actual |
| `0x4E` | SET_AUTO_RECONNECT | STM -> ESP | `[0/1]` | status | ACK, RESP | configura reconexion automatica |
| `0x4F` | BOOT_COMPLETE | ESP -> STM | 5 bytes | evento interno | EVT | informa boot completo del ESP |
| `0x50` | NETWORK_IP | ESP -> STM | `[mode, ip0, ip1, ip2, ip3]` | evento interno | EVT | informa IP STA/AP |
| `0x51` | AUTH_VALIDATE_PIN | STM -> ESP | `[request_id, permission, d0, d1, d2, d3]` | `[status, request_id, permission, granted, ttl32?, attempts_left?]` | RESP | valida PIN en ESP/Web y vuelve al manager de permisos |
| `0x52` | GET_CURRENT_SCREEN | Web/Qt -> STM | vacio | `[screen_code_le32, source]` | RESP | consulta pantalla actual |
| `0x53` | MENU_ITEM_CLICK | Web/Qt -> STM | `[screen_code_le32, visible_item_1_based]` | `[status]` | ACK, RESP | seleccion directa de item visible; reutiliza `MenuSys_HandleClick` |
| `0x54` | TRIGGER_ENCODER_BUTTON | Web/Qt -> STM | `[screen_code_le32, press_kind]` | `[status]` | ACK, RESP | simula encoder press; `press_kind 0=short, 1=long` |
| `0x55` | TRIGGER_USER_BUTTON | Web/Qt -> STM | `[screen_code_le32, press_kind]` | `[status]` | ACK, RESP | simula boton fisico; `press_kind 0=short, 1=long` |
| `0x56` | REQUEST_SCREEN_PAGE | Web/Qt -> STM | `[screen_code_le32, dir]` | `[status]` | ACK, RESP | desplaza pagina de menu por `MENU_VISIBLE_ITEMS`; `dir 0=up, 1=down` |
| `0x57` | TRIGGER_ENCODER_ROTATE_LEFT | Web/Qt -> STM | `[screen_code_le32]` | `[status]` | ACK, RESP | simula encoder CCW |
| `0x58` | TRIGGER_ENCODER_ROTATE_RIGHT | Web/Qt -> STM | `[screen_code_le32]` | `[status]` | ACK, RESP | simula encoder CW |
| `0x59` | AUTH_PIN_GRANTED | Web/Qt -> STM | `[screen_code_le32]` | `[status]` | ACK, RESP | concede la accion pendiente si el screen actual es PIN |
| `0x5A` | BINARY_ASSET_STREAM | STM <-> ESP | STM: `[asset_id]`; ESP: chunks | `[status,...chunk]` | ACK, RESP | baja assets grandes, por ejemplo QR, sin ocupar flash STM |
| `0x5B` | GET_CAR_MODE | Web/Qt -> STM | vacio | `[mode]` | RESP | consulta modo del auto |
| `0x5C` | WIFI_CREDENTIALS_WEB_RESULT | Web/ESP -> STM | 2..38 bytes | `[status]` | ACK, RESP | informa resultado de carga de credenciales por Web |
| `0x5D` | AUTH_REMOTE_RESULT | Web/Qt -> STM | `[screen_code_le32, result_code, attempts_left]` | `[status]` | ACK, RESP | informa denegado/timeout/bloqueado sin validar localmente |
| `0x5E` | SET_I2C_POLICY | Web/Qt -> STM | `[mode]` | `[status,current_mode]` | RESP | cambia politica del I2C Manager |
| `0x5F` | GET_I2C_POLICY | Web/Qt -> STM | vacio | `[status,current_mode]` | RESP | consulta politica del I2C Manager |
| `0x60` | GET_MPU_SNAPSHOT | Web/Qt -> STM | vacio | 42 bytes | RESP | lee snapshot MPU instantaneo |
| `0x61` | SET_MPU_STREAM | Web/Qt -> STM | `[enable]` o `[enable,period_l,period_h]` | `[status,active,period_l,period_h,transport_id,dst_node]` | RESP | inicia/detiene streaming MPU para el requester |
| `0x62` | STOP_MPU_STREAM | Web/Qt -> STM | vacio | `[status,active]` | RESP | detiene streaming MPU del requester |
| `0xE0` | ACK | peer -> STM | 1..2 bytes | none | none | confirma comando; `payload[0]=acked_cmd` |
| `0xE1` | NACK | peer -> STM | 1..2 bytes | none | none | rechaza comando |

Eventos:

| ID | Nombre | Dir | Payload | Uso |
|---:|---|---|---|---|
| `0x80` | EVT_BOOT | ESP -> STM | 0..255 | ESP arranco |
| `0x81` | EVT_MODE_CHANGED | ESP -> STM | 0..255 | cambio modo WiFi |
| `0x82` | EVT_STA_CONNECTED | ESP -> STM | 0..255 | STA conectada |
| `0x83` | EVT_STA_DISCONNECTED | ESP -> STM | 0..255 | STA desconectada |
| `0x84` | EVT_AP_CLIENT_JOIN | ESP -> STM | 0..255 | cliente AP entro |
| `0x85` | EVT_AP_CLIENT_LEAVE | ESP -> STM | 0..255 | cliente AP salio |
| `0x86` | EVT_APP_USER_CONNECTED | ESP/Web -> STM | 0..255 | usuario app conectado |
| `0x87` | EVT_APP_USER_DISCONNECTED | ESP/Web -> STM | 0..255 | usuario app desconectado |
| `0x88` | EVT_WEBSERVER_UP | ESP -> STM | 0..255 | servidor web listo |
| `0x89` | EVT_WEBSERVER_CLIENT_CONNECTED | ESP -> STM | 0..255 | cliente web conectado |
| `0x8A` | EVT_WEBSERVER_CLIENT_DISCONNECTED | ESP -> STM | 0..255 | cliente web desconectado |
| `0x8B` | EVT_LASTWIFI_NOTFOUND | ESP -> STM | 0..255 | ultima WiFi no encontrada |
| `0x8C` | EVT_CONTROLLER_CONNECTED | NRF/ESP -> STM | 0..255 | control conectado |
| `0x8D` | EVT_CONTROLLER_DISCONNECTED | NRF/ESP -> STM | 0..255 | control desconectado |
| `0x8E` | EVT_USB_CONNECTED | USB -> STM | 0..255 | USB conectado |
| `0x8F` | EVT_USB_DISCONNECTED | USB -> STM | 0..255 | USB desconectado |
| `0x90` | EVT_APP_GET_MPU_READINGS | STM -> requester | snapshot de 42 bytes | push de streaming MPU |
| `0x91` | EVT_APP_GET_TCRT_READINGS | STM/peer | 0..255 | reservado para IR/TCRT |
| `0x92` | EVT_WIFI_CONNECTED | ESP -> STM | 0..255 | WiFi conectada |
| `0x93` | EVT_NETWORK_IP | ESP -> STM | 5 bytes | IP de red |
| `0x94` | EVT_BOOT_COMPLETE | ESP -> STM | 5 bytes | boot completo |
| `0x95` | EVT_SCREEN_CHANGED | STM -> broadcast | `[screen_code_le32, source]` | sincroniza Web/Qt con pantalla actual |
| `0x96` | EVT_MENU_SELECTION_CHANGED | STM -> broadcast | `[screen_code_le32, selected, count, source]` | sincroniza seleccion visible |
| `0x97` | EVT_CAR_MODE_CHANGED | STM -> broadcast | `[mode]` | modo del auto cambiado |

## MPU snapshot y streaming

`GET_MPU_SNAPSHOT (0x60)` y `EVT_APP_GET_MPU_READINGS (0x90)` usan el mismo payload de 42 bytes:

| Offset | Campo | Tipo | Unidad |
|---:|---|---|---|
| 0 | status | u8 | `0=OK`, `3=sin muestra valida` |
| 1 | flags | u8 | flags del MPU |
| 2 | sample_seq | u16 LE | contador de muestra |
| 4 | roll | i32 LE | milideg |
| 8 | pitch | i32 LE | milideg |
| 12 | yaw | i32 LE | milideg |
| 16 | accel_x | i16 LE | mg |
| 18 | accel_y | i16 LE | mg |
| 20 | accel_z | i16 LE | mg |
| 22 | linear_accel_x | i16 LE | mg |
| 24 | linear_accel_y | i16 LE | mg |
| 26 | linear_accel_z | i16 LE | mg |
| 28 | gyro_x | i32 LE | mdps |
| 32 | gyro_y | i32 LE | mdps |
| 36 | gyro_z | i32 LE | mdps |
| 40 | sample_dt_us | u16 LE | microsegundos |

Flags MPU:

| Bit | Mascara | Significado |
|---:|---:|---|
| 0 | `0x01` | `DATA_READY` interno |
| 1 | `0x02` | `TX_SENT` interno |
| 2 | `0x04` | `CALIBRATED` |
| 3 | `0x08` | `MPU_NEW_READING` |
| 4 | `0x10` | `MPU_VALID` |
| 5 | `0x20` | `MPU_ERROR` |
| 6 | `0x40` | `MPU_STATIONARY` |
| 7 | `0x80` | `MPU_CALIBRATING` |

Streaming:

1. Web/Qt envia `0x61` con `[1, period_l, period_h]`.
2. STM guarda `transport_id` y `src` del requester.
3. STM responde `[status,active,period_l,period_h,transport_id,dst_node]`.
4. STM emite notificacion OLED `SCREEN_CODE_SENSORS_MPU_STREAM_STARTED`.
5. `UNER_App_Poll()` llama `UNER_App_ServiceMpuStream()`.
6. Si hay muestra nueva (`sample_seq` distinto), STM envia `0x90` al mismo requester.
7. Web/Qt detiene con `0x62` o con `0x61 [0]`.
8. STM emite notificacion OLED `SCREEN_CODE_SENSORS_MPU_STREAM_STOPPED`.

Reglas de transporte:

| Backend firmware | Web por ESP/UART | Qt por USB |
|---|---|---|
| `WIFI_BRIDGE_USE_UNER` | soportado | reservado, requiere USB CDC real |
| `WIFI_BRIDGE_USE_ESP_AT` | no soportado para UNER web por UART1, porque UART1 habla AT | reservado, requiere USB CDC real |

## I2C policy

`SET_I2C_POLICY (0x5E)`:

| Byte | Campo | Valores |
|---:|---|---|
| 0 | mode | `1=Balanced`, `2=PrioritizeMPU`, `3=PrioritizeOLED` |

Respuesta: `[status,current_mode]`.

`GET_I2C_POLICY (0x5F)` no tiene payload y responde `[status,current_mode]`.

## Inconsistencias detectadas

| Area | Hallazgo | Impacto |
|---|---|---|
| IDs de I2C policy | La propuesta inicial usaba `0x51/0x52`, pero esos IDs ya existen como `AUTH_VALIDATE_PIN` y `GET_CURRENT_SCREEN`. Se usaron `0x5E/0x5F`. | evita romper compatibilidad binaria |
| USB CDC | El transporte USB CDC se registra, pero `Core/Src/uner_transport_usbcdc.c` aun es stub. | Qt por USB esta preparado a nivel contrato, no funcional aun |
| ACK generico | La tabla declara flags ACK/RESP, pero no hay generacion automatica universal de ACK en `UNER_Handle`. | cada peer/handler debe cumplir el contrato |
| ESP_AT | En backend ESP_AT, UART1 no puede transportar UNER Web simultaneamente. | Web por ESP solo debe usar streaming MPU si el build es UNER |
