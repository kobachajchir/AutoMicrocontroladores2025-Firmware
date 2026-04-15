import { barWidthFromByte, baselineY, clear, clearBox, drawBox, drawFrame, textAt } from "./helpers";
import { SCREEN_CODE_MOTOR_TEST } from "./screenCodes";
import type { OledCommand } from "./types";

export type MotorSelection = "left" | "right" | "both" | "none" | "unknown";
export type MotorDirection = "forward" | "backward";

export interface MotorTestArgs {
  selectedMotor: MotorSelection;
  direction: MotorDirection;
  leftSpeed: number;
  rightSpeed: number;
  movementEnabled: boolean;
}

function completeMotorLabel(selectedMotor: MotorSelection): string {
  if (selectedMotor === "left") return "Motor Izquierdo";
  if (selectedMotor === "right") return "Motor Derecho";
  if (selectedMotor === "both" || selectedMotor === "none") return "Ambos Motores";
  return "Motor ?";
}

function patchMotorLabel(selectedMotor: MotorSelection): string {
  if (selectedMotor === "left") return "Probando Motor 1";
  if (selectedMotor === "right") return "Probando Motor 2";
  return "Probando Ambos";
}

function selectedSpeedForComplete(args: MotorTestArgs): number {
  if (args.selectedMotor === "right") return args.rightSpeed;
  if (args.selectedMotor === "left") return args.leftSpeed;
  return Math.floor((args.leftSpeed + args.rightSpeed) / 2);
}

function selectedSpeedForPatch(args: MotorTestArgs): number {
  return args.selectedMotor === "right" ? args.rightSpeed : args.leftSpeed;
}

function speedText(speed: number, movementEnabled: boolean): string {
  if (!movementEnabled) return "PARADO";
  const pct = Math.floor((Math.max(0, Math.min(255, speed)) * 100) / 255);
  return `${String(pct).padStart(3, " ")}%`;
}

export const screen040101MotorTestCode = SCREEN_CODE_MOTOR_TEST;

export function buildScreen040101MotorTestCommands(args: MotorTestArgs): OledCommand[] {
  const speed = selectedSpeedForComplete(args);
  const commands: OledCommand[] = [
    clear(),
    ...textAt(3, baselineY(11, "Font7x10"), completeMotorLabel(args.selectedMotor), "Font7x10"),
    ...textAt(41, baselineY(29, "Font7x10"), args.direction === "forward" ? "Adelante" : "Atras", "Font7x10"),
    ...drawFrame(5, 34, 116, 11),
  ];

  if (args.movementEnabled) {
    commands.push(...drawBox(5, 34, barWidthFromByte(speed, 116), 11));
  }

  commands.push(...textAt(50, baselineY(57, "Font7x10"), speedText(speed, args.movementEnabled), "Font7x10"));
  return commands;
}

export function buildScreen040101MotorTestPatchCommands(args: MotorTestArgs): OledCommand[] {
  const speed = selectedSpeedForPatch(args);
  const commands: OledCommand[] = [
    ...clearBox(0, baselineY(11, "Font7x10"), 128, 10),
    ...textAt(3, baselineY(11, "Font7x10"), patchMotorLabel(args.selectedMotor), "Font7x10"),
    ...clearBox(0, baselineY(29, "Font7x10"), 128, 10),
    ...textAt(41, baselineY(29, "Font7x10"), args.direction === "forward" ? "Adelante" : "Atras", "Font7x10"),
    ...clearBox(5, 34, 116, 11),
    ...drawFrame(5, 34, 116, 11),
  ];

  if (args.movementEnabled) {
    commands.push(...drawBox(5, 34, barWidthFromByte(speed, 116), 11));
  }

  commands.push(
    ...clearBox(0, baselineY(57, "Font7x10"), 128, 10),
    ...textAt(50, baselineY(57, "Font7x10"), speedText(speed, args.movementEnabled), "Font7x10"),
  );

  return commands;
}

export const motorTestScreenExample = buildScreen040101MotorTestCommands({
  selectedMotor: "both",
  direction: "forward",
  leftSpeed: 128,
  rightSpeed: 128,
  movementEnabled: true,
});
