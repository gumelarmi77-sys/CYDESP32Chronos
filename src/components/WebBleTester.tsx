import React, { useState, useRef } from 'react';
import { Bluetooth, Send, Unplug, CheckCircle2, AlertCircle } from 'lucide-react';

const NUS_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_RX_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_TX_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';

const SAMPLE_NAV_PACKET = {
  type: 'nav',
  lat: -6.2088,
  lon: 106.8456,
  bearing: 45.0,
  speed_kmh: 52,
  instruction: 'Belok kanan ke Jl. Jend. Sudirman',
  distance_m: 120,
  remaining_m: 4200,
  remaining_s: 540,
  off_route: false,
  lang: 'id',
  gps_weak: false,
  street: 'Jl. Gatot Subroto',
  user_x: 120,
  user_y: 165,
  route: [
    [120, 210],
    [120, 165],
    [120, 110],
    [175, 65],
    [220, 40],
  ],
  streets: [
    [40, 165, 200, 165],
    [120, 20, 120, 225],
    [60, 110, 220, 110],
  ],
};

export const WebBleTester: React.FC = () => {
  const [connectedName, setConnectedName] = useState<string | null>(null);
  const [statusLog, setStatusLog] = useState<string[]>([]);
  const [jsonPayload, setJsonPayload] = useState<string>(JSON.stringify(SAMPLE_NAV_PACKET, null, 2));
  const [errorMsg, setErrorMsg] = useState<string | null>(null);

  const rxCharRef = useRef<any>(null);
  const deviceRef = useRef<any>(null);

  const appendLog = (msg: string) => {
    const time = new Date().toLocaleTimeString('id-ID', { hour12: false });
    setStatusLog((prev) => [`[${time}] ${msg}`, ...prev].slice(0, 30));
  };

  const handleConnectBle = async () => {
    setErrorMsg(null);
    const navAny = navigator as any;
    if (!navAny.bluetooth) {
      setErrorMsg(
        'Browser atau mode iframe ini tidak mendukung Web Bluetooth. Buka halaman di Chrome/Edge desktop atau Android untuk mengetes langsung ke ESP32 CYD.'
      );
      return;
    }

    try {
      appendLog('Memindai perangkat BLE DisplayConnect-CYD...');
      const device = await navAny.bluetooth.requestDevice({
        filters: [{ namePrefix: 'DisplayConnect' }, { services: [NUS_SERVICE_UUID] }],
        optionalServices: [NUS_SERVICE_UUID],
      });

      deviceRef.current = device;
      device.addEventListener('gattserverdisconnected', () => {
        setConnectedName(null);
        rxCharRef.current = null;
        appendLog('Koneksi BLE terputus dari ESP32 CYD.');
      });

      const server = await device.gatt.connect();
      const service = await server.getPrimaryService(NUS_SERVICE_UUID);
      const rx = await service.getCharacteristic(NUS_RX_UUID);
      const tx = await service.getCharacteristic(NUS_TX_UUID);

      rxCharRef.current = rx;

      await tx.startNotifications();
      tx.addEventListener('characteristicvaluechanged', (event: any) => {
        const val = new TextDecoder().decode(event.target.value);
        appendLog(`CYD Notify: ${val.trim()}`);
      });

      setConnectedName(device.name || 'DisplayConnect-CYD');
      appendLog(`Terhubung ke ${device.name || 'CYD'} (Nordic NUS siap).`);
    } catch (err: any) {
      setErrorMsg(err.message || 'Gagal menghubungkan ke perangkat BLE.');
      appendLog(`Error BLE: ${err.message}`);
    }
  };

  const handleDisconnect = () => {
    if (deviceRef.current?.gatt?.connected) {
      deviceRef.current.gatt.disconnect();
    }
    setConnectedName(null);
    rxCharRef.current = null;
  };

  const sendLineToCyd = async (rawObjOrText: string) => {
    if (!rxCharRef.current) {
      setErrorMsg('Hubungkan ke ESP32 CYD terlebih dahulu melalui BLE.');
      return;
    }
    setErrorMsg(null);
    try {
      const compact = JSON.stringify(JSON.parse(rawObjOrText)) + '\n';
      const bytes = new TextEncoder().encode(compact);
      const chunkSize = 180;
      for (let i = 0; i < bytes.length; i += chunkSize) {
        const chunk = bytes.slice(i, i + chunkSize);
        await rxCharRef.current.writeValue(chunk);
      }
      appendLog(`Terkirim ${bytes.length} bytes ke CYD.`);
    } catch (err: any) {
      setErrorMsg(`Gagal kirim JSON: ${err.message}`);
    }
  };

  return (
    <div className="max-w-7xl mx-auto px-6 py-6 space-y-6">
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4 pb-5 border-b border-slate-800">
        <div>
          <h2 className="text-xl font-bold text-white">Uji Protokol BLE Nordic NUS ke ESP32 CYD</h2>
          <p className="text-sm text-slate-400 mt-1">
            Setelah kode firmware di-upload ke ESP32 CYD, hubungkan langsung lewat Chrome Web Bluetooth untuk menguji paket JSON navigasi di layar TFT.
          </p>
        </div>

        <div className="flex items-center gap-3 shrink-0">
          {connectedName ? (
            <button
              onClick={handleDisconnect}
              className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-rose-600 rounded-lg hover:bg-rose-500 transition-colors whitespace-nowrap shrink-0"
            >
              <Unplug className="w-4 h-4" />
              Putuskan ({connectedName})
            </button>
          ) : (
            <button
              onClick={handleConnectBle}
              className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 transition-colors whitespace-nowrap shrink-0"
            >
              <Bluetooth className="w-4 h-4" />
              Scan & Hubungkan CYD
            </button>
          )}
        </div>
      </div>

      {errorMsg && (
        <div className="flex items-start gap-3 rounded-xl border border-rose-500/40 bg-rose-500/10 p-4 text-xs text-rose-300">
          <AlertCircle className="w-4 h-4 shrink-0 mt-0.5 text-rose-400" />
          <span>{errorMsg}</span>
        </div>
      )}

      {connectedName && (
        <div className="flex items-center gap-2.5 rounded-xl border border-emerald-500/30 bg-emerald-500/10 px-4 py-2.5 text-xs text-emerald-300 font-medium">
          <CheckCircle2 className="w-4 h-4 text-emerald-400" />
          <span>Terhubung aktif ke {connectedName} · Service 6E400001</span>
        </div>
      )}

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-start">
        {/* JSON Editor & Preset Triggers */}
        <div className="lg:col-span-7 rounded-xl border border-slate-800 bg-slate-900/60 overflow-hidden">
          <div className="px-4 py-3 border-b border-slate-800 flex flex-wrap items-center justify-between gap-2">
            <span className="text-xs font-semibold text-white">Payload JSON Protokol DisplayConnect</span>
            <div className="flex flex-wrap items-center gap-2">
              <button
                onClick={() => setJsonPayload(JSON.stringify(SAMPLE_NAV_PACKET, null, 2))}
                className="px-2.5 py-1 text-[11px] font-medium text-slate-300 bg-slate-800 rounded hover:bg-slate-700"
              >
                Sample Nav
              </button>
              <button
                onClick={() => setJsonPayload(JSON.stringify({ type: 'notify', kind: 'whatsapp', sender: 'Budi (Touring)', message: 'Tunggu di SPBU depan!' }, null, 2))}
                className="px-2.5 py-1 text-[11px] font-medium text-emerald-300 bg-emerald-950/60 border border-emerald-800/60 rounded hover:bg-emerald-900/60"
              >
                WhatsApp
              </button>
              <button
                onClick={() => setJsonPayload(JSON.stringify({ type: 'notify', kind: 'call', sender: 'Ibu (+62 812...)', message: 'Panggilan Masuk' }, null, 2))}
                className="px-2.5 py-1 text-[11px] font-medium text-rose-300 bg-rose-950/60 border border-rose-800/60 rounded hover:bg-rose-900/60"
              >
                Telepon
              </button>
              <button
                onClick={() => setJsonPayload(JSON.stringify({ type: 'media', state: 'playing', title: 'Gallant Char (Remix)', artist: 'Zeon Synthwave', app: 'SPOTIFY' }, null, 2))}
                className="px-2.5 py-1 text-[11px] font-medium text-amber-300 bg-amber-950/60 border border-amber-800/60 rounded hover:bg-amber-900/60"
              >
                Musik
              </button>
              <button
                onClick={() => setJsonPayload(JSON.stringify({ type: 'loading' }, null, 2))}
                className="px-2.5 py-1 text-[11px] font-medium text-slate-300 bg-slate-800 rounded hover:bg-slate-700"
              >
                Loading
              </button>
              <button
                onClick={() => setJsonPayload(JSON.stringify({ type: 'config', lang: 'id' }, null, 2))}
                className="px-2.5 py-1 text-[11px] font-medium text-slate-300 bg-slate-800 rounded hover:bg-slate-700"
              >
                Config ID
              </button>
            </div>
          </div>

          <textarea
            value={jsonPayload}
            onChange={(e) => setJsonPayload(e.target.value)}
            rows={18}
            className="w-full bg-slate-950 p-4 text-xs font-mono text-cyan-200 focus:outline-none resize-y"
          />

          <div className="px-4 py-3 border-t border-slate-800 bg-slate-900 flex justify-end">
            <button
              onClick={() => sendLineToCyd(jsonPayload)}
              className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 transition-colors"
            >
              <Send className="w-3.5 h-3.5" />
              Kirim JSON ke Layar CYD
            </button>
          </div>
        </div>

        {/* BLE Communication Log */}
        <div className="lg:col-span-5 rounded-xl border border-slate-800 bg-slate-950 overflow-hidden">
          <div className="px-4 py-3 border-b border-slate-800 bg-slate-900/90 flex items-center justify-between">
            <span className="text-xs font-semibold text-white">Log Transmisi BLE</span>
            <button
              onClick={() => setStatusLog([])}
              className="text-[11px] text-slate-400 hover:text-white"
            >
              Bersihkan
            </button>
          </div>
          <div className="h-[420px] overflow-y-auto p-4 font-mono text-xs space-y-2">
            {statusLog.length === 0 ? (
              <div className="text-slate-500">
                Belum ada aktivitas BLE. Klik "Scan & Hubungkan CYD" setelah board ESP32 dinyalakan.
              </div>
            ) : (
              statusLog.map((line, i) => (
                <div key={i} className="text-slate-300 border-b border-slate-900 pb-1.5 break-all">
                  {line}
                </div>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
