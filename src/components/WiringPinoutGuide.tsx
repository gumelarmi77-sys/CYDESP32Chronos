import React, { useState } from 'react';
import { CYDHardwareConfig } from '../types';
import { AlertTriangle, Cpu, Sparkles } from 'lucide-react';

interface WiringPinoutGuideProps {
  config: CYDHardwareConfig;
}

export const WiringPinoutGuide: React.FC<WiringPinoutGuideProps> = ({ config }) => {
  const [aiQuestion, setAiQuestion] = useState('');
  const [aiAnswer, setAiAnswer] = useState('');
  const [askingAi, setAskingAi] = useState(false);

  const pinRows = [
    { subsystem: 'TFT LCD (HSPI)', signal: 'TFT_MOSI', gpio: 'GPIO 13', notes: 'Terintegrasi di PCB CYD' },
    { subsystem: 'TFT LCD (HSPI)', signal: 'TFT_MISO', gpio: 'GPIO 12', notes: 'Terintegrasi di PCB CYD' },
    { subsystem: 'TFT LCD (HSPI)', signal: 'TFT_SCLK', gpio: 'GPIO 14', notes: 'Terintegrasi di PCB CYD' },
    { subsystem: 'TFT LCD (HSPI)', signal: 'TFT_CS / DC', gpio: 'GPIO 15 / GPIO 2', notes: 'RST tidak terhubung (-1)' },
    { subsystem: 'TFT Backlight', signal: 'TFT_BL (PWM)', gpio: 'GPIO 21', notes: 'Kontrol kecerahan 25%/65%/100%' },
    { subsystem: 'Touch XPT2046 (VSPI)', signal: 'MOSI / MISO / CLK', gpio: '32 / 39 / 25', notes: 'Bus SPI terpisah dari TFT' },
    { subsystem: 'Touch XPT2046 (VSPI)', signal: 'TOUCH_CS / IRQ', gpio: 'GPIO 33 / GPIO 36', notes: 'Mode poll pressure untuk akurasi' },
    { subsystem: 'GPS Eksternal (CN1/P3)', signal: 'GPS TX → ESP RX', gpio: `GPIO ${config.gpsRxPin}`, notes: 'Pin tengah JST CN1/P3 (Serial2)' },
    { subsystem: 'GPS Eksternal (CN1/P3)', signal: 'GPS RX ← ESP TX', gpio: `GPIO ${config.gpsTxPin}`, notes: 'Pin JST CN1/P3 (Serial2)' },
    { subsystem: 'Audio / Buzzer', signal: 'BUZZER / SPK', gpio: `GPIO ${config.buzzerPin}`, notes: 'Piezo pasif atau speaker JST P4' },
  ];

  const handleAskHelp = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!aiQuestion.trim()) return;
    setAskingAi(true);
    setAiAnswer('');
    try {
      const res = await fetch('/api/ai/firmware-help', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          question: aiQuestion,
          boardVariant: config.tftDriver,
        }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.error || 'Gagal meminta bantuan');
      setAiAnswer(data.answer);
    } catch (err: any) {
      setAiAnswer(`Error: ${err.message}`);
    } finally {
      setAskingAi(false);
    }
  };

  return (
    <div className="max-w-7xl mx-auto px-6 py-6 space-y-8">
      {/* Power Warning for Motorcycle 12V */}
      <div className="flex items-start gap-3.5 rounded-xl border border-amber-500/40 bg-amber-500/10 p-4">
        <AlertTriangle className="w-5 h-5 text-amber-400 shrink-0 mt-0.5" />
        <div className="space-y-1 text-xs leading-relaxed">
          <div className="font-bold text-amber-300">Peringatan Kelistrikan Sepeda Motor (12V DC)</div>
          <p className="text-slate-300">
            Jangan hubungkan aki motor 12V (yang bisa melonjak hingga 14.5V saat mesin menyala) langsung ke pin 5V / VIN CYD ESP32! Gunakan modul Step-Down Buck Converter 12V ke 5V (minimal 1.5A, idealnya dengan soket USB waterproof di stang) agar ESP32 dan modul GPS NEO-6M aman dari lonjakan tegangan.
          </p>
        </div>
      </div>

      {/* Pinout Table + Touch Zone Layout */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8 items-start">
        <div className="lg:col-span-7 rounded-xl border border-slate-800 bg-slate-900/60 overflow-hidden">
          <div className="px-5 py-3.5 border-b border-slate-800 flex items-center justify-between">
            <h2 className="text-sm font-bold text-white flex items-center gap-2">
              <Cpu className="w-4 h-4 text-cyan-400" />
              Tabel Pinout ESP32-2432S028R (CYD)
            </h2>
            <span className="text-xs text-slate-400 font-mono">3.3V Logic</span>
          </div>
          <div className="overflow-x-auto">
            <table className="w-full text-left text-xs">
              <thead className="bg-slate-950/80 text-slate-400 border-b border-slate-800">
                <tr>
                  <th className="px-4 py-2.5 font-semibold">Subsistem</th>
                  <th className="px-4 py-2.5 font-semibold">Sinyal</th>
                  <th className="px-4 py-2.5 font-semibold">Pin ESP32</th>
                  <th className="px-4 py-2.5 font-semibold">Keterangan</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-800/70">
                {pinRows.map((row, idx) => (
                  <tr key={idx} className="hover:bg-slate-800/40">
                    <td className="px-4 py-2.5 text-slate-300 font-medium">{row.subsystem}</td>
                    <td className="px-4 py-2.5 font-mono text-cyan-300">{row.signal}</td>
                    <td className="px-4 py-2.5 font-mono tabular-nums text-white font-semibold">{row.gpio}</td>
                    <td className="px-4 py-2.5 text-slate-400">{row.notes}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        {/* Touch Zones & Installation Steps */}
        <div className="lg:col-span-5 space-y-6">
          <div className="rounded-xl border border-slate-800 bg-slate-900/60 p-5 space-y-4">
            <h3 className="text-sm font-bold text-white">Zona Sentuh Layar (Sarung Tangan Motor)</h3>
            <p className="text-xs text-slate-400 leading-relaxed">
              Layar sentuh resistif XPT2046 bekerja responsif menggunakan sarung tangan motor biasa. Area sentuh diperbesar ke tiga sudut utama:
            </p>
            <div className="space-y-2.5 text-xs">
              <div className="flex justify-between py-2 border-b border-slate-800">
                <span className="font-semibold text-white">Sudut Kiri Atas (BRI)</span>
                <span className="text-slate-400">Ubah kecerahan TFT (25% / 65% / 100%)</span>
              </div>
              <div className="flex justify-between py-2 border-b border-slate-800">
                <span className="font-semibold text-white">Sudut Kanan Atas (MAP/HUD)</span>
                <span className="text-slate-400">Ganti Vector Map vs Panah Besar HUD</span>
              </div>
              <div className="flex justify-between py-2">
                <span className="font-semibold text-white">Sudut Kanan Bawah (ZK/CH)</span>
                <span className="text-slate-400">Toggle Tema Zaku Olive / Char Desert (NVS)</span>
              </div>
            </div>
          </div>

          <div className="rounded-xl border border-slate-800 bg-slate-900/60 p-5 space-y-3">
            <h3 className="text-sm font-bold text-white">Cara Compile & Flash ke ESP32 CYD</h3>
            <ol className="text-xs text-slate-300 space-y-2 list-decimal list-inside leading-relaxed">
              <li>Klik <strong>Download .ZIP</strong> di pojok kanan atas lalu ekstrak foldernya.</li>
              <li><strong>PlatformIO (Disarankan):</strong> Buka folder hasil ekstrak di VS Code + PlatformIO, lalu klik <em>Upload</em>. Semua library dan pin TFT sudah otomatis dikonfigurasi di <code className="text-cyan-400">platformio.ini</code>.</li>
              <li><strong>Arduino IDE:</strong> Install library <code className="text-cyan-400">TFT_eSPI</code>, <code className="text-cyan-400">NimBLE-Arduino</code> (v2.1.2+), <code className="text-cyan-400">ChronosESP32</code>, <code className="text-cyan-400">ESP32Time</code>, <code className="text-cyan-400">XPT2046_Touchscreen</code>, dan <code className="text-cyan-400">TinyGPSPlus</code>. Salin isi <code className="text-cyan-400">User_Setup_CYD.h</code> ke <code className="text-cyan-400">libraries/TFT_eSPI/User_Setup.h</code>.</li>
              <li>Pilih Partition Scheme <strong>Huge APP (3MB No OTA)</strong> agar NimBLE + ChronosESP32 + TFT_eSPI muat, lalu Upload ke ESP32 Dev Module.</li>
              <li>Buka aplikasi Android <strong>Chronos</strong> (Play Store / <code className="text-cyan-400">fbiego/chronos-esp32</code>), pindai &amp; pilih <strong>{config.bleDeviceName}</strong>, aktifkan <strong>Navigation</strong> &amp; <strong>Notification Access</strong>, lalu mulai rute Google Maps!</li>
            </ol>
          </div>
        </div>
      </div>

      {/* Troubleshooting Assistant */}
      <div className="rounded-xl border border-slate-800 bg-slate-900/60 p-5 space-y-4">
        <div className="flex items-center gap-2">
          <Sparkles className="w-4 h-4 text-cyan-400" />
          <h3 className="text-sm font-bold text-white">Asisten Troubleshooting Flash & Wiring CYD</h3>
        </div>
        <form onSubmit={handleAskHelp} className="flex flex-col sm:flex-row gap-3">
          <input
            type="text"
            value={aiQuestion}
            onChange={(e) => setAiQuestion(e.target.value)}
            placeholder="Contoh: Layar CYD saya putih polos atau warna terbalik setelah upload, apa solusinya?"
            className="flex-1 bg-slate-950 border border-slate-800 rounded-lg px-3.5 py-2 text-xs text-white placeholder-slate-500 focus:outline-none focus:border-cyan-500"
          />
          <button
            type="submit"
            disabled={askingAi || !aiQuestion.trim()}
            className="px-4 py-2 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 disabled:opacity-50 transition-colors whitespace-nowrap shrink-0"
          >
            {askingAi ? 'Menganalisis...' : 'Tanya Solusi'}
          </button>
        </form>
        {aiAnswer && (
          <div className="p-4 rounded-lg bg-slate-950 border border-slate-800 text-xs text-slate-200 whitespace-pre-wrap leading-relaxed">
            {aiAnswer}
          </div>
        )}
      </div>
    </div>
  );
};
