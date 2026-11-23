import React, { useState, useEffect, useCallback } from 'react';
import { ActiveSection, CYDHardwareConfig, DEFAULT_CYD_CONFIG, FirmwareFile } from './types';
import { Navbar } from './components/Navbar';
import { FirmwareExplorer } from './components/FirmwareExplorer';
import { CYDPreviewHUD } from './components/CYDPreviewHUD';
import { HardwareConfigurator } from './components/HardwareConfigurator';
import { WiringPinoutGuide } from './components/WiringPinoutGuide';
import { WebBleTester } from './components/WebBleTester';
import { ChronosIntegrationGuide } from './components/ChronosIntegrationGuide';
import { OfflineIndicator } from './components/OfflineIndicator';
import { downloadFirmwareZip } from './utils/firmwareGenerator';

export default function App() {
  const [activeSection, setActiveSection] = useState<ActiveSection>('code');
  const [files, setFiles] = useState<FirmwareFile[]>([]);
  const [androidFiles, setAndroidFiles] = useState<FirmwareFile[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [config, setConfig] = useState<CYDHardwareConfig>(DEFAULT_CYD_CONFIG);
  const [isDownloading, setIsDownloading] = useState(false);

  const loadFirmwareFiles = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const [fwRes, androidRes] = await Promise.all([
        fetch('/api/firmware/files'),
        fetch('/api/android/files'),
      ]);
      if (!fwRes.ok) {
        throw new Error(`Gagal memuat file firmware (HTTP ${fwRes.status})`);
      }
      const fwData = await fwRes.json();
      setFiles(fwData.files || []);
      if (androidRes.ok) {
        const androidData = await androidRes.json();
        setAndroidFiles(androidData.files || []);
      }
    } catch (err: any) {
      setError(err.message || 'Terjadi kesalahan saat memuat file firmware ESP32 CYD.');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    loadFirmwareFiles();
  }, [loadFirmwareFiles]);

  const handleDownloadZip = async () => {
    if (files.length === 0) return;
    setIsDownloading(true);
    try {
      await downloadFirmwareZip(files, config);
    } finally {
      setIsDownloading(false);
    }
  };

  return (
    <div className="min-h-screen flex flex-col bg-slate-950 text-slate-100">
      <Navbar
        activeSection={activeSection}
        setActiveSection={setActiveSection}
        onDownloadZip={handleDownloadZip}
        isDownloading={isDownloading}
      />

      {/* Mobile Navigation Bar (visible < md) */}
      <div className="md:hidden flex items-center justify-around border-b border-slate-800 bg-slate-900 px-2 py-2 text-xs font-medium overflow-x-auto">
        <button
          onClick={() => setActiveSection('code')}
          className={`px-2 py-1 rounded-md whitespace-nowrap ${activeSection === 'code' ? 'bg-cyan-600 text-white' : 'text-slate-400'}`}
        >
          Kode
        </button>
        <button
          onClick={() => setActiveSection('preview')}
          className={`px-2 py-1 rounded-md whitespace-nowrap ${activeSection === 'preview' ? 'bg-cyan-600 text-white' : 'text-slate-400'}`}
        >
          Pratinjau
        </button>
        <button
          onClick={() => setActiveSection('config')}
          className={`px-2 py-1 rounded-md whitespace-nowrap ${activeSection === 'config' ? 'bg-cyan-600 text-white' : 'text-slate-400'}`}
        >
          Konfigurasi
        </button>
        <button
          onClick={() => setActiveSection('wiring')}
          className={`px-2 py-1 rounded-md whitespace-nowrap ${activeSection === 'wiring' ? 'bg-cyan-600 text-white' : 'text-slate-400'}`}
        >
          Pinout
        </button>
        <button
          onClick={() => setActiveSection('chronos')}
          className={`px-2 py-1 rounded-md whitespace-nowrap ${activeSection === 'chronos' ? 'bg-cyan-600 text-white' : 'text-slate-400'}`}
        >
          Chronos
        </button>
        <button
          onClick={() => setActiveSection('ble-test')}
          className={`px-2 py-1 rounded-md whitespace-nowrap ${activeSection === 'ble-test' ? 'bg-cyan-600 text-white' : 'text-slate-400'}`}
        >
          Uji BLE
        </button>
      </div>

      <main className="flex-1">
        {activeSection === 'code' && (
          <FirmwareExplorer
            files={files}
            androidFiles={androidFiles}
            loading={loading}
            error={error}
            config={config}
            setConfig={setConfig}
            onRetry={loadFirmwareFiles}
          />
        )}

        {activeSection === 'preview' && (
          <CYDPreviewHUD
            config={config}
            onGoToCode={() => setActiveSection('code')}
          />
        )}

        {activeSection === 'config' && (
          <HardwareConfigurator
            config={config}
            setConfig={setConfig}
            onGoToCode={() => setActiveSection('code')}
          />
        )}

        {activeSection === 'wiring' && (
          <WiringPinoutGuide config={config} />
        )}

        {activeSection === 'chronos' && (
          <ChronosIntegrationGuide
            config={config}
            onGoToBleTest={() => setActiveSection('ble-test')}
            onGoToPreview={() => setActiveSection('preview')}
            onGoToWiring={() => setActiveSection('wiring')}
          />
        )}

        {activeSection === 'ble-test' && (
          <WebBleTester />
        )}
      </main>

      <footer className="border-t border-slate-900 py-4 px-6 text-center text-xs text-slate-500">
        DisplayConnect CYD Motorcycle Firmware · Berdasarkan protokol BLE NUS v3.0 (malaq88/DisplayConnect)
      </footer>

      <OfflineIndicator />
    </div>
  );
}
