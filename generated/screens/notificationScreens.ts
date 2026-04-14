import { baselineY, clear, drawXbm, setInverse, setWhite, textAt, withNotificationProgress } from "./helpers";
import {
  SCREEN_CODE_DIAG_COMMAND_RECEIVED,
  SCREEN_CODE_DIAG_CONTROLLER_CONNECTED,
  SCREEN_CODE_DIAG_CONTROLLER_DISCONNECTED,
  SCREEN_CODE_DIAG_ESP_CONN_FAILED,
  SCREEN_CODE_DIAG_ESP_CONN_SUCCEEDED,
  SCREEN_CODE_DIAG_PING_RECEIVED,
} from "./screenCodes";
import type { NotificationProgressArgs, OledCommand } from "./types";

function notification(commands: OledCommand[], progress?: NotificationProgressArgs): OledCommand[] {
  return withNotificationProgress(commands, progress);
}

export const screen060101ControllerConnectedCode = SCREEN_CODE_DIAG_CONTROLLER_CONNECTED;

export function buildScreen060101ControllerConnectedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return notification([
    clear(),
    setWhite(),
    ...drawXbm(46, 4, 37, 27, "Icon_Controller_bits"),
    ...textAt(28, baselineY(48, "Font11x18"), "Control", "Font11x18"),
    ...textAt(16, baselineY(63, "Font11x18"), "conectado", "Font11x18"),
  ], progress);
}

export const screen060102ControllerDisconnectedCode = SCREEN_CODE_DIAG_CONTROLLER_DISCONNECTED;

export function buildScreen060102ControllerDisconnectedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return notification([
    clear(),
    setWhite(),
    ...textAt(28, baselineY(48, "Font11x18"), "Control", "Font11x18"),
    ...textAt(8, baselineY(63, "Font11x18"), "desconectado", "Font11x18"),
    { type: "drawLine", x0: 46, y0: 2, x1: 84, y1: 30 },
    { type: "drawLine", x0: 47, y0: 2, x1: 85, y1: 30 },
    { type: "drawLine", x0: 47, y0: 1, x1: 85, y1: 29 },
    setInverse(),
    ...drawXbm(46, 4, 37, 27, "Icon_Controller_bits"),
    setWhite(),
  ], progress);
}

export const screen060201CommandReceivedCode = SCREEN_CODE_DIAG_COMMAND_RECEIVED;

export function buildScreen060201CommandReceivedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return notification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 19, 16, "Icon_Wifi_100_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "ECHO", "Font11x18"),
    ...textAt(2, baselineY(50, "Font11x18"), "recibido", "Font11x18"),
  ], progress);
}

export const screen060202PingReceivedCode = SCREEN_CODE_DIAG_PING_RECEIVED;

export function buildScreen060202PingReceivedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return notification([
    clear(),
    setWhite(),
    ...drawXbm(2, 12, 16, 16, "Icon_Info_bits"),
    ...textAt(27, baselineY(30, "Font11x18"), "PING", "Font11x18"),
    ...textAt(2, baselineY(50, "Font11x18"), "recibido", "Font11x18"),
  ], progress);
}

export const screen060301EspConnSucceededCode = SCREEN_CODE_DIAG_ESP_CONN_SUCCEEDED;

export function buildScreen060301EspConnectionSucceededCommands(): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(27, baselineY(36, "Font11x18"), "Conexion", "Font11x18"),
    ...textAt(32, baselineY(49, "Font11x18"), "exitosa", "Font11x18"),
    ...drawXbm(111, 47, 13, 13, "Icon_Encoder_bits"),
    ...drawXbm(57, 6, 14, 16, "Icon_Checked_bits"),
  ];
}

export const screen060302EspConnFailedCode = SCREEN_CODE_DIAG_ESP_CONN_FAILED;

export function buildScreen060302EspConnectionFailedCommands(): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(27, baselineY(36, "Font11x18"), "Conexion", "Font11x18"),
    ...textAt(32, baselineY(49, "Font11x18"), "fallida", "Font11x18"),
    ...drawXbm(111, 47, 13, 13, "Icon_Encoder_bits"),
    ...drawXbm(58, 9, 11, 16, "Icon_Crossed_bits"),
  ];
}

export const controllerConnectedNotificationExample = buildScreen060101ControllerConnectedNotificationCommands();
export const commandReceivedNotificationExample = buildScreen060201CommandReceivedNotificationCommands();
