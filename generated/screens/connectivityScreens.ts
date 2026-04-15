import {
  baselineY,
  clear,
  clearBox,
  drawXbm,
  drawXbmMsb,
  setWhite,
  textAt,
  textMaxAt,
  withNotificationProgress,
} from "./helpers";
import {
  SCREEN_CODE_CONNECTIVITY_ESP_AP_STARTED,
  SCREEN_CODE_CONNECTIVITY_ESP_BOOT_RECEIVED,
  SCREEN_CODE_CONNECTIVITY_ESP_CHECK_REQUIRED,
  SCREEN_CODE_CONNECTIVITY_ESP_CHECKING,
  SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_RECEIVED,
  SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST,
  SCREEN_CODE_CONNECTIVITY_ESP_MODE_CHANGED,
  SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT,
  SCREEN_CODE_CONNECTIVITY_USB_CONNECTED,
  SCREEN_CODE_CONNECTIVITY_USB_DISCONNECTED,
  SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_CONNECTED,
  SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_DISCONNECTED,
  SCREEN_CODE_CONNECTIVITY_WEB_SERVER_UP,
  SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTED,
  SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTING,
  SCREEN_CODE_CONNECTIVITY_WIFI_NOT_CONNECTED,
  SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_CANCELED,
  SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_COMPLETE,
  SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING,
  SCREEN_CODE_CONNECTIVITY_WIFI_STATUS,
} from "./screenCodes";
import type { NotificationProgressArgs, OledCommand } from "./types";

export type WifiStatusState = "sta" | "ap" | "disconnected";

export interface WifiStatusArgs {
  state: WifiStatusState;
  ssid?: string;
  ipAddress?: string;
}

function simpleNotification(commands: OledCommand[], progress?: NotificationProgressArgs): OledCommand[] {
  return withNotificationProgress(commands, progress);
}

export const screen020201WifiStatusCode = SCREEN_CODE_CONNECTIVITY_WIFI_STATUS;

export function buildScreen020201WifiStatusCommands(args: WifiStatusArgs): OledCommand[] {
  if (args.state === "sta") {
    return [
      clear(),
      setWhite(),
      ...drawXbm(54, 4, 19, 16, "Icon_Wifi_100_bits"),
      ...textAt(23, baselineY(31, "Font7x10"), "Conectado a", "Font7x10"),
      ...textMaxAt(10, baselineY(44, "Font7x10"), args.ssid ?? "WiFi", 16, "Font7x10"),
      ...textMaxAt(8, baselineY(60, "Font7x10"), args.ipAddress ?? "Sin IP", 16, "Font7x10"),
    ];
  }

  if (args.state === "ap") {
    return [
      clear(),
      setWhite(),
      ...drawXbm(54, 4, 15, 16, "Icon_APWifi_bits"),
      ...textAt(28, baselineY(31, "Font7x10"), "AP activo", "Font7x10"),
      ...textMaxAt(8, baselineY(48, "Font7x10"), args.ipAddress ?? "Sin IP", 16, "Font7x10"),
    ];
  }

  return [
    clear(),
    setWhite(),
    ...drawXbm(54, 3, 19, 16, "Icon_Wifi_NotConnected_bits"),
    ...textAt(7, baselineY(40, "Font11x18"), "No hay red", "Font11x18"),
    ...textAt(16, baselineY(55, "Font11x18"), "conectada", "Font11x18"),
  ];
}

export const screen020202WifiSearchCode = SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING;

export function buildScreen020202WifiSearchCommands(args: { secondsRemaining: number }): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(27, baselineY(20, "Font11x18"), "Buscando", "Font11x18"),
    ...textAt(2, baselineY(40, "Font11x18"), "redes wifi", "Font11x18"),
    ...textAt(50, baselineY(60, "Font7x10"), "Cancelar", "Font7x10"),
    ...drawXbm(112, 48, 13, 13, "Icon_Encoder_bits"),
    ...drawXbm(2, 2, 19, 16, "Icon_Wifi_100_bits"),
    ...clearBox(4, baselineY(62, "Font11x18"), 22, 18),
    ...textAt(4, baselineY(62, "Font11x18"), String(args.secondsRemaining), "Font11x18"),
  ];
}

export const screen020204WifiNotConnectedCode = SCREEN_CODE_CONNECTIVITY_WIFI_NOT_CONNECTED;

