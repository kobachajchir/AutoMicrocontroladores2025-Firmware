import {
  baselineY,
  clear,
  clearBox,
  drawBox,
  drawXbm,
  irBarHeight,
  setBlack,
  setWhite,
  textAt,
  u8,
} from "./helpers";
import {
  SCREEN_CODE_SENSORS_IR_VALUES,
  SCREEN_CODE_SENSORS_MPU_VALUES,
  SCREEN_CODE_SENSORS_RADAR,
} from "./screenCodes";
import type { OledCommand } from "./types";

export interface IrValuesArgs {
  irValues: number[];
}

export interface MpuValuesArgs {
  accelX: number;
  accelY: number;
  accelZ: number;
  gyroX: number;
  gyroY: number;
  gyroZ: number;
}

export interface LegacyMpuTableArgs extends MpuValuesArgs {
  calibrated: boolean;
  angleXMd: number;
  angleYMd: number;
  angleZMd: number;
}

function sensorValueAt(values: number[], index: number): number {
  return values[index] ?? 0;
}

function normalizeAngleFromMd(angleMd: number): number {
  let angle = Math.trunc(angleMd / 1000) % 360;
  if (angle > 180) angle -= 360;
  if (angle < -180) angle += 360;
  return angle;
}

export const screen030201IrValuesCode = SCREEN_CODE_SENSORS_IR_VALUES;

export function buildScreen030201IrValuesCommands(args: IrValuesArgs): OledCommand[] {
  const commands: OledCommand[] = [
    clear(),
    setWhite(),
    { type: "drawLine", x0: 0, y0: 9, x1: 127, y1: 9 },
    ...textAt(0, u8(7 - 10), "IR1", "Font7x10"),
    ...textAt(16, u8(7 - 10), "IR2", "Font7x10"),
    ...textAt(32, u8(7 - 10), "IR3", "Font7x10"),
    ...textAt(48, u8(7 - 10), "IR4", "Font7x10"),
    ...textAt(63, u8(7 - 10), "IR5", "Font7x10"),
    ...textAt(79, u8(7 - 10), "IR6", "Font7x10"),
    ...textAt(95, u8(7 - 10), "IR7", "Font7x10"),
    ...textAt(111, u8(7 - 10), "IR8", "Font7x10"),
    setWhite(),
  ];

  const barX = [0, 16, 32, 48, 64, 80, 96, 112];
  for (let i = 0; i < 8; i++) {
    const barHeight = irBarHeight(sensorValueAt(args.irValues, i), 52);
    const barY = 12 + (52 - barHeight);
    commands.push(
      setBlack(),
      { type: "fillRect", x: barX[i], y: 12, w: 14, h: 52 },
      setWhite(),
      { type: "fillRect", x: barX[i], y: barY, w: 14, h: barHeight },
    );
  }

  return commands;
}

export function buildScreen030201LegacyIrGraphCommands(args: IrValuesArgs): OledCommand[] {
  const commands: OledCommand[] = [
    clear(),
    ...textAt(1, 0, "IR1", "Font7x10"),
    ...textAt(16, 0, "IR2", "Font7x10"),
    ...textAt(31, 0, "IR3", "Font7x10"),
    ...textAt(46, 0, "IR4", "Font7x10"),
    ...textAt(61, 0, "IR5", "Font7x10"),
    ...textAt(76, 0, "IR6", "Font7x10"),
    ...textAt(91, 0, "IR7", "Font7x10"),
    ...textAt(106, 0, "IR8", "Font7x10"),
    { type: "drawLine", x0: 0, y0: 11, x1: 127, y1: 11 },
  ];

  const barX = [5, 20, 35, 50, 65, 80, 95, 110];
  for (let i = 0; i < 8; i++) {
    const h = irBarHeight(sensorValueAt(args.irValues, i), 51);
    const y = 13 + (51 - h);
    commands.push(...clearBox(barX[i], 13, 12, 51), ...drawBox(barX[i], y, 12, h));
  }

  return commands;
}

export const screen030301MpuValuesCode = SCREEN_CODE_SENSORS_MPU_VALUES;

