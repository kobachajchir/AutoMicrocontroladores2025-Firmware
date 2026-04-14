import {
  clear,
  drawXbm,
  drawXbmMsb,
  setWhite,
  textAt,
  textMaxAt,
  clearBox,
  baselineY,
} from "./helpers";
import {
  SCREEN_CODE_CORE_DASHBOARD,
  SCREEN_CODE_CORE_MODE_CHANGE,
  SCREEN_CODE_CORE_STARTUP,
  SCREEN_CODE_SERVICE_TEST_SCREEN,
} from "./screenCodes";
import type { OledCommand } from "./types";

export type DashboardConnectionState = "none" | "sta" | "ap";
export type CarModeLabel = "IDLE" | "FOLLOW" | "TEST" | "DEF" | "ERROR";

export interface DashboardScreenArgs {
  connection: DashboardConnectionState;
  ssid?: string;
  ipAddress?: string;
  usbActive?: boolean;
  rfActive?: boolean;
  carMode: CarModeLabel;
}

export interface ModeChangeScreenArgs {
  mode: "IDLE" | "FOLLOW" | "TEST" | "ERROR";
}

export const screen010001StartupCode = SCREEN_CODE_CORE_STARTUP;

export function buildScreen010001StartupCommands(): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...drawXbmMsb(0, 0, 128, 64, "Icon_Auto_bits"),
  ];
}

export const screen010101DashboardCode = SCREEN_CODE_CORE_DASHBOARD;

export function buildScreen010101DashboardCommands(args: DashboardScreenArgs): OledCommand[] {
  const commands: OledCommand[] = [clear()];

  if (args.connection === "sta") {
    commands.push(
      ...drawXbm(1, 2, 19, 16, "Icon_Wifi_bits"),
      ...textMaxAt(21, 0, args.ssid ?? "WiFi", 14, "Font7x10"),
      ...textMaxAt(18, 10, args.ipAddress ?? "Sin IP", 15, "Font7x10"),
    );
  } else if (args.connection === "ap") {
    commands.push(
      ...drawXbm(1, 2, 15, 16, "Icon_APWifi_bits"),
      ...textAt(21, 0, "AP name", "Font7x10"),
      ...textMaxAt(18, 10, args.ipAddress ?? "Sin IP", 15, "Font7x10"),
    );
  } else {
    commands.push(
      ...textAt(2, 0, "No ESP", "Font7x10"),
      ...textAt(2, 10, "Sin WiFi", "Font7x10"),
    );
  }

  if (args.usbActive) {
    commands.push(...drawXbm(91, 2, 16, 16, "Icon_USB_bits"));
  }

  if (args.rfActive) {
    commands.push(...drawXbm(110, 2, 17, 16, "Icon_RF_bits"));
  }

  commands.push(
    { type: "drawHorizontalLine", x: 1, y: 20, length: 125 },
    ...textAt(2, 24, "Inicio", "Font11x18"),
    ...textAt(80, 28, args.carMode, "Font7x10"),
    ...drawXbm(4, 47, 15, 16, "Icon_UserBtn_bits"),
    ...textAt(23, 51, "Menu", "Font7x10"),
    ...textAt(80, 51, "Modo", "Font7x10"),
    ...drawXbm(111, 48, 13, 13, "Icon_Encoder_bits"),
  );

  return commands;
}

export const screen010102ModeChangeCode = SCREEN_CODE_CORE_MODE_CHANGE;

export function buildScreen010102ModeChangeCommands(args: ModeChangeScreenArgs): OledCommand[] {
  return [
    clear(),
    ...textAt(42, 2, "Modo", "Font11x18"),
    ...buildScreen010102ModeOnlyPatchCommands(args),
    ...drawXbm(14, 30, 4, 7, "Arrow_Left_bits"),
    ...drawXbm(110, 30, 4, 7, "Arrow_Right_bits"),
    ...drawXbm(24, 50, 13, 13, "Icon_Encoder_bits"),
    ...textAt(40, 51, "Confirmar", "Font7x10"),
  ];
}

export function buildScreen010102ModeOnlyPatchCommands(args: ModeChangeScreenArgs): OledCommand[] {
  const modeWidth = args.mode.length * 11;
  const modeX = Math.floor((128 - modeWidth) / 2);

  return [
    ...clearBox(31, 24, 66, 18),
    ...textAt(modeX, baselineY(42, "Font11x18"), args.mode, "Font11x18"),
  ];
}

export const screen070101TestScreenCode = SCREEN_CODE_SERVICE_TEST_SCREEN;

export function buildScreen070101TestScreenCommands(): OledCommand[] {
  return [clear()];
}

export const startupScreenExample = buildScreen010001StartupCommands();
export const dashboardScreenExample = buildScreen010101DashboardCommands({
  connection: "sta",
  ssid: "AutoWiFi",
  ipAddress: "192.168.4.1",
  usbActive: true,
  rfActive: false,
  carMode: "IDLE",
});
export const modeChangeScreenExample = buildScreen010102ModeChangeCommands({ mode: "FOLLOW" });
