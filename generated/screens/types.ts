export type ScreenCode = number;

export enum SSD1306Color {
  Black = "Black",
  White = "White",
  Inverse = "Inverse",
}

export type OledFont = "Font7x10" | "Font11x18";

export type OledCommand =
  | { type: "clear" }
  | { type: "fill" }
  | { type: "setColor"; color: SSD1306Color }
  | { type: "setInverted"; value: boolean }
  | { type: "setCursor"; x: number; y: number }
  | { type: "drawText"; text: string; font: OledFont }
  | { type: "drawTextMax"; text: string; maxChars: number; font: OledFont }
  | { type: "drawPixel"; x: number; y: number }
  | { type: "drawLine"; x0: number; y0: number; x1: number; y1: number }
  | { type: "drawHorizontalLine"; x: number; y: number; length: number }
  | { type: "drawVerticalLine"; x: number; y: number; length: number }
  | { type: "drawRect"; x: number; y: number; w: number; h: number }
  | { type: "fillRect"; x: number; y: number; w: number; h: number }
  | { type: "drawTriangle"; x1: number; y1: number; x2: number; y2: number; x3: number; y3: number }
  | { type: "drawFillTriangle"; x1: number; y1: number; x2: number; y2: number; x3: number; y3: number }
  | { type: "drawCircle"; x: number; y: number; radius: number }
  | { type: "fillCircle"; x: number; y: number; radius: number }
  | { type: "drawCircleQuads"; x: number; y: number; radius: number; quads: number }
  | { type: "drawArc"; x: number; y: number; radius: number; startAngle: number; sweep: number }
  | { type: "drawProgressBar"; x: number; y: number; barWidth: number; barHeight: number; progress: number }
  | { type: "drawBitmap"; x: number; y: number; width: number; height: number; dataRef: string }
  | { type: "drawBitmapMSB"; x: number; y: number; width: number; height: number; dataRef: string }
  | { type: "polyline"; vertices: Array<{ x: number; y: number }> };

export interface NotificationProgressArgs {
  elapsedTicks?: number;
  totalTicks?: number;
  includeProgressOverlay?: boolean;
}

export interface MenuScreenItem {
  label: string;
  iconRef?: string;
  visible?: boolean;
  screenCode?: ScreenCode;
}