export function buildScreen020204WifiNotConnectedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...textAt(7, baselineY(40, "Font11x18"), "No hay red", "Font11x18"),
    ...drawXbm(54, 3, 19, 16, "Icon_Wifi_NotConnected_bits"),
    ...textAt(16, baselineY(55, "Font11x18"), "conectada", "Font11x18"),
  ], progress);
}

export const screen020205WifiConnectingCode = SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTING;

export function buildScreen020205WifiConnectingCommands(args: { ssid: string }, progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(53, 14, 19, 16, "Icon_Wifi_100_bits"),
    ...textAt(24, baselineY(42, "Font7x10"), "Conectando a", "Font7x10"),
    ...textMaxAt(24, baselineY(54, "Font7x10"), args.ssid, 14, "Font7x10"),
  ], progress);
}

export const screen020206WifiConnectedCode = SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTED;

export function buildScreen020206WifiConnectedCommands(args: { ssid: string; ipAddress: string }, progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(53, 6, 19, 16, "Icon_Wifi_100_bits"),
    ...textAt(24, baselineY(31, "Font7x10"), "Conectado a", "Font7x10"),
    ...textMaxAt(24, baselineY(43, "Font7x10"), args.ssid, 14, "Font7x10"),
    ...textAt(16, baselineY(59, "Font7x10"), "IP:", "Font7x10"),
    ...textMaxAt(34, baselineY(59, "Font7x10"), args.ipAddress, 15, "Font7x10"),
  ], progress);
}

export const screen020207WifiSearchCompleteCode = SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_COMPLETE;

export function buildScreen020207WifiSearchCompleteNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 19, 16, "Icon_Wifi_100_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "Busqueda", "Font11x18"),
    ...textAt(2, baselineY(50, "Font11x18"), "completada", "Font11x18"),
  ], progress);
}

export const screen020208WifiSearchCanceledCode = SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_CANCELED;

export function buildScreen020208WifiSearchCanceledNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 19, 16, "Icon_Wifi_NotConnected_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "Busqueda", "Font11x18"),
    ...textAt(10, baselineY(50, "Font11x18"), "cancelada", "Font11x18"),
  ], progress);
}

export const screen020302EspCheckingCode = SCREEN_CODE_CONNECTIVITY_ESP_CHECKING;

export function buildScreen020302EspCheckingConnectionNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...textAt(2, baselineY(30, "Font11x18"), "Chequeando", "Font11x18"),
    ...textAt(10, baselineY(50, "Font11x18"), "conexion", "Font11x18"),
  ], progress);
}

export const screen020303EspFirmwareRequestCode = SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST;

export function buildScreen020303EspFirmwareRequestNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...textAt(32, baselineY(24, "Font11x18"), "Pedir", "Font11x18"),
    ...textAt(32, baselineY(44, "Font11x18"), "firmware", "Font11x18"),
    ...textAt(50, baselineY(60, "Font7x10"), "Cancelar", "Font7x10"),
    ...drawXbm(112, 48, 13, 13, "Icon_Encoder_bits"),
    ...drawXbm(8, 16, 16, 16, "Icon_Info_bits"),
  ], progress);
}

export const screen020304EspResetSentCode = SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT;

export function buildScreen020304EspResetSentNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...textAt(32, baselineY(24, "Font11x18"), "Enviar", "Font11x18"),
    ...textAt(32, baselineY(44, "Font11x18"), "reset", "Font11x18"),
    ...textAt(50, baselineY(60, "Font7x10"), "Cancelar", "Font7x10"),
    ...drawXbm(112, 48, 13, 13, "Icon_Encoder_bits"),
    ...drawXbm(8, 16, 16, 16, "Icon_Refrescar_bits"),
  ], progress);
}

export const screen020305EspCheckRequiredCode = SCREEN_CODE_CONNECTIVITY_ESP_CHECK_REQUIRED;

export function buildScreen020305EspCheckConnectionRequiredNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...textAt(16, baselineY(30, "Font11x18"), "Chequee", "Font11x18"),
    ...textAt(10, baselineY(50, "Font11x18"), "conexion", "Font11x18"),
  ], progress);
}

export const screen020306EspBootReceivedCode = SCREEN_CODE_CONNECTIVITY_ESP_BOOT_RECEIVED;

export function buildScreen020306EspBootReceivedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbmMsb(2, 12, 19, 16, "Icon_Info_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "ESP", "Font11x18"),
    ...textAt(2, baselineY(50, "Font11x18"), "iniciada", "Font11x18"),
  ], progress);
}

