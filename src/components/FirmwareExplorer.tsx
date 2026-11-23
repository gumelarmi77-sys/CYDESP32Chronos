import React, { useState, useMemo, useEffect } from 'react';
import { CYDHardwareConfig, FirmwareFile } from '../types';
import { applyConfigToFile, downloadAndroidPatchZip } from '../utils/firmwareGenerator';
import { Copy, Check, Download, FileCode, Search, FolderOpen, Cpu, Smartphone } from 'lucide-react';

interface FirmwareExplorerProps {
  files: FirmwareFile[];
  androidFiles: FirmwareFile[];
  loading: boolean;
  error: string | null;
  config: CYDHardwareConfig;
  setConfig: React.Dispatch<React.SetStateAction<CYDHardwareConfig>>;
  onRetry: () => void;
}

type CodeTargetTab = 'esp32' | 'android';

export const FirmwareExplorer: React.FC<FirmwareExplorerProps> = ({
  files,
  androidFiles,
  loading,
  error,
  config,
  setConfig,
  onRetry,
}) => {
  const [targetTab, setTargetTab] = useState<CodeTargetTab>('esp32');
  const [selectedPath, setSelectedPath] = useState<string>('DisplayConnect_Moto_CYD/DisplayConnect_Moto_CYD.ino');
  const [searchQuery, setSearchQuery] = useState('');
  const [copied, setCopied] = useState(false);
  const [isDownloadingAndroid, setIsDownloadingAndroid] = useState(false);

  const currentFileList = targetTab === 'esp32' ? files : androidFiles;

  useEffect(() => {
    setSearchQuery('');
    if (targetTab === 'esp32') {
      setSelectedPath('DisplayConnect_Moto_CYD/DisplayConnect_Moto_CYD.ino');
    } else {
      setSelectedPath('README_ANDROID_PATCH.md');
    }
  }, [targetTab]);

  const filteredFiles = useMemo(() => {
    const q = searchQuery.trim().toLowerCase();
    if (!q) return currentFileList;
    return currentFileList.filter((f) => f.path.toLowerCase().includes(q));
  }, [currentFileList, searchQuery]);

  const activeFile = useMemo(() => {
    return currentFileList.find((f) => f.path === selectedPath) || currentFileList[0] || null;
  }, [currentFileList, selectedPath]);

  const renderedContent = useMemo(() => {
    if (!activeFile) return '';
    return targetTab === 'esp32' ? applyConfigToFile(activeFile, config) : activeFile.content;
  }, [activeFile, config, targetTab]);

  const handleCopy = async () => {
    if (!renderedContent) return;
    await navigator.clipboard.writeText(renderedContent);
    setCopied(true);
    setTimeout(() => setCopied(false), 1800);
  };

  const handleDownloadSingle = () => {
    if (!activeFile) return;
    const blob = new Blob([renderedContent], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = activeFile.name;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  const handleDownloadAndroidZip = async () => {
    if (androidFiles.length === 0) return;
    setIsDownloadingAndroid(true);
    try {
      await downloadAndroidPatchZip(androidFiles);
    } finally {
      setIsDownloadingAndroid(false);
    }
  };

  if (loading) {
    return (
      <div className="max-w-7xl mx-auto px-6 py-10">
        <div className="h-[620px] rounded-xl border border-slate-800 bg-slate-900/50 animate-pulse" />
      </div>
    );
  }

  if (error) {
    return (
      <div className="max-w-7xl mx-auto px-6 py-12">
        <div className="rounded-xl border border-rose-500/30 bg-slate-900 p-8 text-center space-y-4">
          <p className="text-sm text-rose-400 font-medium">{error}</p>
          <button
            onClick={onRetry}
            className="px-4 py-2 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 transition-colors"
          >
            Muat Ulang File Firmware
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="max-w-7xl mx-auto px-6 py-6 space-y-5">
      {/* Top Hero Summary + Platform Target Switcher */}
      <div className="flex flex-col lg:flex-row lg:items-center justify-between gap-4 pb-5 border-b border-slate-800">
        <div className="space-y-1 max-w-2xl">
          <h1 className="text-2xl font-bold tracking-tight text-white">
            {targetTab === 'esp32'
              ? 'Kode Firmware ESP32 CYD — DisplayConnect Motor'
              : 'Patch Android DisplayConnect — Notifikasi WhatsApp & Telepon'}
          </h1>
          <p className="text-sm text-slate-400">
            {targetTab === 'esp32'
              ? 'Firmware Arduino/PlatformIO siap compile untuk layar TFT ESP32-2432S028R. Kompatibel penuh dengan protokol BLE UART v3.0 dari malaq88/DisplayConnect, tema taktis Zaku/Char, sentuh XPT2046, dan GPS NEO-6M.'
              : 'Ekstensi Android Kotlin (NotificationListenerService + TelephonyCallback) untuk repositori malaq88/DisplayConnect agar pesan WhatsApp & panggilan masuk otomatis tampil di layar CYD motor.'}
          </p>
        </div>

        {/* Target Switcher + Context Controls */}
        <div className="flex flex-wrap items-center gap-2.5 shrink-0">
          <div className="flex items-center p-1 bg-slate-900 rounded-lg border border-slate-800">
            <button
              onClick={() => setTargetTab('esp32')}
              className={`flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold rounded-md transition-colors whitespace-nowrap shrink-0 ${
                targetTab === 'esp32'
                  ? 'bg-cyan-600 text-white'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Cpu className="w-3.5 h-3.5" />
              <span>ESP32 CYD ({files.length})</span>
            </button>
            <button
              onClick={() => setTargetTab('android')}
              className={`flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold rounded-md transition-colors whitespace-nowrap shrink-0 ${
                targetTab === 'android'
                  ? 'bg-emerald-600 text-white'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Smartphone className="w-3.5 h-3.5" />
              <span>Android App Patch ({androidFiles.length})</span>
            </button>
          </div>

          {targetTab === 'esp32' ? (
            <>
              <div className="flex items-center p-1 bg-slate-900 rounded-lg border border-slate-800">
                <button
                  onClick={() => setConfig((prev) => ({ ...prev, tftDriver: 'ILI9341_2_DRIVER' }))}
                  className={`px-3 py-1.5 text-xs font-medium rounded-md transition-colors whitespace-nowrap shrink-0 ${
                    config.tftDriver === 'ILI9341_2_DRIVER'
                      ? 'bg-cyan-600 text-white'
                      : 'text-slate-400 hover:text-white'
                  }`}
                >
                  ILI9341 (1-USB)
                </button>
                <button
                  onClick={() => setConfig((prev) => ({ ...prev, tftDriver: 'ST7789_DRIVER' }))}
                  className={`px-3 py-1.5 text-xs font-medium rounded-md transition-colors whitespace-nowrap shrink-0 ${
                    config.tftDriver === 'ST7789_DRIVER'
                      ? 'bg-cyan-600 text-white'
                      : 'text-slate-400 hover:text-white'
                  }`}
                >
                  ST7789 (2-USB)
                </button>
              </div>

              <button
                onClick={() => setConfig((prev) => ({ ...prev, enableHwGps: !prev.enableHwGps }))}
                className={`px-3 py-2 text-xs font-semibold rounded-lg border transition-colors whitespace-nowrap shrink-0 ${
                  config.enableHwGps
                    ? 'bg-emerald-500/10 border-emerald-500/40 text-emerald-400'
                    : 'bg-slate-900 border-slate-800 text-slate-400 hover:text-white'
                }`}
              >
                {config.enableHwGps ? 'GPS NEO-6M: Aktif' : 'GPS NEO-6M: Nonaktif'}
              </button>
            </>
          ) : (
            <button
              onClick={handleDownloadAndroidZip}
              disabled={isDownloadingAndroid || androidFiles.length === 0}
              className="flex items-center gap-2 px-3.5 py-2 text-xs font-semibold text-white bg-emerald-600 rounded-lg hover:bg-emerald-500 disabled:opacity-60 transition-colors whitespace-nowrap shrink-0"
            >
              <Download className="w-3.5 h-3.5" />
              <span>{isDownloadingAndroid ? 'Menyiapkan...' : 'Download Patch Android (.ZIP)'}</span>
            </button>
          )}
        </div>
      </div>

      {/* Main Split IDE Layout: File Tree + Code Viewer */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-start">
        <aside className="lg:col-span-3 rounded-xl border border-slate-800 bg-slate-900/70 overflow-hidden">
          <div className="p-3 border-b border-slate-800 space-y-2.5">
            <div className="flex items-center justify-between text-xs font-semibold text-slate-300">
              <span className="flex items-center gap-1.5">
                <FolderOpen className={`w-3.5 h-3.5 ${targetTab === 'esp32' ? 'text-cyan-400' : 'text-emerald-400'}`} />
                {targetTab === 'esp32'
                  ? `Project CYD (${files.length})`
                  : `Patch Android (${androidFiles.length})`}
              </span>
            </div>
            <div className="relative">
              <Search className="w-3.5 h-3.5 text-slate-500 absolute left-2.5 top-2.5" />
              <input
                type="text"
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                placeholder={targetTab === 'esp32' ? 'Cari file .ino / .cpp / .h' : 'Cari file .kt / .xml / .md'}
                className="w-full bg-slate-950 border border-slate-800 rounded-lg pl-8 pr-3 py-1.5 text-xs text-slate-200 placeholder-slate-500 focus:outline-none focus:border-cyan-500"
              />
            </div>
          </div>

          <div className="max-h-[560px] overflow-y-auto divide-y divide-slate-800/50">
            {filteredFiles.length === 0 ? (
              <div className="p-4 text-xs text-slate-500 text-center">File tidak ditemukan.</div>
            ) : (
              filteredFiles.map((file) => {
                const isSelected = activeFile?.path === file.path;
                const isModifiedByConfig =
                  targetTab === 'esp32' &&
                  (file.path.endsWith('config.h') ||
                    file.path.endsWith('User_Setup_CYD.h') ||
                    file.path === 'platformio.ini');

                return (
                  <button
                    key={file.path}
                    onClick={() => setSelectedPath(file.path)}
                    className={`w-full flex items-center justify-between gap-2 px-3.5 py-2.5 text-left text-xs transition-colors ${
                      isSelected
                        ? targetTab === 'esp32'
                          ? 'bg-cyan-600/15 text-cyan-300 font-semibold'
                          : 'bg-emerald-600/15 text-emerald-300 font-semibold'
                        : 'text-slate-300 hover:bg-slate-800/60'
                    }`}
                  >
                    <span className="flex items-center gap-2 min-w-0">
                      <FileCode
                        className={`w-3.5 h-3.5 shrink-0 ${
                          isSelected
                            ? targetTab === 'esp32'
                              ? 'text-cyan-400'
                              : 'text-emerald-400'
                            : 'text-slate-500'
                        }`}
                      />
                      <span className="truncate">{file.name}</span>
                    </span>
                    <span className="text-[11px] font-mono tabular-nums text-slate-500 shrink-0">
                      {isModifiedByConfig ? 'Dinamis' : `${(file.bytes / 1024).toFixed(1)}K`}
                    </span>
                  </button>
                );
              })
            )}
          </div>
        </aside>

        {/* Code Viewer Pane */}
        <section className="lg:col-span-9 rounded-xl border border-slate-800 bg-slate-950 overflow-hidden">
          {activeFile ? (
            <>
              <div className="flex flex-wrap items-center justify-between gap-3 px-4 py-3 border-b border-slate-800 bg-slate-900/90">
                <div className="min-w-0">
                  <div className="text-xs font-mono font-semibold text-white truncate">
                    {activeFile.path}
                  </div>
                  <div className="text-[11px] text-slate-400 mt-0.5 tabular-nums">
                    {renderedContent.split('\n').length} baris · {new Blob([renderedContent]).size} bytes
                  </div>
                </div>

                <div className="flex items-center gap-2 shrink-0">
                  <button
                    onClick={handleDownloadSingle}
                    className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium text-slate-200 bg-slate-800 rounded-lg hover:bg-slate-700 transition-colors whitespace-nowrap shrink-0"
                  >
                    <Download className="w-3.5 h-3.5" />
                    <span>Simpan File</span>
                  </button>
                  <button
                    onClick={handleCopy}
                    className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 transition-colors whitespace-nowrap shrink-0"
                  >
                    {copied ? <Check className="w-3.5 h-3.5" /> : <Copy className="w-3.5 h-3.5" />}
                    <span>{copied ? 'Tersalin' : 'Salin Kode'}</span>
                  </button>
                </div>
              </div>

              <div className="max-h-[600px] overflow-auto p-4 bg-slate-950">
                <pre className="text-xs leading-relaxed font-mono text-slate-200 whitespace-pre">
                  <code>{renderedContent}</code>
                </pre>
              </div>
            </>
          ) : (
            <div className="p-12 text-center text-sm text-slate-500">
              Pilih file di panel kiri untuk melihat kode.
            </div>
          )}
        </section>
      </div>
    </div>
  );
};
