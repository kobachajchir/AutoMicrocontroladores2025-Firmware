# OLED Screen Inventory
The generated TypeScript builders are under `/screens/*.ts`. Each builder returns the ordered semantic `OledCommand[]` that the React OLED renderer can execute. The official identity remains the numeric `screen_code`; labels and function names are support metadata.

## Translation Rules Applied

- `ssd1306_Clear()` and wrapper-level `OledUtils_Clear()` became `{ type: "clear" }`.
- `ssd1306_SetColor(White|Black|Inverse)` became `setColor`.
- `ssd1306_SetCursor(x, y)` became `setCursor`.
- `ssd1306_WriteString(..., Font_7x10|Font_11x18)` became `drawText`.
- `Oled_DrawStrMax(text, n)` became `drawTextMax` with the same `maxChars`.
- `Oled_ClearBox` became `setColor Black` + `fillRect` + `setColor White`.
- `Oled_DrawBox` became `setColor White` + `fillRect`.
- `Oled_DrawFrame` became `setColor White` + `drawRect`.
- `Oled_DrawXBM` became `setColor White` + `drawBitmap`.
- `Oled_DrawXBM_MSB` and `ssd1306_DrawQR_Fixed` became `drawBitmapMSB`. The QR renderer is exact because the C QR loop is 64x64, row-packed, MSB-first.
- Notification progress is exposed separately by `notificationProgressOverlayCommands()` and appended by notification builders by default, matching `OledUtils_RenderNotification_Wrapper()`.
- C unsigned cursor underflows are preserved with `u8(...)`. For example, `7 - Font_7x10.FontHeight` becomes `253`.

## Screen Table