export const screen020307EspFirmwareReceivedCode = SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_RECEIVED;

export function buildScreen020307EspFirmwareScreenCommands(args: { firmwareVersion?: string }): OledCommand[] {
  const firmwareVersion = args.firmwareVersion && args.firmwareVersion.length > 0 ? args.firmwareVersion : "Sin datos";

  return [
    clear(),
    setWhite(),
    ...drawXbm(8, 5, 16, 16, "Icon_Info_bits"),
    ...textAt(34, baselineY(22, "Font11x18"), "FW ESP", "Font11x18"),
    ...textMaxAt(6, baselineY(43, "Font7x10"), firmwareVersion, 17, "Font7x10"),
    ...textAt(33, baselineY(62, "Font7x10"), "OK: volver", "Font7x10"),
  ];
}

export function buildScreen020307EspFirmwareReceivedNotificationCommands(args: { firmwareVersion: string }, progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(8, 16, 16, 16, "Icon_Info_bits"),
    ...textAt(34, baselineY(24, "Font11x18"), "FW ESP", "Font11x18"),
    ...textMaxAt(6, baselineY(48, "Font7x10"), args.firmwareVersion, 17, "Font7x10"),
  ], progress);
}

export const screen020308EspModeChangedCode = SCREEN_CODE_CONNECTIVITY_ESP_MODE_CHANGED;

export function buildScreen020308EspModeChangedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 16, 16, "Icon_Info_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "Modo", "Font11x18"),
    ...textAt(8, baselineY(50, "Font11x18"), "actualizado", "Font11x18"),
  ], progress);
}

export const screen020309EspApStartedCode = SCREEN_CODE_CONNECTIVITY_ESP_AP_STARTED;

export function buildScreen020309EspApStartedNotificationCommands(args: { ipAddress: string }, progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 19, 16, "Icon_Wifi_100_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "AP OK", "Font11x18"),
    ...textAt(8, baselineY(60, "Font7x10"), args.ipAddress, "Font7x10"),
  ], progress);
}

export const screen020401UsbConnectedCode = SCREEN_CODE_CONNECTIVITY_USB_CONNECTED;

export function buildScreen020401UsbConnectedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 16, 16, "Icon_Info_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "USB", "Font11x18"),
    ...textAt(2, baselineY(50, "Font11x18"), "conectado", "Font11x18"),
  ], progress);
}

export const screen020402UsbDisconnectedCode = SCREEN_CODE_CONNECTIVITY_USB_DISCONNECTED;

export function buildScreen020402UsbDisconnectedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 16, 16, "Icon_Info_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "USB", "Font11x18"),
    ...textAt(8, baselineY(50, "Font11x18"), "desconect", "Font11x18"),
  ], progress);
}

export const screen020501WebServerUpCode = SCREEN_CODE_CONNECTIVITY_WEB_SERVER_UP;

export function buildScreen020501WebServerUpNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 16, 16, "Icon_Info_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "WEB", "Font11x18"),
    ...textAt(2, baselineY(50, "Font11x18"), "lista", "Font11x18"),
  ], progress);
}

export const screen020502WebClientConnectedCode = SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_CONNECTED;

export function buildScreen020502WebClientConnectedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 16, 16, "Icon_Link_bits"),
    ...textAt(37, baselineY(30, "Font11x18"), "Web", "Font11x18"),
    ...textAt(20, baselineY(50, "Font11x18"), "conectado", "Font11x18"),
  ], progress);
}

export const screen020503WebClientDisconnectedCode = SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_DISCONNECTED;

export function buildScreen020503WebClientDisconnectedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return simpleNotification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 16, 16, "Icon_Link_bits"),
    { type: "drawLine", x0: 2, y0: 12, x1: 18, y1: 28 },
    { type: "drawLine", x0: 3, y0: 12, x1: 19, y1: 28 },
    ...textAt(37, baselineY(30, "Font11x18"), "Web", "Font11x18"),
    ...textAt(20, baselineY(50, "Font11x18"), "desconect", "Font11x18"),
  ], progress);
}

export const wifiSearchScreenExample = buildScreen020202WifiSearchCommands({ secondsRemaining: 10 });
export const espFirmwareScreenExample = buildScreen020307EspFirmwareScreenCommands({
  firmwareVersion: "ESP01 v1.4.2",
});
export const wifiConnectedNotificationExample = buildScreen020206WifiConnectedCommands({
  ssid: "AutoLab",
  ipAddress: "192.168.0.50",
});
