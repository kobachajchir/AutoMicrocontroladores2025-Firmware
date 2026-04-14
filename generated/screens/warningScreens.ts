import { baselineY, clear, drawXbm, drawFrame, setWhite, textAt, withNotificationProgress } from "./helpers";
import {
  SCREEN_CODE_WARNING_LOCKED,
  SCREEN_CODE_WARNING_PERMISSION_DENIED,
  SCREEN_CODE_WARNING_PIN_BLOCKED,
  SCREEN_CODE_WARNING_PIN_DENIED,
  SCREEN_CODE_WARNING_PIN_ENTRY,
  SCREEN_CODE_WARNING_PIN_INCORRECT,
  SCREEN_CODE_WARNING_PIN_MODIFIED,
  SCREEN_CODE_WARNING_PIN_TIMEOUT,
  SCREEN_CODE_WARNING_PIN_WAITING,
} from "./screenCodes";
import type { NotificationProgressArgs, OledCommand, ScreenCode } from "./types";

export type LockScreenState = "locked" | "pinIncorrect" | "pinModified" | "enterPin";
export type PermissionPinState = "input" | "waiting" | "denied" | "timeout" | "blocked";

export interface PermissionPinFormArgs {
  digits: number[];
  pinIndex?: number;
}

function lockTextForState(state: LockScreenState): { screenCode: ScreenCode; message: string; x: number } {
  if (state === "pinIncorrect") return { screenCode: SCREEN_CODE_WARNING_PIN_INCORRECT, message: "PIN incorrecto", x: 22 };
  if (state === "pinModified") return { screenCode: SCREEN_CODE_WARNING_PIN_MODIFIED, message: "PIN modificado", x: 22 };
  if (state === "enterPin") return { screenCode: SCREEN_CODE_WARNING_PIN_ENTRY, message: "Ingrese PIN", x: 28 };
  return { screenCode: SCREEN_CODE_WARNING_LOCKED, message: "Pantalla bloqueada", x: 10 };
}

function pinDigit(digits: number[], index: number): string {
  return String(Math.max(0, Math.min(9, digits[index] ?? 0)));
}

function buildPermissionPinFormCommands(args: PermissionPinFormArgs & { waiting?: boolean; confirmText: string }): OledCommand[] {
  const frameX = [23, 44, 65, 86];
  const textX = [27, 48, 69, 90];
  const commands: OledCommand[] = [
    setWhite(),
    ...drawXbm(33, 0, 13, 16, "Icon_Lock_bits"),
    ...textAt(49, baselineY(14, "Font7x10"), "PIN", "Font7x10"),
  ];

  for (let i = 0; i < 4; i++) {
    commands.push(setWhite(), ...drawFrame(frameX[i], 18, 16, 17));
    if (!args.waiting && i === (args.pinIndex ?? 0)) {
      commands.push(...drawFrame(frameX[i] - 2, 16, 20, 21));
    }
    commands.push(...textAt(textX[i], 17, args.waiting ? "*" : pinDigit(args.digits, i), "Font11x18"));
  }

  commands.push(
    setWhite(),
    ...drawXbm(27, 47, 15, 16, "Icon_UserBtn_bits"),
    ...textAt(45, baselineY(60, "Font7x10"), args.confirmText, "Font7x10"),
  );

  return commands;
}

export const screen080101LockCode = SCREEN_CODE_WARNING_LOCKED;
export const screen080102PinIncorrectCode = SCREEN_CODE_WARNING_PIN_INCORRECT;
export const screen080103PinModifiedCode = SCREEN_CODE_WARNING_PIN_MODIFIED;
export const screen080104PinEntryCode = SCREEN_CODE_WARNING_PIN_ENTRY;

export function buildLockScreenCommands(args: { state: LockScreenState }): OledCommand[] {
  const text = lockTextForState(args.state);
  return [
    clear(),
    setWhite(),
    ...drawXbm(57, 17, 13, 16, "Icon_Lock_bits"),
    ...textAt(text.x, baselineY(48, "Font7x10"), text.message, "Font7x10"),
  ];
}

export function buildScreen080101LockedCommands(): OledCommand[] {
  return buildLockScreenCommands({ state: "locked" });
}

export function buildScreen080102PinIncorrectLockCommands(): OledCommand[] {
  return buildLockScreenCommands({ state: "pinIncorrect" });
}

export function buildScreen080103PinModifiedLockCommands(): OledCommand[] {
  return buildLockScreenCommands({ state: "pinModified" });
}

export function buildScreen080104LockEnterPinCommands(): OledCommand[] {
  return buildLockScreenCommands({ state: "enterPin" });
}

export function buildScreen080104PermissionPinEntryCommands(args: PermissionPinFormArgs): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...buildPermissionPinFormCommands({ ...args, confirmText: "Confirmar" }),
  ];
}

export const screen080105PinWaitingCode = SCREEN_CODE_WARNING_PIN_WAITING;

export function buildScreen080105PermissionPinWaitingCommands(args: PermissionPinFormArgs): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...buildPermissionPinFormCommands({ ...args, waiting: true, confirmText: "Validando" }),
  ];
}

export const screen080106PinDeniedCode = SCREEN_CODE_WARNING_PIN_DENIED;

export function buildScreen080106PermissionPinDeniedCommands(args: { attemptsLeft: number }): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(15, 14, "Rechazado", "Font11x18"),
    ...textAt(4, 42, `Intentos: ${args.attemptsLeft}`, "Font7x10"),
    ...textAt(4, 58, "OK: reintentar", "Font7x10"),
  ];
}

export const screen080107PinTimeoutCode = SCREEN_CODE_WARNING_PIN_TIMEOUT;

export function buildScreen080107PermissionPinTimeoutCommands(): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(26, 14, "Sin resp.", "Font11x18"),
    ...textAt(4, 42, "ESP no valido", "Font7x10"),
    ...textAt(4, 58, "OK: volver", "Font7x10"),
  ];
}

export const screen080108PinBlockedCode = SCREEN_CODE_WARNING_PIN_BLOCKED;

export function buildScreen080108PermissionPinBlockedCommands(): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(18, 14, "Bloqueado", "Font11x18"),
    ...textAt(4, 42, "Demasiados fallos", "Font7x10"),
    ...textAt(4, 58, "OK: volver", "Font7x10"),
  ];
}

export const screen080109PermissionDeniedCode = SCREEN_CODE_WARNING_PERMISSION_DENIED;

export function buildScreen080109PermissionDeniedNotificationCommands(progress?: NotificationProgressArgs): OledCommand[] {
  return withNotificationProgress([
    clear(),
    setWhite(),
    ...drawXbm(57, 3, 13, 16, "Icon_Lock_bits"),
    ...textAt(13, baselineY(34, "Font7x10"), "PIN no verificado", "Font7x10"),
    ...textAt(4, baselineY(52, "Font7x10"), "Accion restringida", "Font7x10"),
  ], progress);
}

export const lockScreenExample = buildLockScreenCommands({ state: "locked" });
export const permissionPinEntryExample = buildScreen080104PermissionPinEntryCommands({
  digits: [1, 2, 3, 4],
  pinIndex: 2,
});