| # | Screen / state | screen_code | Source function | Category | Builder | Params / assets | Translation |
|---:|---|---:|---|---|---|---|---|
| 1 | Startup splash | `0x010001` | `OledUtils_RenderStartupNotification` in `Core/Src/oled_utils.c` | transient state screen | `buildScreen010001StartupCommands` | `Icon_Auto_bits`, MSB bitmap | exact |
| 2 | Dashboard | `0x010101` | `OledUtils_RenderDashboard` via `OledUtils_RenderDashboard_Wrapper` | dashboard screen | `buildScreen010101DashboardCommands` | `connection`, `ssid`, `ipAddress`, `usbActive`, `rfActive`, `carMode`; WiFi/AP/USB/RF/user/encoder icons | exact, parameterized |
| 3 | Mode change selector, full draw | `0x010102` | `OledUtils_RenderModeChange_Full` | transient state screen | `buildScreen010102ModeChangeCommands` | `mode`; arrow and encoder icons | exact |
| 4 | Mode change selector, text-only patch | `0x010102` | `OledUtils_RenderModeChange_ModeOnly` | transient state patch | `buildScreen010102ModeOnlyPatchCommands` | `mode`; clears only mode text area | exact |
| 5 | Main menu | `0x010201` | `MenuSys_RenderMenu` + `OledUtils_DrawItem` | menu screen | `buildScreen010201MainMenuCommands` | selected index, visible `Sensores`; WiFi/Sensors/Config/Volver icons | exact for current menu renderer |
| 6 | WiFi menu | `0x020101` | `MenuSys_RenderMenu` + `submenu1Items` | menu screen | `buildScreen020101WifiMenuCommands` | selected index; Info/Refresh/Link/Volver icons | exact |
| 7 | ESP submenu | `0x020301` | `MenuSys_RenderMenu` + `submenuESPItems` | menu screen | `buildScreen020301EspMenuCommands` | selected index; Link/Info/Refresh/Volver icons | exact |
| 8 | Sensors menu | `0x030101` | `MenuSys_RenderMenu` + `submenu2Items` | menu screen | `buildScreen030101SensorsMenuCommands` | selected index; tool icons | exact |
| 9 | Settings menu | `0x050101` | `MenuSys_RenderMenu` + `submenu3Items` | menu screen | `buildScreen050101SettingsMenuCommands` | selected index; Preferences item has no renderer/screen code | exact |
| 10 | WiFi results menu | `0x020203` | `OledUtils_RenderWiFiSearchResults_Wrapper` + `WiFiResults_BuildMenu` | menu screen | `buildScreen020203WifiResultsMenuCommands` | `networkSsids`, selected index; dynamic SSID labels | exact, parameterized |
| 11 | WiFi results summary | `0x020203` | `OledUtils_ShowWifiResults` | status view | `buildScreen020203WifiResultsSummaryCommands` | encoder icon | exact, but no active caller found |
| 12 | WiFi status, STA connected | `0x020201` | `OledUtils_RenderWiFiStatus` | connectivity screen | `buildScreen020201WifiStatusCommands({state:"sta"})` | `ssid`, `ipAddress`, WiFi icon | exact |
| 13 | WiFi status, AP active | `0x020201` | `OledUtils_RenderWiFiStatus` | connectivity screen | `buildScreen020201WifiStatusCommands({state:"ap"})` | `ipAddress`, AP icon | exact |
| 14 | WiFi status, disconnected | `0x020201` | `OledUtils_RenderWiFiStatus` | connectivity screen | `buildScreen020201WifiStatusCommands({state:"disconnected"})` | disconnected WiFi icon | exact |
| 15 | WiFi search with timer | `0x020202` | `OledUtils_RenderWiFiSearchScene` + `OledUtils_UpdateWiFiSearchTimer` | transient state screen | `buildScreen020202WifiSearchCommands` | `secondsRemaining`, WiFi/encoder icons | exact |
| 16 | WiFi not connected notification | `0x020204` | `OledUtils_RenderWiFiNotConnected` | notification | `buildScreen020204WifiNotConnectedNotificationCommands` | disconnected WiFi icon, notification progress overlay | exact |
| 17 | WiFi connecting | `0x020205` | `OledUtils_RenderWiFiConnecting`; alias `OledUtils_RenderESPWifiConnectingNotification` | notification | `buildScreen020205WifiConnectingCommands` | `ssid`, WiFi icon, `drawTextMax(14)` | exact |
| 18 | WiFi connected | `0x020206` | `OledUtils_RenderWiFiConnected`; alias `OledUtils_RenderESPWifiConnectedNotification` | notification | `buildScreen020206WifiConnectedCommands` | `ssid`, `ipAddress`, WiFi icon, `drawTextMax` | exact |
| 19 | WiFi search complete | `0x020207` | `OledUtils_RenderWiFiSearchCompleteNotification` | notification | `buildScreen020207WifiSearchCompleteNotificationCommands` | WiFi icon | exact |
| 20 | WiFi search canceled | `0x020208` | `OledUtils_RenderWiFiSearchCanceledNotification` | notification | `buildScreen020208WifiSearchCanceledNotificationCommands` | disconnected WiFi icon | exact |
| 21 | ESP checking connection | `0x020302` | `OledUtils_RenderESPCheckingConnectionNotification` | notification | `buildScreen020302EspCheckingConnectionNotificationCommands` | text only | exact |
| 22 | ESP firmware request | `0x020303` | `OledUtils_RenderESPFirmwareRequestNotification` | notification | `buildScreen020303EspFirmwareRequestNotificationCommands` | info and encoder icons | exact |
| 23 | ESP reset sent | `0x020304` | `OledUtils_RenderESPResetSentNotification` | notification | `buildScreen020304EspResetSentNotificationCommands` | refresh and encoder icons | exact |
| 24 | ESP check required | `0x020305` | `OledUtils_RenderESPCheckConnectionRequiredNotification` | notification | `buildScreen020305EspCheckConnectionRequiredNotificationCommands` | text only | exact |
| 25 | ESP boot received | `0x020306` | `OledUtils_RenderESPBootReceivedNotification` | notification | `buildScreen020306EspBootReceivedNotificationCommands` | `Icon_Info_bits` drawn MSB as 19x16, exactly as C | exact |
| 26 | ESP firmware full screen | `0x020307` | `OledUtils_RenderESPFirmwareScreen` via `OledUtils_RenderESPFirmwareScreen_Wrapper` | full screen | `buildScreen020307EspFirmwareScreenCommands` | `firmwareVersion`, info icon, `drawTextMax(17)` | exact |
| 27 | ESP firmware received notification | `0x020307` | `OledUtils_RenderESPFirmwareReceivedNotification` | notification | `buildScreen020307EspFirmwareReceivedNotificationCommands` | `firmwareVersion`, info icon, `drawTextMax(17)` | exact; same code as full firmware screen |
| 28 | ESP mode changed | `0x020308` | `OledUtils_RenderESPModeChangedNotification` | notification | `buildScreen020308EspModeChangedNotificationCommands` | info icon | exact |
| 29 | ESP AP started | `0x020309` | `OledUtils_RenderESPAPStartedNotification` | notification | `buildScreen020309EspApStartedNotificationCommands` | `ipAddress`, WiFi icon | exact |
| 30 | USB connected | `0x020401` | `OledUtils_RenderESPUsbConnectedNotification` | notification | `buildScreen020401UsbConnectedNotificationCommands` | info icon | exact |
| 31 | USB disconnected | `0x020402` | `OledUtils_RenderESPUsbDisconnectedNotification` | notification | `buildScreen020402UsbDisconnectedNotificationCommands` | info icon | exact |
| 32 | Web server up | `0x020501` | `OledUtils_RenderESPWebServerUpNotification` | notification | `buildScreen020501WebServerUpNotificationCommands` | info icon | exact |
| 33 | Web client connected | `0x020502` | `OledUtils_RenderESPWebClientConnectedNotification` | notification | `buildScreen020502WebClientConnectedNotificationCommands` | link icon | exact |
| 34 | Web client disconnected | `0x020503` | `OledUtils_RenderESPWebClientDisconnectedNotification` | notification | `buildScreen020503WebClientDisconnectedNotificationCommands` | link icon plus two diagonal lines | exact |
| 35 | IR values live graph | `0x030201` | `OledUtils_RenderIRGraphScene` + `OledUtils_UpdateIRBars` | sensor screen | `buildScreen030201IrValuesCommands` | `irValues[8]` | exact; legend y underflows to 253 like firmware |
| 36 | Legacy IR graph | `0x030201` | `OledUtils_DrawIRGraph` + `OledUtils_DrawIRBars` | sensor screen | `buildScreen030201LegacyIrGraphCommands` | `irValues[8]` | exact; currently not the wrapper path |
| 37 | MPU live values | `0x030301` | `OledUtils_RenderMPUScene` + `OledUtils_UpdateMPUValues` | sensor screen | `buildScreen030301MpuValuesCommands` | accel/gyro values | exact; title y underflows to 254 like firmware |
| 38 | Legacy MPU table | `0x030301` | `OledUtils_RenderValoresMPUScreen` | sensor screen | `buildScreen030301LegacyMpuTableCommands` | calibration, accel/gyro, milli-degree angles | exact; currently not the wrapper path |
| 39 | Radar | `0x030401` | `OledUtils_RenderRadarGraph` | sensor screen | `buildScreen030401RadarCommands` | car icon, two circles | exact |
| 40 | Radar objects patch | `0x030401` | `OledUtils_RenderRadarGraph_Objs` | patch | `buildScreen030401RadarObjectsPatchCommands` | none | exact empty patch |
| 41 | Motor test, full | `0x040101` | `OledUtils_MotorTest_Complete` | motor/control screen | `buildScreen040101MotorTestCommands` | selected motor, direction, speeds, movement flag | exact |
| 42 | Motor test, patch | `0x040101` | `OledUtils_MotorTest_Changes` | motor/control patch | `buildScreen040101MotorTestPatchCommands` | selected motor, direction, speeds, movement flag | exact |
| 43 | About project | `0x050201` | `OledUtils_RenderProyectScreen` | settings/info screen | `buildScreen050201AboutProjectCommands` | info icon, arrow | exact |
| 44 | About repo QR | `0x050202` | `OledUtils_RenderProyectInfoScreen` | settings/info screen | `buildScreen050202AboutRepoCommands` | QR bitmap MSB and back arrow | exact |
| 45 | Warning time config | `0x050301` | `OledUtils_RenderWarningTimeConfig` | settings screen | `buildScreen050301WarningTimeCommands` | `seconds`, encoder icon | exact |
| 46 | Controller connected | `0x060101` | `OledUtils_RenderControllerConnected` | notification | `buildScreen060101ControllerConnectedNotificationCommands` | controller icon | exact |
| 47 | Controller disconnected | `0x060102` | `OledUtils_RenderControllerDisconnected` | notification | `buildScreen060102ControllerDisconnectedNotificationCommands` | controller icon, diagonal lines; preserves redundant Inverse->White sequence | exact |
| 48 | Command received | `0x060201` | `OledUtils_RenderCommandReceivedNotification` | notification | `buildScreen060201CommandReceivedNotificationCommands` | WiFi icon | exact |
| 49 | Ping received | `0x060202` | `OledUtils_RenderPingReceivedNotification` | notification | `buildScreen060202PingReceivedNotificationCommands` | info icon | exact |
| 50 | ESP connection succeeded | `0x060301` | `OledUtils_ESPConnSucceeded` | status view | `buildScreen060301EspConnectionSucceededCommands` | check and encoder icons | exact; no active caller found in current scan |
| 51 | ESP connection failed | `0x060302` | `OledUtils_ESPConnFailed` | status view | `buildScreen060302EspConnectionFailedCommands` | cross and encoder icons | exact; no active caller found in current scan |
| 52 | Test screen | `0x070101` | `OledUtils_RenderTestScreen` | service screen | `buildScreen070101TestScreenCommands` | none | exact empty screen |
| 53 | Lock screen, locked | `0x080101` | `OledUtils_RenderLockScreen` | warning screen | `buildScreen080101LockedCommands` | lock icon | exact |
| 54 | Lock screen, PIN incorrect | `0x080102` | `OledUtils_RenderLockScreen` | warning screen | `buildScreen080102PinIncorrectLockCommands` | lock icon | exact |
| 55 | Lock screen, PIN modified | `0x080103` | `OledUtils_RenderLockScreen` | warning screen | `buildScreen080103PinModifiedLockCommands` | lock icon | exact |
| 56 | Lock screen, enter PIN prompt | `0x080104` | `OledUtils_RenderLockScreen` | warning screen | `buildScreen080104LockEnterPinCommands` | lock icon | exact; same code as permission PIN form |
| 57 | Permission PIN entry form | `0x080104` | `Permission_RenderPinEntry` with `AUTH_INPUT_PIN` | confirmation / permission screen | `buildScreen080104PermissionPinEntryCommands` | digits, selected digit, user button and lock icons | exact; same code as lock enter PIN prompt |
| 58 | Permission PIN waiting | `0x080105` | `Permission_RenderPinEntry` with `AUTH_WAITING_ESP` | loading / permission screen | `buildScreen080105PermissionPinWaitingCommands` | masked digits, lock/user icons | exact |
| 59 | Permission PIN denied | `0x080106` | `Permission_RenderPinEntry` with `AUTH_DENIED` | warning screen | `buildScreen080106PermissionPinDeniedCommands` | `attemptsLeft` | exact |
| 60 | Permission PIN timeout | `0x080107` | `Permission_RenderPinEntry` with `AUTH_TIMEOUT` | warning screen | `buildScreen080107PermissionPinTimeoutCommands` | text only | exact |
| 61 | Permission PIN blocked | `0x080108` | `Permission_RenderPinEntry` with `AUTH_LOCKED` | warning screen | `buildScreen080108PermissionPinBlockedCommands` | text only | exact |
| 62 | Permission denied notification | `0x080109` | `OledUtils_RenderPermissionDeniedNotification` | notification | `buildScreen080109PermissionDeniedNotificationCommands` | lock icon | exact |

