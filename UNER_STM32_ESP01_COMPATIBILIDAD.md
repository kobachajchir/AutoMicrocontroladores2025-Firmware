# Protocolo UNER v2 STM32 ↔ ESP01 (Implementación Firmware STM32)

Este documento resume **cómo quedó integrado UNER v2 del lado STM32** para que sea compatible con la ESP01 (UNER v2), incluyendo scheduler por flags, comandos soportados y comportamiento de UI/notificaciones.

## 1) Compatibilidad de frame (wire)
STM32 usa el mismo frame UNER v2:

1. `H0..H3 = 'U' 'N' 'E' 'R'`
2. `LEN`
3. `TOKEN = 0x3A`
4. `VER = 0x02`
5. `ROUTE` (src/dst en nibbles)
6. `CMD`
7. `PAYLOAD`
8. `CHK` (XOR acumulado)

Nodos usados:
- `src STM32/MCU = 0x1`
- `dst ESP/PC bridge = 0x2`
- `broadcast = 0xF`

## 2) Scheduler por flags (sin pérdida de comandos)
Se reemplazó el modelo de “un único comando pendiente” por bitmask:

- `cmd_flags_pending`: comandos pendientes.
- `cmd_flags_inflight`: comando en vuelo.
- `cmd_last_sent`: último cmd enviado (debug/consistencia).

API implementada:
- `UNER_CmdFlag_Set(mask)`
- `UNER_CmdFlag_Clear(mask)`
- `UNER_CmdFlag_GetPending()`
- `UNER_CmdScheduler_TrySendNext()`
- `UNER_CmdScheduler_OnTxComplete()`

### Prioridad actual
1. `STOP_SCAN`
2. `START_SCAN`
3. `GET_SCAN_RESULTS`
4. `PING`
5. `REQUEST_FIRMWARE`
6. `REBOOT_ESP`
7. `GET_STATUS`

### Concurrencia
- `Set/Clear/Get` protegidos con sección crítica corta.
- ISR TX complete sólo libera `inflight` y limpia bit enviado.
- Coalescing natural: si llega el mismo flag repetido, se envía una sola vez.

## 3) Comandos STM32 enviados por UI

- `0x31 PING`: al chequear conexión ESP.
- `0x41 REQUEST_FIRMWARE`: al ocultar notificación de firmware (si no se canceló).
- `0x16 REBOOT_ESP`: al ocultar notificación de reset (si no se canceló).
- `0x14 START_SCAN`: al entrar a búsqueda de redes.
- `0x15 GET_SCAN_RESULTS`: al recibir finalizador de scan (`0xFE`).
- `0x18 STOP_SCAN`: al cancelar búsqueda (click encoder) o timeout.
- `0x30 GET_STATUS`: al abrir pantalla de info WiFi y al iniciar stack.

## 4) Respuestas/eventos decodificados

### Respuestas directas
- `PING (0x31)`: si `status=0`, marca ESP presente y muestra notificación.
- `GET_STATUS (0x30)`: actualiza flags locales de `AP_ACTIVE`/`WIFI_ACTIVE`.
- `GET_SCAN_RESULTS (0x15)`:
  - payload `[0xFE]` => fin asíncrona de scan, muestra “búsqueda completada” y agenda `GET_SCAN_RESULTS`.
  - payload `[status,count,ssid_len,ssid...]` => parsea SSIDs y abre pantalla de resultados.

### Eventos ESP nuevos soportados
- `UNER_EVT_NETWORK_IP (0x93)` (if + ip): actualiza estado de interfaz (STA/AP).
- `UNER_EVT_BOOT_COMPLETE (0x94)` (if + ip): marca ESP presente y sincroniza interfaz inicial.

## 5) Flujo de scan implementado
1. Entrar a pantalla “Buscar APs” → `START_SCAN`.
2. Si usuario cancela (encoder short press) → `STOP_SCAN`.
3. Si ESP envía `CMD=0x15 payload=[0xFE]` → se notifica fin y se envía `GET_SCAN_RESULTS`.
4. Al recibir resultados (`status=0`) se guardan SSID en buffer local y se muestra pantalla de resultados.

## 6) Buffers locales de resultados WiFi
- `wifiScanCount`
- `wifiScanSsids[8][33]`

La pantalla de resultados muestra hasta 3 SSID por diseño OLED (128x64), manteniendo compatibilidad con UI existente.

## 7) Política de notificaciones con cancelación
Para `REQUEST_FIRMWARE` y `REBOOT_ESP`:
- Se muestra notificación “enviar ...”.
- Si el usuario la cancela, **no se envía comando**.
- Si no cancela y expira timeout, el hook `onHide` dispara el comando por flag.

## 8) Compatibilidad con implementación actual
Se mantiene transporte UART1+DMA y parser UNER actual. La transmisión existente no se rompe; el scheduler serializa envíos respetando DMA busy y callback de TX complete.
