export type ActiveSection = 'code' | 'preview' | 'config' | 'wiring' | 'chronos' | 'ble-test';

export interface FirmwareFile {
  name: string;
  path: string;
  language: string;
  content: string;
  bytes: number;
}

export interface CYDHardwareConfig {
  enableChronosApp: boolean;
  tftDriver: 'ILI9341_2_DRIVER' | 'ST7789_DRIVER';
  rotation: 0 | 1;
  bleDeviceName: string;
  defaultLang: 'id' | 'en' | 'pt-BR';
  enableHwGps: boolean;
  gpsRxPin: number;
  gpsTxPin: number;
  gpsBaud: number;
  enableTurnBuzzer: boolean;
  buzzerPin: number;
  turnAlertDistM: number;
}

export const DEFAULT_CYD_CONFIG: CYDHardwareConfig = {
  enableChronosApp: true,
  tftDriver: 'ILI9341_2_DRIVER',
  rotation: 0,
  bleDeviceName: 'DisplayConnect-CYD',
  defaultLang: 'id',
  enableHwGps: true,
  gpsRxPin: 27,
  gpsTxPin: 22,
  gpsBaud: 9600,
  enableTurnBuzzer: true,
  buzzerPin: 26,
  turnAlertDistM: 100,
};