## Helper-Only Routines

| Helper | Source function | Generated builder/helper | Notes |
|---|---|---|---|
| Notification progress overlay | `OledUtils_RenderNotificationProgress` | `notificationProgressOverlayCommands` | Draws/clears `y=0` line. It has no independent `screen_code`; it is appended to notifications. |
| Current menu item draw | `OledUtils_DrawItem` | `buildMenuItemCommands` | Used by all current `MenuSys_RenderMenu` screens. |
| Legacy vertical menu | `OledUtils_RenderVerticalMenu` | `buildLegacyVerticalMenuCommands` | Present in code, but the current wrapper uses `MenuSys_RenderMenu`. |
| IR bar patch | `OledUtils_UpdateIRBars` | inlined into `buildScreen030201IrValuesCommands` | Partial live sensor update. |
| MPU value patch | `OledUtils_UpdateMPUValues` | inlined into `buildScreen030301MpuValuesCommands` | Partial live sensor update. |
| WiFi search timer patch | `OledUtils_UpdateWiFiSearchTimer` | inlined into `buildScreen020202WifiSearchCommands` | Timer-only patch after the search scene. |

## Completeness Checklist

- Total detected user-visible screen/render states: 62.
- Total translated builders: 62.
- Exact translations: 62.
- Partial translations: 0.
- Unresolved screens: 0.
- Helper-only routines without independent screen identity: notification progress overlay, current menu item draw, legacy vertical menu, IR/MPU/timer patches.

