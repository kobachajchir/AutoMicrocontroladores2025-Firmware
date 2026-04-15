import { clear, clearBox, drawFrame, drawXbm, textAt } from "./helpers";
import {
  SCREEN_CODE_CONNECTIVITY_ESP_MENU,
  SCREEN_CODE_CONNECTIVITY_WIFI_MENU,
  SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS,
  SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING,
  SCREEN_CODE_CONNECTIVITY_WIFI_STATUS,
  SCREEN_CODE_CORE_DASHBOARD,
  SCREEN_CODE_CORE_MAIN_MENU,
  SCREEN_CODE_SENSORS_IR_VALUES,
  SCREEN_CODE_SENSORS_MENU,
  SCREEN_CODE_SENSORS_MPU_VALUES,
  SCREEN_CODE_MOTOR_TEST,
  SCREEN_CODE_SETTINGS_ABOUT_PROJECT,
  SCREEN_CODE_SETTINGS_MENU,
  SCREEN_CODE_CONNECTIVITY_ESP_CHECKING,
  SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST,
  SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT,
} from "./screenCodes";
import type { MenuScreenItem, OledCommand, ScreenCode } from "./types";

export interface MenuScreenArgs {
  screenCode: ScreenCode;
  items: MenuScreenItem[];
  selectedIndex: number;
  firstVisibleIndex?: number;
}

const MENU_VISIBLE_ITEMS = 3;
const MENU_ITEM_Y0 = 4;
const MENU_ITEM_SPACING = 21;

function visibleItems(items: MenuScreenItem[]): Array<MenuScreenItem & { sourceIndex: number }> {
  return items
    .map((item, sourceIndex) => ({ ...item, sourceIndex }))
    .filter((item) => item.visible !== false);
}

function menuIconSize(iconRef?: string): { width: number; height: number } {
  if (iconRef === "Icon_Wifi_bits") return { width: 19, height: 16 };
  if (iconRef === "Icon_Sensors_bits") return { width: 14, height: 16 };
  return { width: 16, height: 16 };
}

export function buildMenuItemCommands(item: MenuScreenItem, y: number, selected: boolean): OledCommand[] {
  const commands: OledCommand[] = [...drawFrame(0, y - 3, 128, 20)];

  if (item.iconRef) {
    const icon = menuIconSize(item.iconRef);
    commands.push(...drawXbm(6, y - 1, icon.width, icon.height, item.iconRef));
  }

  commands.push(...textAt(27, y + 3, item.label, "Font7x10"));

  if (selected) {
    commands.push(...drawXbm(117, y, 7, 16, "Icon_Cursor_bits"));
  } else {
    commands.push(...clearBox(117, y, 7, 16));
  }

  return commands;
}

export function buildMenuScreenCommands(args: MenuScreenArgs): OledCommand[] {
  const menuItems = visibleItems(args.items);
  const firstVisibleIndex = args.firstVisibleIndex ?? 0;
  const commands: OledCommand[] = [clear()];

  for (let slot = 0; slot < MENU_VISIBLE_ITEMS; slot++) {
    const item = menuItems[firstVisibleIndex + slot];
    if (!item) break;

    const y = MENU_ITEM_Y0 + slot * MENU_ITEM_SPACING;
    commands.push(...buildMenuItemCommands(item, y, item.sourceIndex === args.selectedIndex));
  }

  return commands;
}

export const mainMenuItems: MenuScreenItem[] = [
  { label: "Wifi", iconRef: "Icon_Wifi_bits", screenCode: SCREEN_CODE_CONNECTIVITY_WIFI_MENU },
  { label: "Sensores", iconRef: "Icon_Sensors_bits", screenCode: SCREEN_CODE_SENSORS_MENU },
  { label: "Config.", iconRef: "Icon_Config_bits", screenCode: SCREEN_CODE_SETTINGS_MENU },
  { label: "Volver", iconRef: "Icon_Volver_bits", screenCode: SCREEN_CODE_CORE_DASHBOARD },
];

