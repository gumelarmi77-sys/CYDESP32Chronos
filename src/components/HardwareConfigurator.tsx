import React from 'react';
import { CYDHardwareConfig, DEFAULT_CYD_CONFIG } from '../types';
import { buildCustomConfigH } from '../utils/firmwareGenerator';
import { RotateCcw } from 'lucide-react';

interface HardwareConfiguratorProps {
  config: CYDHardwareConfig;
  setConfig: React.Dispatch<React.SetStateAction<CYDHardwareConfig>>;
  onGoToCode: () => void;
}

export const HardwareConfigurator: React.FC<HardwareConfiguratorProps> = ({
  config,
  setConfig,
  onGoToCode,
}) => {
  const previewConfigH = buildCustomConfigH(config);

  return (
    <div className="max-w-7xl mx-auto px-6 py-6 space-y-6">
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4 pb-5 border-b border-slate-800">
        <div>
          <h2 className="text-xl font-bold text-white">Konfigurasi Board & Parameter CYD ESP32</h2>
          <p className="text-sm text-slate-400 mt-1">
            Sesuaikan driver layar TFT, modul GPS UART, buzzer belokan, dan bahasa sebelum menyalin atau mengunduh ZIP firmware.
          </p>
        </div>
        <div className="flex items-center gap-3 shrink-0">
          <button
            onClick={() => setConfig(DEFAULT_CYD_CONFIG)}
            className="flex items-center gap-1.5 px-3.5 py-2 text-xs font-medium text-slate-300 bg-slate-900 border border-slate-800 rounded-lg hover:bg-slate-800 transition-colors whitespace-nowrap shrink-0"
          >
            <RotateCcw className="w-3.5 h-3.5" />
            Reset Default
          </button>
          <button
            onClick={onGoToCode}
            className="px-4 py-2 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 transition-colors whitespace-nowrap shrink-0"
          >
            Lihat Kode Hasil
          </button>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8 items-start">
        {/* Form Controls */}
        <div className="lg:col-span-6 space-y-6 rounded-xl border border-slate-800 bg-slate-900/60 p-6">
          {/* Android App Protocol Mode */}
          <div className="space-y-2">
            <label className="block text-xs font-semibold text-slate-300">Aplikasi Smartphone Pendamping (BLE)</label>
            <div className="grid grid-cols-2 gap-3">
              <button
                onClick={() => setConfig((p) => ({ ...p, enableChronosApp: true }))}
                className={`p-3 rounded-lg border text-left transition-colors ${
                  config.enableChronosApp
                    ? 'border-cyan-500 bg-cyan-500/10 text-white'
                    : 'border-slate-800 bg-slate-950 text-slate-400 hover:text-white'
                }`}
              >
                <div className="text-xs font-semibold">Chronos App (Direkomendasikan)</div>
                <div className="text-[11px] text-slate-400 mt-0.5">Langsung pakai Chronos Play Store (fbiego)</div>
              </button>
              <button
                onClick={() => setConfig((p) => ({ ...p, enableChronosApp: false }))}
                className={`p-3 rounded-lg border text-left transition-colors ${
                  !config.enableChronosApp
                    ? 'border-cyan-500 bg-cyan-500/10 text-white'
                    : 'border-slate-800 bg-slate-950 text-slate-400 hover:text-white'
                }`}
              >
                <div className="text-xs font-semibold">DisplayConnect JSON</div>
                <div className="text-[11px] text-slate-400 mt-0.5">Untuk APK DisplayConnect / Uji Web BLE</div>
              </button>
            </div>
          </div>

          {/* Display Driver */}
          <div className="space-y-2">
            <label className="block text-xs font-semibold text-slate-300">Driver Layar TFT (ESP32-2432S028R)</label>
            <div className="grid grid-cols-2 gap-3">
              <button
                onClick={() => setConfig((p) => ({ ...p, tftDriver: 'ILI9341_2_DRIVER' }))}
                className={`p-3 rounded-lg border text-left transition-colors ${
                  config.tftDriver === 'ILI9341_2_DRIVER'
                    ? 'border-cyan-500 bg-cyan-500/10 text-white'
                    : 'border-slate-800 bg-slate-950 text-slate-400 hover:text-white'
                }`}
              >
                <div className="text-xs font-semibold">ILI9341 (Standar 1-USB)</div>
                <div className="text-[11px] text-slate-400 mt-0.5">Micro-USB tunggal · Paling umum</div>
              </button>
              <button
                onClick={() => setConfig((p) => ({ ...p, tftDriver: 'ST7789_DRIVER' }))}
                className={`p-3 rounded-lg border text-left transition-colors ${
                  config.tftDriver === 'ST7789_DRIVER'
                    ? 'border-cyan-500 bg-cyan-500/10 text-white'
                    : 'border-slate-800 bg-slate-950 text-slate-400 hover:text-white'
                }`}
              >
                <div className="text-xs font-semibold">ST7789 (Varian 2-USB)</div>
                <div className="text-[11px] text-slate-400 mt-0.5">USB-C + Micro-USB · TFT_BGR</div>
              </button>
            </div>
          </div>

          {/* BLE Name & Language */}
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <div className="space-y-1.5">
              <label className="block text-xs font-semibold text-slate-300">Nama Perangkat BLE</label>
              <input
                type="text"
                value={config.bleDeviceName}
                onChange={(e) => setConfig((p) => ({ ...p, bleDeviceName: e.target.value }))}
                className="w-full bg-slate-950 border border-slate-800 rounded-lg px-3 py-2 text-xs font-mono text-white focus:outline-none focus:border-cyan-500"
              />
            </div>

            <div className="space-y-1.5">
              <label className="block text-xs font-semibold text-slate-300">Bahasa Default Layar</label>
              <select
                value={config.defaultLang}
                onChange={(e) => setConfig((p) => ({ ...p, defaultLang: e.target.value as 'id' | 'en' | 'pt-BR' }))}
                className="w-full bg-slate-950 border border-slate-800 rounded-lg px-3 py-2 text-xs text-white focus:outline-none focus:border-cyan-500"
              >
                <option value="id">Indonesia (id)</option>
                <option value="en">English (en)</option>
                <option value="pt-BR">Português (pt-BR)</option>
              </select>
            </div>
          </div>

          {/* Rotation */}
          <div className="space-y-2">
            <label className="block text-xs font-semibold text-slate-300">Orientasi Layar TFT</label>
            <div className="grid grid-cols-2 gap-3">
              <button
                onClick={() => setConfig((p) => ({ ...p, rotation: 0 }))}
                className={`p-3 rounded-lg border text-left transition-colors ${
                  config.rotation === 0
                    ? 'border-cyan-500 bg-cyan-500/10 text-white'
                    : 'border-slate-800 bg-slate-950 text-slate-400 hover:text-white'
                }`}
              >
                <div className="text-xs font-semibold">Portrait 240×320 (0)</div>
                <div className="text-[11px] text-slate-400 mt-0.5">Direkomendasikan untuk app DisplayConnect</div>
              </button>
              <button
                onClick={() => setConfig((p) => ({ ...p, rotation: 1 }))}
                className={`p-3 rounded-lg border text-left transition-colors ${
                  config.rotation === 1
                    ? 'border-cyan-500 bg-cyan-500/10 text-white'
                    : 'border-slate-800 bg-slate-950 text-slate-400 hover:text-white'
                }`}
              >
                <div className="text-xs font-semibold">Landscape 320×240 (1)</div>
                <div className="text-[11px] text-slate-400 mt-0.5">Cocok jika mount stang horizontal</div>
              </button>
            </div>
          </div>

          {/* Hardware GPS Toggle & Pins */}
          <div className="pt-4 border-t border-slate-800 space-y-4">
            <div className="flex items-center justify-between">
              <div>
                <div className="text-xs font-semibold text-white">Integrasi Modul GPS Fisik (NEO-6M / NEO-8M)</div>
                <div className="text-[11px] text-slate-400">Membaca kecepatan km/h & satelit langsung via UART2 konektor CN1/P3</div>
              </div>
              <button
                onClick={() => setConfig((p) => ({ ...p, enableHwGps: !p.enableHwGps }))}
                className={`px-3 py-1.5 rounded-lg text-xs font-semibold transition-colors whitespace-nowrap shrink-0 ${
                  config.enableHwGps ? 'bg-emerald-600 text-white' : 'bg-slate-800 text-slate-400'
                }`}
              >
                {config.enableHwGps ? 'Aktif' : 'Nonaktif'}
              </button>
            </div>

            {config.enableHwGps && (
              <div className="grid grid-cols-3 gap-3">
                <div className="space-y-1">
                  <label className="text-[11px] text-slate-400">GPS RX Pin</label>
                  <input
                    type="number"
                    value={config.gpsRxPin}
                    onChange={(e) => setConfig((p) => ({ ...p, gpsRxPin: Number(e.target.value) || 27 }))}
                    className="w-full bg-slate-950 border border-slate-800 rounded-lg px-2.5 py-1.5 text-xs font-mono tabular-nums text-white"
                  />
                </div>
                <div className="space-y-1">
                  <label className="text-[11px] text-slate-400">GPS TX Pin</label>
                  <input
                    type="number"
                    value={config.gpsTxPin}
                    onChange={(e) => setConfig((p) => ({ ...p, gpsTxPin: Number(e.target.value) || 22 }))}
                    className="w-full bg-slate-950 border border-slate-800 rounded-lg px-2.5 py-1.5 text-xs font-mono tabular-nums text-white"
                  />
                </div>
                <div className="space-y-1">
                  <label className="text-[11px] text-slate-400">Baudrate</label>
                  <select
                    value={config.gpsBaud}
                    onChange={(e) => setConfig((p) => ({ ...p, gpsBaud: Number(e.target.value) }))}
                    className="w-full bg-slate-950 border border-slate-800 rounded-lg px-2.5 py-1.5 text-xs font-mono tabular-nums text-white"
                  >
                    <option value={9600}>9600</option>
                    <option value={38400}>38400</option>
                    <option value={115200}>115200</option>
                  </select>
                </div>
              </div>
            )}
          </div>

          {/* Turn Alert Buzzer */}
          <div className="pt-4 border-t border-slate-800 space-y-4">
            <div className="flex items-center justify-between">
              <div>
                <div className="text-xs font-semibold text-white">Buzzer Peringatan Belokan (GPIO 26)</div>
                <div className="text-[11px] text-slate-400">Berbunyi singkat saat mendekati belokan atau keluar rute</div>
              </div>
              <button
                onClick={() => setConfig((p) => ({ ...p, enableTurnBuzzer: !p.enableTurnBuzzer }))}
                className={`px-3 py-1.5 rounded-lg text-xs font-semibold transition-colors whitespace-nowrap shrink-0 ${
                  config.enableTurnBuzzer ? 'bg-emerald-600 text-white' : 'bg-slate-800 text-slate-400'
                }`}
              >
                {config.enableTurnBuzzer ? 'Aktif' : 'Nonaktif'}
              </button>
            </div>

            {config.enableTurnBuzzer && (
              <div className="grid grid-cols-2 gap-3">
                <div className="space-y-1">
                  <label className="text-[11px] text-slate-400">Pin Buzzer / Speaker</label>
                  <input
                    type="number"
                    value={config.buzzerPin}
                    onChange={(e) => setConfig((p) => ({ ...p, buzzerPin: Number(e.target.value) || 26 }))}
                    className="w-full bg-slate-950 border border-slate-800 rounded-lg px-2.5 py-1.5 text-xs font-mono tabular-nums text-white"
                  />
                </div>
                <div className="space-y-1">
                  <label className="text-[11px] text-slate-400">Jarak Alert (meter)</label>
                  <input
                    type="number"
                    value={config.turnAlertDistM}
                    onChange={(e) => setConfig((p) => ({ ...p, turnAlertDistM: Number(e.target.value) || 100 }))}
                    className="w-full bg-slate-950 border border-slate-800 rounded-lg px-2.5 py-1.5 text-xs font-mono tabular-nums text-white"
                  />
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Live config.h Preview */}
        <div className="lg:col-span-6 rounded-xl border border-slate-800 bg-slate-950 overflow-hidden">
          <div className="px-4 py-3 border-b border-slate-800 bg-slate-900/90 flex items-center justify-between">
            <span className="text-xs font-mono font-semibold text-white">Pratinjau config.h</span>
            <span className="text-[11px] text-cyan-400">Otomatis diperbarui</span>
          </div>
          <pre className="p-4 text-xs font-mono text-slate-200 overflow-x-auto leading-relaxed">
            <code>{previewConfigH}</code>
          </pre>
        </div>
      </div>
    </div>
  );
};
