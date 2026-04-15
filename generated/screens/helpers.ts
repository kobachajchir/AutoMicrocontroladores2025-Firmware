import { SSD1306Color, type NotificationProgressArgs, type OledCommand, type OledFont } from "./types";

export const SSD1306_WIDTH = 128;
export const SSD1306_HEIGHT = 64;

export const FONT_WIDTH: Record<OledFont, number> = {
  Font7x10: 7,
  Font11x18: 11,
};

export const FONT_HEIGHT: Record<OledFont, number> = {
  Font7x10: 10,
  Font11x18: 18,
};

export function u8(value: number): number {
  return ((value % 256) + 256) % 256;
}

export function clear(): OledCommand {
  return { type: "clear" };
}

export function setColor(color: SSD1306Color): OledCommand {
  return { type: "setColor", color };
}

export function setWhite(): OledCommand {
  return setColor(SSD1306Color.White);
}

export function setBlack(): OledCommand {
  return setColor(SSD1306Color.Black);
}

export function setInverse(): OledCommand {
  return setColor(SSD1306Color.Inverse);
}

export function setCursor(x: number, y: number): OledCommand {
  return { type: "setCursor", x, y };
}

export function drawText(text: string, font: OledFont): OledCommand {
  return { type: "drawText", text, font };
}

export function drawTextMax(text: string, maxChars: number, font: OledFont): OledCommand {
  return { type: "drawTextMax", text, maxChars, font };
}

export function textAt(x: number, y: number, text: string, font: OledFont): OledCommand[] {
  return [setCursor(x, y), setWhite(), drawText(text, font)];
}

export function textMaxAt(x: number, y: number, text: string, maxChars: number, font: OledFont): OledCommand[] {
  return [setCursor(x, y), setWhite(), drawTextMax(text, maxChars, font)];
}

export function clearBox(x: number, y: number, w: number, h: number): OledCommand[] {
  return [setBlack(), { type: "fillRect", x, y, w, h }, setWhite()];
}

export function drawBox(x: number, y: number, w: number, h: number): OledCommand[] {
  return [setWhite(), { type: "fillRect", x, y, w, h }];
}

export function drawFrame(x: number, y: number, w: number, h: number): OledCommand[] {
  return [setWhite(), { type: "drawRect", x, y, w, h }];
}

export function drawXbm(x: number, y: number, width: number, height: number, dataRef: string): OledCommand[] {
  return [setWhite(), { type: "drawBitmap", x, y, width, height, dataRef }];
}

export function drawXbmMsb(x: number, y: number, width: number, height: number, dataRef: string): OledCommand[] {
  return [setWhite(), { type: "drawBitmapMSB", x, y, width, height, dataRef }];
}

export function baselineY(baseline: number, font: OledFont): number {
  return baseline - FONT_HEIGHT[font];
}

export function percentTextFromByte(speed: number): string {
  const pct = Math.floor((Math.max(0, Math.min(255, speed)) * 100) / 255);
  return `${String(pct).padStart(3, " ")}%`;
}

export function barWidthFromByte(speed: number, width: number): number {
  return Math.floor((Math.max(0, Math.min(255, speed)) * width) / 255);
}

export function irBarHeight(value: number, maxHeight: number): number {
  return Math.floor((Math.max(0, Math.min(4095, value)) * maxHeight) / 4095);
}

export function notificationProgressOverlayCommands(args: NotificationProgressArgs = {}): OledCommand[] {
  if (args.includeProgressOverlay === false) {
    return [];
  }

  const totalTicks = Math.max(1, args.totalTicks ?? 1);
  const elapsedTicks = Math.max(0, Math.min(totalTicks, args.elapsedTicks ?? 0));
  const width = Math.floor((elapsedTicks * SSD1306_WIDTH) / totalTicks);

  return [
    ...clearBox(0, 0, SSD1306_WIDTH, 1),
    setWhite(),
    { type: "drawHorizontalLine", x: 0, y: 0, length: width },
  ];
}

export function withNotificationProgress(commands: OledCommand[], args: NotificationProgressArgs = {}): OledCommand[] {
  return [...commands, ...notificationProgressOverlayCommands(args)];
}