export const wifiMenuItems: MenuScreenItem[] = [
  { label: "Info AP", iconRef: "Icon_Info_bits", screenCode: SCREEN_CODE_CONNECTIVITY_WIFI_STATUS },
  { label: "Buscar APs", iconRef: "Icon_Refrescar_bits", screenCode: SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING },
  { label: "Conexion ESP", iconRef: "Icon_Link_bits", screenCode: SCREEN_CODE_CONNECTIVITY_ESP_MENU },
  { label: "Volver", iconRef: "Icon_Volver_bits", screenCode: SCREEN_CODE_CORE_MAIN_MENU },
];

export const espMenuItems: MenuScreenItem[] = [
  { label: "Chk Conexion", iconRef: "Icon_Link_bits", screenCode: SCREEN_CODE_CONNECTIVITY_ESP_CHECKING },
  { label: "Firmware", iconRef: "Icon_Info_bits", screenCode: SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST },
  { label: "Reset ESP", iconRef: "Icon_Refrescar_bits", screenCode: SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT },
  { label: "Volver", iconRef: "Icon_Volver_bits", screenCode: SCREEN_CODE_CORE_MAIN_MENU },
];

export const sensorsMenuItems: MenuScreenItem[] = [
  { label: "Valores IR", iconRef: "Icon_Tool_bits", screenCode: SCREEN_CODE_SENSORS_IR_VALUES },
  { label: "Valores MPU", iconRef: "Icon_Tool_bits", screenCode: SCREEN_CODE_SENSORS_MPU_VALUES },
  { label: "Test motores", iconRef: "Icon_Tool_bits", screenCode: SCREEN_CODE_MOTOR_TEST },
  { label: "Volver", iconRef: "Icon_Volver_bits", screenCode: SCREEN_CODE_CORE_MAIN_MENU },
];

export const settingsMenuItems: MenuScreenItem[] = [
  { label: "Preferencias", iconRef: "Icon_Prefs_bits" },
  { label: "Acerca de", iconRef: "Icon_Info_bits", screenCode: SCREEN_CODE_SETTINGS_ABOUT_PROJECT },
  { label: "Volver", iconRef: "Icon_Volver_bits", screenCode: SCREEN_CODE_CORE_MAIN_MENU },
];

export const screen010201MainMenuCode = SCREEN_CODE_CORE_MAIN_MENU;

export function buildScreen010201MainMenuCommands(args: { selectedIndex?: number; firstVisibleIndex?: number; sensoresVisible?: boolean } = {}): OledCommand[] {
  const items = mainMenuItems.map((item) =>
    item.label === "Sensores" ? { ...item, visible: args.sensoresVisible ?? true } : item,
  );

  return buildMenuScreenCommands({
    screenCode: SCREEN_CODE_CORE_MAIN_MENU,
    items,
    selectedIndex: args.selectedIndex ?? 0,
    firstVisibleIndex: args.firstVisibleIndex,
  });
}

export const screen020101WifiMenuCode = SCREEN_CODE_CONNECTIVITY_WIFI_MENU;

export function buildScreen020101WifiMenuCommands(args: { selectedIndex?: number; firstVisibleIndex?: number } = {}): OledCommand[] {
  return buildMenuScreenCommands({
    screenCode: SCREEN_CODE_CONNECTIVITY_WIFI_MENU,
    items: wifiMenuItems,
    selectedIndex: args.selectedIndex ?? 0,
    firstVisibleIndex: args.firstVisibleIndex,
  });
}

export const screen020301EspMenuCode = SCREEN_CODE_CONNECTIVITY_ESP_MENU;

export function buildScreen020301EspMenuCommands(args: { selectedIndex?: number; firstVisibleIndex?: number } = {}): OledCommand[] {
  return buildMenuScreenCommands({
    screenCode: SCREEN_CODE_CONNECTIVITY_ESP_MENU,
    items: espMenuItems,
    selectedIndex: args.selectedIndex ?? 0,
    firstVisibleIndex: args.firstVisibleIndex,
  });
}