export function buildScreen030301MpuValuesCommands(args: MpuValuesArgs): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(5, u8(16 - 18), "Sensor MPU", "Font11x18"),
    ...textAt(5, baselineY(32, "Font7x10"), "AX", "Font7x10"),
    ...textAt(5, baselineY(42, "Font7x10"), "AY", "Font7x10"),
    ...textAt(5, baselineY(52, "Font7x10"), "AZ", "Font7x10"),
    ...textAt(68, baselineY(33, "Font7x10"), "GX", "Font7x10"),
    ...textAt(68, baselineY(43, "Font7x10"), "GY", "Font7x10"),
    ...textAt(68, baselineY(53, "Font7x10"), "GZ", "Font7x10"),
    ...clearBox(24, baselineY(32, "Font7x10"), 42, 10),
    ...textAt(24, baselineY(32, "Font7x10"), String(args.accelX), "Font7x10"),
    ...clearBox(24, baselineY(42, "Font7x10"), 42, 10),
    ...textAt(24, baselineY(42, "Font7x10"), String(args.accelY), "Font7x10"),
    ...clearBox(24, baselineY(52, "Font7x10"), 42, 10),
    ...textAt(24, baselineY(52, "Font7x10"), String(args.accelZ), "Font7x10"),
    ...clearBox(88, baselineY(33, "Font7x10"), 42, 10),
    ...textAt(88, baselineY(33, "Font7x10"), String(args.gyroX), "Font7x10"),
    ...clearBox(88, baselineY(43, "Font7x10"), 42, 10),
    ...textAt(88, baselineY(43, "Font7x10"), String(args.gyroY), "Font7x10"),
    ...clearBox(88, baselineY(53, "Font7x10"), 42, 10),
    ...textAt(88, baselineY(53, "Font7x10"), String(args.gyroZ), "Font7x10"),
  ];
}

export function buildScreen030301LegacyMpuTableCommands(args: LegacyMpuTableArgs): OledCommand[] {
  return [
    clear(),
    ...textAt(5, baselineY(14, "Font7x10"), "Sensor MPU", "Font7x10"),
    ...clearBox(70, baselineY(14, "Font7x10"), 57, 10),
    ...textAt(70, baselineY(14, "Font7x10"), args.calibrated ? "Calib." : "No calib.", "Font7x10"),
    ...textAt(20, baselineY(28, "Font7x10"), "A", "Font7x10"),
    ...textAt(50, baselineY(28, "Font7x10"), "G", "Font7x10"),
    ...textAt(100, baselineY(28, "Font7x10"), "DEG", "Font7x10"),
    ...textAt(5, baselineY(38, "Font7x10"), "X", "Font7x10"),
    ...textAt(5, baselineY(48, "Font7x10"), "Y", "Font7x10"),
    ...textAt(5, baselineY(58, "Font7x10"), "Z", "Font7x10"),
    ...clearBox(20, baselineY(38, "Font7x10"), 115, 10),
    ...textAt(20, baselineY(38, "Font7x10"), String(args.accelX), "Font7x10"),
    ...textAt(50, baselineY(38, "Font7x10"), String(args.gyroX), "Font7x10"),
    ...textAt(100, baselineY(38, "Font7x10"), String(normalizeAngleFromMd(args.angleXMd)), "Font7x10"),
    ...clearBox(20, baselineY(48, "Font7x10"), 115, 10),
    ...textAt(20, baselineY(48, "Font7x10"), String(args.accelY), "Font7x10"),
    ...textAt(50, baselineY(48, "Font7x10"), String(args.gyroY), "Font7x10"),
    ...textAt(100, baselineY(48, "Font7x10"), String(normalizeAngleFromMd(args.angleYMd)), "Font7x10"),
    ...clearBox(20, baselineY(58, "Font7x10"), 115, 10),
    ...textAt(20, baselineY(58, "Font7x10"), String(args.accelZ), "Font7x10"),
    ...textAt(50, baselineY(58, "Font7x10"), String(args.gyroZ), "Font7x10"),
    ...textAt(100, baselineY(58, "Font7x10"), String(normalizeAngleFromMd(args.angleZMd)), "Font7x10"),
  ];
}

export const screen030401RadarCode = SCREEN_CODE_SENSORS_RADAR;

export function buildScreen030401RadarCommands(): OledCommand[] {
  return [
    clear(),
    setWhite(),
    { type: "drawCircle", x: 63, y: 60, radius: 60 },
    { type: "drawCircle", x: 63, y: 60, radius: 40 },
    ...drawXbm(40, 48, 48, 13, "Icon_Car_bits"),
  ];
}

export function buildScreen030401RadarObjectsPatchCommands(): OledCommand[] {
  return [];
}

export const irValuesScreenExample = buildScreen030201IrValuesCommands({
  irValues: [0, 512, 1024, 1536, 2048, 2560, 3072, 4095],
});
export const mpuValuesScreenExample = buildScreen030301MpuValuesCommands({
  accelX: 120,
  accelY: -40,
  accelZ: 980,
  gyroX: 3,
  gyroY: -2,
  gyroZ: 0,
});
