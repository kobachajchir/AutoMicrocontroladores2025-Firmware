# Protocolo UNER v2 en STM32 (compat ESP01)

Este documento describe **la implementación STM32** para interoperar con el ESP01 usando UNER v2.

## 1) Compatibilidad de frame
STM32 usa el mismo frame:
`'U' 'N' 'E' 'R' LEN TOKEN(0x3A) VER(0x02) ROUTE CMD PAYLOAD CHK(XOR)`.

- `ROUTE`: nibble alto `src`, nibble bajo `dst`.
- Nodos compatibles: MCU `0x1`, PC `0x2`, WEB `0x3`, NRF `0x4`, broadcast `0xF`.

## 2) Comandos soportados desde STM32 → ESP01
Se contemplan comandos de WiFi/estado y diagnóstico:

- `0x10 SET_MODE_AP`
- `0x11 SET_MODE_STA`
- `0x12 SET_CREDENTIALS`
- `0x13 CLEAR_CREDENTIALS`
- `0x14 START_SCAN`
- `0x15 GET_SCAN_RESULTS`
- `0x16 REBOOT_ESP`
- `0x17 FACTORY_RESET`
- `0x18 STOP_SCAN`
- `0x30 GET_STATUS`
- `0x31 PING`
- `0x40 GET_PREFERENCES`
- `0x41 REQUEST_FIRMWARE`
- `0x42 ECHO`
- `0x43 SET_ENCODER_FAST`
- `0x44 GET_CONNECTED_USERS`
- `0x45 GET_USER_INFO`
- `0x46 GET_INTERFACES_CONNECTED`
- `0x47 GET_CREDENTIALS`
- `0x48 CONNECT_WIFI`
- `0x49 DISCONNECT_WIFI`
- `0x4A SET_AP_CONFIG`
- `0x4B START_AP`
- `0x4C STOP_AP`
- `0x4D GET_CONNECTED_USERS_MODE`
- `0x4E SET_AUTO_RECONNECT`
- `0x4F BOOT_COMPLETE` (push desde ESP)
- `0x50 NETWORK_IP` (push desde ESP)

## 3) Scheduler por flags (bitmask)
Para evitar perder comandos simultáneos, STM32 no usa un único pending: usa bitmap de 32 bits.

- `uner_cmd_flags_pending`: comandos pendientes.
- `uner_cmd_flags_inflight`: comando en vuelo (hasta ACK/RESP).
- `uner_cmd_last_sent`: último comando enviado.
- `uner_snapshots[]`: snapshot del payload por comando.

### API expuesta
- `UNER_App_SendCommand(cmd,payload,len)` → encola por flag.
- `UNER_App_CmdScheduler_TrySendNext()` → envía siguiente por prioridad.
- `UNER_App_GetPendingFlags()` / `UNER_App_GetInFlightFlags()`.
- `UNER_App_GetFlagBitmap(...)` → exporta low/high byte en `Byte_Flag_Struct`.

### Prioridad aplicada
1. Conectividad (`CONNECT_WIFI`, `DISCONNECT_WIFI`, `START_AP`, `STOP_AP`).
2. Control (`SET_MODE_*`, `START/STOP/GET_SCAN`, `GET_STATUS`).
3. Diagnóstico/config (`GET_*`, `SET_*`, `REQUEST_FIRMWARE`, `ECHO`, etc.).

## 4) Confirmaciones y asíncronos
STM32 procesa:

- `ACK/NACK` (`0xE0/0xE1`): limpia wait/inflight del comando confirmado.
- `response CMD igual al request`: status en `payload[0]` + data.
- push asíncronos especiales:
  - `0x15 [0xFE]` fin de scan,
  - `0x48 [0xFE,ip0..ip3]` conexión STA finalizada,
  - `0x4B [0xFE,ip0..ip3]` AP listo.

## 5) Eventos UNER (`0x80..0x94`)
STM32 recibe y actualiza estado local (flags de sistema) para:
- modo WiFi,
- STA conectada/desconectada,
- clientes AP,
- webserver/app user,
- USB/RF,
- WiFi connected credentials event (`0x92`).
- network ip event (`0x93`) y boot complete (`0x94`) con payload `[if, ip0, ip1, ip2, ip3]`.

## 6) Notificaciones a usuario
La implementación muestra respuestas/eventos en OLED con renderer genérico:

- `OledUtils_SetUNERNotificationText(line1,line2)`
- `OledUtils_RenderUNERNotification()`

Y mantiene pantallas existentes para casos puntuales (ej: scan completado/cancelado, controlador conectado).

## 7) Integración recomendada de uso
1. Productor llama `UNER_App_SendCommand`.
2. Loop principal ejecuta `UNER_App_Poll()` (incluye scheduler + parser).
3. Al recibir ACK/RESP se libera `inflight`.
4. En scan: `START_SCAN` -> esperar finalizador `0xFE` -> enviar `GET_SCAN_RESULTS` y luego renderizar lista.
5. UI consume notificaciones y estado.

## 8) Nota de seguridad
`GET_CREDENTIALS` y algunos eventos pueden transportar password en claro. Usar sólo en enlaces confiables.