## Ambiguities And Edge Cases

- `SCREEN_CODE_WARNING_PIN_ENTRY` (`0x080104`) is used by two visually different render paths: `OledUtils_RenderLockScreen` with the simple "Ingrese PIN" prompt and `Permission_RenderPinEntry` with the four-digit PIN form. The firmware currently disambiguates these through the report source (`SCREEN_REPORT_SOURCE_RENDER` vs `SCREEN_REPORT_SOURCE_PERMISSION`), not through a unique code.
- `SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_RECEIVED` (`0x020307`) is used by both the full firmware screen and the transient "FW ESP" notification. Use context/source or the builder selected by flow.
- `submenu3Items[0]` (`Preferencias`) has `SCREEN_CODE_NONE` and no `screenRenderFn`, so it is a menu item but not a renderable screen in the current code.
- `OledUtils_RenderIRGraphScene` and `OledUtils_RenderMPUScene` contain baseline expressions that underflow through `uint8_t` cursor parameters. The generated commands preserve firmware behavior with wrapped y values (`253` and `254`) rather than inventing corrected coordinates.
- `OledUtils_ESPConnSucceeded` and `OledUtils_ESPConnFailed` are translated because they render screens, but no active caller was found in the current scan.

## Web-Side Decoding

```ts
const menu = (screenCode >> 16) & 0xff;
const submenu = (screenCode >> 8) & 0xff;
const page = screenCode & 0xff;
```

For event payloads from the firmware, `screen_code` is serialized little-endian as four bytes followed by the one-byte `source` field. Example: payload bytes `01 02 01 00 01` decode to `screen_code = 0x010201` and `source = 1`.
