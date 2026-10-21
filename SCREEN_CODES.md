# Screen codes contract

Los screen codes son IDs estables para sincronizar UI embebida, Web y Qt. No son textos ni estados internos: son identificadores binarios de lo que el usuario debe ver o de la interaccion que esta activa.

Fuente de verdad: `Core/Inc/screen_codes.h`, `Core/Src/menusystem.c`, `Core/Src/oled_utils.c`, `Core/Src/permissions.c` y `Core/Src/menu_definition.c`.

## Formato

```c
SCREEN_CODE(menu, submenu, page) =
    ((menu & 0xFF) << 16) |
    ((submenu & 0xFF) << 8) |
    (page & 0xFF)
```

Campos:

| Campo | Bits | Uso |
|---|---:|---|
| menu | 23..16 | grupo mayor de UI |
| submenu | 15..8 | grupo interno |
| page | 7..0 | pantalla puntual |

Ejemplo: `SCREEN_CODE(0x03,0x03,0x02) = 0x00030302`, notificacion "MPU stream iniciado".

## Source del reporte

`EVT_SCREEN_CHANGED (0x95)` envia `[screen_code_le32, source]`.

| Source | Valor | Significado |
|---|---:|---|
| UNKNOWN | `0` | origen no definido |
| MENU | `1` | menu/submenu navegable |
| RENDER | `2` | pantalla activa renderizada |
| NOTIFICATION | `3` | overlay temporal |
| PERMISSION | `4` | flujo PIN/permiso |
| SYSTEM | `5` | arranque o cambio interno |

`EVT_MENU_SELECTION_CHANGED (0x96)` envia `[screen_code_le32, selected_index, item_count, source]`. `selected_index` es base cero; `MENU_ITEM_CLICK (0x53)` recibe item visible base uno.

## Estado vs notificacion vs interaccion

| Concepto | Persistencia | Input esperado | Ejemplo |
|---|---|---|---|
| Estado | refleja situacion del sistema | normalmente ninguno o refresh | WiFi conectado, USB conectado |
| Notificacion | overlay temporal, se restaura la pantalla anterior | normalmente ninguno | PING recibido, stream MPU iniciado |
| Interaccion | pantalla que consume eventos | encoder, boton, web remote input | menu, PIN, busqueda WiFi |

## Tabla completa

