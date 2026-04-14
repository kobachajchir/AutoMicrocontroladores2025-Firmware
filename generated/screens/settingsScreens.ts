import { baselineY, clear, drawXbm, drawXbmMsb, setWhite, textAt } from "./helpers";
import {
  SCREEN_CODE_SETTINGS_ABOUT_PROJECT,
  SCREEN_CODE_SETTINGS_ABOUT_REPO,
  SCREEN_CODE_SETTINGS_WARNING_TIME,
} from "./screenCodes";
import type { OledCommand } from "./types";

export const screen050201AboutProjectCode = SCREEN_CODE_SETTINGS_ABOUT_PROJECT;

export function buildScreen050201AboutProjectCommands(): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(40, baselineY(19, "Font11x18"), "Auto", "Font11x18"),
    ...textAt(0, baselineY(30, "Font7x10"), "Microcontroladores", "Font7x10"),
    ...drawXbm(20, 2, 15, 16, "Icon_Info_bits"),
    ...textAt(60, baselineY(57, "Font7x10"), "Github", "Font7x10"),
    ...drawXbm(105, 49, 4, 7, "Arrow_Right_bits"),
    ...textAt(20, baselineY(42, "Font7x10"), "Koba Chajchir", "Font7x10"),
    ...textAt(4, baselineY(62, "Font11x18"), "2026", "Font11x18"),
  ];
}

export const screen050202AboutRepoCode = SCREEN_CODE_SETTINGS_ABOUT_REPO;

export function buildScreen050202AboutRepoCommands(): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...drawXbm(12, 50, 4, 7, "Arrow_Left_bits"),
    ...textAt(11, baselineY(13, "Font7x10"), "Repo", "Font7x10"),
    ...textAt(2, baselineY(25, "Font7x10"), "Firmware", "Font7x10"),
    ...textAt(2, baselineY(37, "Font7x10"), "STM32", "Font7x10"),
    ...drawXbmMsb(64, 0, 64, 64, "QRCode_Github_bits"),
  ];
}

export const screen050301WarningTimeCode = SCREEN_CODE_SETTINGS_WARNING_TIME;

export function buildScreen050301WarningTimeCommands(args: { seconds: number }): OledCommand[] {
  return [
    clear(),
    setWhite(),
    ...textAt(21, baselineY(17, "Font11x18"), "Tiempo de", "Font11x18"),
    ...textAt(36, baselineY(33, "Font11x18"), "avisos", "Font11x18"),
    ...textAt(27, baselineY(53, "Font11x18"), `${args.seconds} segs.`, "Font11x18"),
    ...drawXbm(112, 48, 13, 13, "Icon_Encoder_bits"),
  ];
}

export const aboutProjectScreenExample = buildScreen050201AboutProjectCommands();
export const aboutRepoScreenExample = buildScreen050202AboutRepoCommands();
export const warningTimeScreenExample = buildScreen050301WarningTimeCommands({ seconds: 10 });