export const screen030101SensorsMenuCode = SCREEN_CODE_SENSORS_MENU;

export function buildScreen030101SensorsMenuCommands(args: { selectedIndex?: number; firstVisibleIndex?: number } = {}): OledCommand[] {
  return buildMenuScreenCommands({
    screenCode: SCREEN_CODE_SENSORS_MENU,
    items: sensorsMenuItems,
    selectedIndex: args.selectedIndex ?? 0,
    firstVisibleIndex: args.firstVisibleIndex,
  });
}

export const screen050101SettingsMenuCode = SCREEN_CODE_SETTINGS_MENU;

export function buildScreen050101SettingsMenuCommands(args: { selectedIndex?: number; firstVisibleIndex?: number } = {}): OledCommand[] {
  return buildMenuScreenCommands({
    screenCode: SCREEN_CODE_SETTINGS_MENU,
    items: settingsMenuItems,
    selectedIndex: args.selectedIndex ?? 0,
    firstVisibleIndex: args.firstVisibleIndex,
  });
}

export const screen020203WifiResultsMenuCode = SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS;

export function buildScreen020203WifiResultsMenuCommands(args: {
  networkSsids?: string[];
  selectedIndex?: number;
  firstVisibleIndex?: number;
} = {}): OledCommand[] {
  const networkItems = (args.networkSsids?.length ? args.networkSsids : ["Sin redes"]).map((ssid) => ({
    label: ssid,
    iconRef: "Icon_Wifi_bits",
    screenCode: SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS,
  }));

  const items: MenuScreenItem[] = [
    ...networkItems,
    { label: "Actualizar", iconRef: "Icon_Refrescar_bits", screenCode: SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING },
    { label: "Volver", iconRef: "Icon_Volver_bits", screenCode: SCREEN_CODE_CONNECTIVITY_WIFI_MENU },
  ];

  return buildMenuScreenCommands({
    screenCode: SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS,
    items,
    selectedIndex: args.selectedIndex ?? 0,
    firstVisibleIndex: args.firstVisibleIndex,
  });
}

export function buildScreen020203WifiResultsSummaryCommands(): OledCommand[] {
  return [
    clear(),
    ...textAt(14, 2, "Resultados", "Font11x18"),
    ...textAt(36, 20, "WiFi", "Font11x18"),
    ...textAt(4, 50, "OK: volver", "Font7x10"),
    ...drawXbm(112, 48, 13, 13, "Icon_Encoder_bits"),
  ];
}

export function buildLegacyVerticalMenuCommands(args: MenuScreenArgs): OledCommand[] {
  const menuItems = visibleItems(args.items);
  const page = Math.floor(args.selectedIndex / MENU_VISIBLE_ITEMS);
  const first = page * MENU_VISIBLE_ITEMS;
  const last = Math.min(first + MENU_VISIBLE_ITEMS, menuItems.length);
  const commands: OledCommand[] = [clear()];

  for (let i = first; i < last; i++) {
    const item = menuItems[i];
    const local = i - first;
    const y = MENU_ITEM_Y0 + local * MENU_ITEM_SPACING;

    if (item.iconRef) {
      commands.push(...drawXbm(8, y - 13, 16, 16, item.iconRef));
    }

    commands.push(...textAt(32, y, item.label, "Font7x10"));

    if (item.sourceIndex === args.selectedIndex) {
      commands.push(...drawFrame(0, y - 3, 128, 20));
    }
  }

  commands.push(...drawXbm(0, MENU_ITEM_Y0 + (args.selectedIndex % MENU_VISIBLE_ITEMS) * MENU_ITEM_SPACING, 8, 16, "Icon_Cursor_bits"));
  return commands;
}

export const mainMenuScreenExample = buildScreen010201MainMenuCommands({ selectedIndex: 0 });
export const wifiResultsMenuScreenExample = buildScreen020203WifiResultsMenuCommands({
  networkSsids: ["AutoLab", "Casa"],
  selectedIndex: 1,
});