| Code | Macro | Tipo | Cuando aparece | Quien dispara | Web/Qt debe renderizar |
|---:|---|---|---|---|---|
| `0x00000000` | `SCREEN_CODE_NONE` | interno | sin pantalla valida | inicializacion/fallback | no renderizar; pedir `GET_CURRENT_SCREEN` |
| `0x00010001` | `SCREEN_CODE_CORE_STARTUP` | estado | arranque del menu system | `MenuSys_Init` | splash/estado inicial |
| `0x00010101` | `SCREEN_CODE_CORE_DASHBOARD` | pantalla activa | dashboard principal | menu, volver desde menu | dashboard del auto |
| `0x00010102` | `SCREEN_CODE_CORE_MODE_CHANGE` | notificacion/estado | cambio de modo del auto | wrappers/event manager | overlay de modo |
| `0x00010201` | `SCREEN_CODE_CORE_MAIN_MENU` | menu | menu raiz | boton/volver | lista: Wifi, Sensores, Config., Volver |
| `0x00020101` | `SCREEN_CODE_CONNECTIVITY_WIFI_MENU` | submenu | menu WiFi | item Wifi | lista: Info AP, Buscar APs, Conexion ESP, Volver |
| `0x00020201` | `SCREEN_CODE_CONNECTIVITY_WIFI_STATUS` | pantalla activa | Info AP | menu WiFi | estado STA/AP, IP/SSID si existe |
| `0x00020202` | `SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING` | interaccion | scan WiFi en curso | Buscar APs, START_SCAN | pantalla con timer/cancelar |
| `0x00020203` | `SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS` | menu/submenu | resultados WiFi listos | GET_SCAN_RESULTS status OK | lista de SSIDs encontrados |
| `0x00020204` | `SCREEN_CODE_CONNECTIVITY_WIFI_NOT_CONNECTED` | notificacion/estado | no hay red conectada | WiFi status | "No hay red conectada" |
| `0x00020205` | `SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTING` | notificacion | conexion STA en curso | CONNECT_WIFI | "Conectando a <ssid>" |
| `0x00020206` | `SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTED` | notificacion/estado | STA conectada e IP valida | eventos STA/IP | SSID e IP |
| `0x00020207` | `SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_COMPLETE` | notificacion | scan finalizado | resultados de scan | resumen scan completo |
| `0x00020208` | `SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_CANCELED` | notificacion | scan cancelado | STOP_SCAN/cancelar | icono WiFi, "Busqueda cancelada" |
| `0x00020209` | `SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS` | pantalla activa | detalle de una red seleccionada | WIFI_GET_DETAIL | SSID, RSSI, cifrado, canal, conectar |
| `0x0002020A` | `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_WEB` | notificacion/interaccion remota | STM espera credenciales Web | flujo credenciales | pedir credenciales en Web |
| `0x0002020B` | `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_SUCCEEDED` | notificacion | credenciales aceptadas/conexion OK | resultado Web/ESP | "WiFi conectado" |
| `0x0002020C` | `SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_FAILED` | notificacion | credenciales fallaron/timeout/cancelado | resultado Web/ESP | error segun status |
| `0x00020301` | `SCREEN_CODE_CONNECTIVITY_ESP_MENU` | submenu | menu Conexion ESP | menu WiFi | lista: Chk Conexion, Firmware, Reset ESP, Volver |
| `0x00020302` | `SCREEN_CODE_CONNECTIVITY_ESP_CHECKING` | notificacion | chequeo conexion ESP | Chk Conexion | "Chequeando conexion" |
| `0x00020303` | `SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST` | interaccion | pedir firmware ESP | menu ESP | pantalla "Pedir firmware" |
| `0x00020304` | `SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT` | notificacion | reset enviado al ESP | Reset ESP concedido | "Reset enviado" |
| `0x00020305` | `SCREEN_CODE_CONNECTIVITY_ESP_CHECK_REQUIRED` | notificacion | se requiere chequeo de ESP | fallo/conexion dudosa | "Chequee conexion" |
| `0x00020306` | `SCREEN_CODE_CONNECTIVITY_ESP_BOOT_RECEIVED` | notificacion | evento boot ESP recibido | EVT_BOOT | "ESP iniciada" |
| `0x00020307` | `SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_RECEIVED` | pantalla activa | version ESP recibida | REQUEST_FIRMWARE | version firmware |
| `0x00020308` | `SCREEN_CODE_CONNECTIVITY_ESP_MODE_CHANGED` | notificacion | modo WiFi ESP cambio | EVT_MODE_CHANGED | "Modo actualizado" |
| `0x00020309` | `SCREEN_CODE_CONNECTIVITY_ESP_AP_STARTED` | notificacion | AP iniciado | START_AP/IP | "AP OK" e IP |
| `0x0002030A` | `SCREEN_CODE_CONNECTIVITY_ESP_REBOOT_MODE` | interaccion | seleccion modo reboot ESP | Reset ESP | opciones de reboot |
| `0x00020401` | `SCREEN_CODE_CONNECTIVITY_USB_CONNECTED` | notificacion/estado | USB conectado | EVT_USB_CONNECTED | "USB conectado" |
| `0x00020402` | `SCREEN_CODE_CONNECTIVITY_USB_DISCONNECTED` | notificacion/estado | USB desconectado | EVT_USB_DISCONNECTED | "USB desconectado" |
| `0x00020501` | `SCREEN_CODE_CONNECTIVITY_WEB_SERVER_UP` | notificacion/estado | servidor Web listo | EVT_WEBSERVER_UP | "WEB lista" |
| `0x00020502` | `SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_CONNECTED` | notificacion/estado | cliente Web conectado | EVT_WEBSERVER_CLIENT_CONNECTED | "Web conectado" |
| `0x00020503` | `SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_DISCONNECTED` | notificacion/estado | cliente Web desconectado | EVT_WEBSERVER_CLIENT_DISCONNECTED | "Web desconectado" |
| `0x00030101` | `SCREEN_CODE_SENSORS_MENU` | submenu | menu Sensores | item Sensores, visible en test mode | lista: Valores IR, Valores MPU, Test motores, Volver |
| `0x00030201` | `SCREEN_CODE_SENSORS_IR_VALUES` | pantalla activa | vista IR | Valores IR | grafico de 8 barras IR |
| `0x00030301` | `SCREEN_CODE_SENSORS_MPU_VALUES` | pantalla activa | vista MPU | Valores MPU | accel y gyro compactos |
| `0x00030302` | `SCREEN_CODE_SENSORS_MPU_STREAM_STARTED` | notificacion | streaming MPU aceptado | CMD 0x61 OK enable=1 | icono info, "MPU stream iniciado" |
| `0x00030303` | `SCREEN_CODE_SENSORS_MPU_STREAM_STOPPED` | notificacion | streaming MPU detenido | CMD 0x61 enable=0 o CMD 0x62 OK | icono info, "MPU stream detenido" |
| `0x00030401` | `SCREEN_CODE_SENSORS_RADAR` | reservado | radar/sensor futuro | no confirmado | reservar renderer |
| `0x00040101` | `SCREEN_CODE_MOTOR_TEST` | interaccion protegida | test motores | item Test motores | controles de motor, requiere permiso si aplica |
| `0x00050101` | `SCREEN_CODE_SETTINGS_MENU` | submenu | menu Config | item Config. | lista: Preferencias, Acerca de, Volver |
| `0x00050201` | `SCREEN_CODE_SETTINGS_ABOUT_PROJECT` | pantalla activa | acerca del proyecto | item Acerca de | info del proyecto |
| `0x00050202` | `SCREEN_CODE_SETTINGS_ABOUT_REPO` | pantalla activa | repo/QR/info | About flow | QR o link repo |
| `0x00050301` | `SCREEN_CODE_SETTINGS_WARNING_TIME` | advertencia | tiempo/config warning | settings | advertencia temporal |
| `0x00060101` | `SCREEN_CODE_DIAG_CONTROLLER_CONNECTED` | notificacion/estado | control conectado | EVT_CONTROLLER_CONNECTED | "Control conectado" |
| `0x00060102` | `SCREEN_CODE_DIAG_CONTROLLER_DISCONNECTED` | notificacion/estado | control desconectado | EVT_CONTROLLER_DISCONNECTED | "Control desconectado" |
| `0x00060201` | `SCREEN_CODE_DIAG_COMMAND_RECEIVED` | notificacion | comando ECHO recibido | CMD 0x42 | "ECHO recibido" |
| `0x00060202` | `SCREEN_CODE_DIAG_PING_RECEIVED` | notificacion | PING recibido | CMD 0x31 | "PING recibido" |
| `0x00060301` | `SCREEN_CODE_DIAG_ESP_CONN_SUCCEEDED` | notificacion/estado | conexion ESP OK | diagnostico | exito conexion ESP |
| `0x00060302` | `SCREEN_CODE_DIAG_ESP_CONN_FAILED` | notificacion/estado | conexion ESP fallo | diagnostico | fallo conexion ESP |
| `0x00070101` | `SCREEN_CODE_SERVICE_TEST_SCREEN` | desarrollo | pantalla de test | wrapper servicio | no usar en UX final |
| `0x00080101` | `SCREEN_CODE_WARNING_LOCKED` | warning | sistema bloqueado | permisos/seguridad | bloqueo general |
| `0x00080102` | `SCREEN_CODE_WARNING_PIN_INCORRECT` | warning | PIN incorrecto legado | permisos | error PIN |
| `0x00080103` | `SCREEN_CODE_WARNING_PIN_MODIFIED` | warning | PIN modificado | permisos | confirmacion PIN |
| `0x00080104` | `SCREEN_CODE_WARNING_PIN_ENTRY` | validacion | ingreso de PIN | `Permission_Request` | 4 digitos, encoder cambia, OK confirma |
| `0x00080105` | `SCREEN_CODE_WARNING_PIN_WAITING` | validacion | PIN enviado, esperando ESP/Web | `AUTH_WAITING_ESP` | spinner/espera validacion |
| `0x00080106` | `SCREEN_CODE_WARNING_PIN_DENIED` | validacion | PIN denegado | validacion negativa | intentos restantes, reintentar |
| `0x00080107` | `SCREEN_CODE_WARNING_PIN_TIMEOUT` | validacion | timeout de validacion | `Permission_Task` 5s | "Sin resp.", volver |
| `0x00080108` | `SCREEN_CODE_WARNING_PIN_BLOCKED` | validacion | demasiados fallos | permisos | bloqueado |
| `0x00080109` | `SCREEN_CODE_WARNING_PERMISSION_DENIED` | notificacion | accion restringida sin permiso | permiso denegado | lock, "Accion restringida" |

## Reglas para Web/Qt

1. La UI remota no debe inferir estado desde textos del OLED; debe usar `EVT_SCREEN_CHANGED (0x95)` y `EVT_MENU_SELECTION_CHANGED (0x96)`.
2. Si Web/Qt pierde sincronizacion, debe enviar `GET_CURRENT_SCREEN (0x52)` y luego reconstruir la vista.
3. Para simular input, Web/Qt debe incluir siempre el screen code actual. Si el STM cambiate pantalla antes de recibir el comando, responde `SCREEN_MISMATCH`.
4. Las notificaciones son overlays. Web/Qt puede mostrarlas sin destruir el estado de menu local; al expirar, debe esperar el siguiente `EVT_SCREEN_CHANGED` o pedir `GET_CURRENT_SCREEN`.
5. Las pantallas `PIN_*` son validaciones: bloquean el flujo anterior hasta `granted`, `denied`, `timeout`, `cancelled` o `locked`.

