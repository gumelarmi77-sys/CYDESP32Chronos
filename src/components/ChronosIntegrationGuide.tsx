import React, { useState, useMemo } from 'react';
import { CYDHardwareConfig } from '../types';
import {
  Check,
  ChevronLeft,
  ChevronRight,
  Copy,
  Bluetooth,
  Navigation,
  Bell,
  Smartphone,
  Cpu,
  ShieldCheck,
  AlertTriangle,
  Play,
  ArrowRight,
  Wrench,
  Music,
  PhoneIncoming,
  MessageSquare,
  ExternalLink,
  Code2,
} from 'lucide-react';

interface ChronosIntegrationGuideProps {
  config: CYDHardwareConfig;
  onGoToBleTest: () => void;
  onGoToPreview: () => void;
  onGoToWiring: () => void;
}

type ChronosEventKey = 'navigation' | 'whatsapp' | 'ringer' | 'music';
type CodeTabKey = 'bridge' | 'platformio';

interface StepCheckItem {
  id: string;
  label: string;
  detail: string;
}

interface GuideStep {
  id: number;
  title: string;
  shortTitle: string;
  summary: string;
  checklist: StepCheckItem[];
}

const GUIDE_STEPS: GuideStep[] = [
  {
    id: 1,
    title: '01. Arsitektur Chronos App ke ESP32 CYD',
    shortTitle: 'Arsitektur',
    summary:
      'Smartphone Android cukup menjalankan aplikasi resmi Chronos (fbiego/chronos-esp32). Chronos membaca navigasi Google Maps, WhatsApp, panggilan masuk, dan baterai HP lalu mengirimnya via BLE ke ESP32 CYD.',
    checklist: [
      {
        id: 'c-arch-1',
        label: 'Instal aplikasi Chronos (v3.7.5+) dari Google Play Store di smartphone Android',
        detail: 'Versi v3.7.5 ke atas membawa fitur bawaan Navigation Forwarding (ikon belokan 48x48, jarak, durasi, ETA, dan nama jalan).',
      },
      {
        id: 'c-arch-2',
        label: 'Gunakan library fbiego/ChronosESP32 di firmware ESP32 CYD',
        detail: 'Chronos Android mengirim paket biner BLE di UUID 6E400001/02/03 sehingga ESP32 CYD memerlukan ChronosESP32 untuk mem-parsing struct Navigation & Notification.',
      },
      {
        id: 'c-arch-3',
        label: 'Opsional: Gunakan GPS NEO-6M di CYD untuk koordinat lat/lon mentah',
        detail: 'Chronos mengirim teks & ikon belokan Google Maps; jika ingin koordinat satelit & kompas mandiri, aktifkan modul GPS NEO-6M pada GPIO RX/TX CYD.',
      },
    ],
  },
  {
    id: 2,
    title: '02. Izin Android & Pengaturan Aplikasi Chronos',
    shortTitle: 'Izin & Setup',
    summary:
      'Chronos membaca banner navigasi Google Maps dan pesan WhatsApp melalui NotificationListenerService Android. Semua izin sistem wajib diaktifkan agar data mengalir real-time saat layar HP mati.',
    checklist: [
      {
        id: 'c-perm-1',
        label: 'Berikan Notification Access ke Chronos (Tab Notifications → Self Test)',
        detail: 'Buka Chronos → tab Notifications → ketuk Self Test. Jika diminta, aktifkan Akses Notifikasi untuk Chronos di Pengaturan Android.',
      },
      {
        id: 'c-perm-2',
        label: 'Aktifkan toggle Navigation di menu Device Settings Chronos',
        detail: 'Setelah terhubung ke ESP32, masuk ke pengaturan perangkat di Chronos lalu nyalakan opsi Navigation (toggle off/on jika belum merespons).',
      },
      {
        id: 'c-perm-3',
        label: 'Berikan izin Telepon (Phone/Call) & Nearby Devices (Bluetooth)',
        detail: 'Dibutuhkan agar setRingerCallback() di ESP32 CYD menerima nama penelepon masuk secara instan.',
      },
      {
        id: 'c-perm-4',
        label: 'Matikan Optimasi Baterai (Unrestricted) untuk aplikasi Chronos',
        detail: 'Mencegah Android Doze Mode memutus koneksi BLE atau menunda pembaruan belokan Google Maps saat HP disimpan di saku.',
      },
    ],
  },
  {
    id: 3,
    title: '03. Pairing BLE & Pemilihan Notifikasi di Chronos',
    shortTitle: 'Pairing BLE',
    summary:
      'Hubungkan aplikasi Chronos ke ESP32 CYD melalui Bluetooth Low Energy dan pilih aplikasi sumber notifikasi yang ingin diteruskan ke layar stang motor.',
    checklist: [
      {
        id: 'c-pair-1',
        label: 'Nyalakan Bluetooth & GPS HP, lalu Scan perangkat di aplikasi Chronos',
        detail: 'Pilih nama perangkat BLE sesuai config.h (default: DisplayConnect-CYD) dari daftar pindaian Chronos App.',
      },
      {
        id: 'c-pair-2',
        label: 'Centang WhatsApp & aplikasi pesan penting di App Notifications Chronos',
        detail: 'Google Maps otomatis terbaca oleh listener navigasi selama Notification Access aktif; WhatsApp & WhatsApp Business perlu dicentang di daftar filter aplikasi.',
      },
      {
        id: 'c-pair-3',
        label: 'Verifikasi sinkronisasi jam, baterai HP, dan Self Test Notification',
        detail: 'Saat status di Chronos menjadi Connected, jam CYD dan persentase baterai HP (watch.getPhoneBattery()) langsung tersinkronisasi.',
      },
    ],
  },
  {
    id: 4,
    title: '04. Flash Firmware CYD (Sudah Terintegrasi ChronosESP32)',
    shortTitle: 'Upload Firmware',
    summary:
      'Firmware DisplayConnect Moto CYD di aplikasi ini sudah dilengkapi modul chronos_bridge.cpp dan ENABLE_CHRONOS_APP = 1 secara bawaan. Anda cukup mengunduh .ZIP lalu meng-upload ke ESP32 CYD.',
    checklist: [
      {
        id: 'c-fw-1',
        label: 'Klik Download .ZIP lalu ekstrak folder firmware ESP32 CYD',
        detail: 'Di dalam ZIP sudah tersedia chronos_bridge.h/.cpp dan platformio.ini dengan dependensi fbiego/ChronosESP32@^1.9.1.',
      },
      {
        id: 'c-fw-2',
        label: 'Compile & Upload via PlatformIO (disarankan) atau Arduino IDE (install ChronosESP32 + ESP32Time)',
        detail: 'Pastikan Partition Scheme memakai Huge APP (3MB No OTA) agar NimBLE + ChronosESP32 + TFT_eSPI muat.',
      },
      {
        id: 'c-fw-3',
        label: 'Pastikan config.h memiliki #define ENABLE_CHRONOS_APP 1',
        detail: 'Saat bernilai 1, layar standby CYD menampilkan "HUBUNGKAN APP CHRONOS" dan langsung menerima paket Chronos Android.',
      },
      {
        id: 'c-fw-4',
        label: 'Uji tombol layar sentuh mode MUS untuk kontrol musik HP via watch.musicControl()',
        detail: 'Sentuhan Next/Prev/Play/Pause/Vol pada layar CYD otomatis diteruskan balik ke pemutar musik smartphone melalui Chronos.',
      },
    ],
  },
  {
    id: 5,
    title: '05. Uji Navigasi Google Maps & Diagnostik Lapangan',
    shortTitle: 'Uji Real-Time',
    summary:
      'Jalankan rute Google Maps di smartphone, uji notifikasi WhatsApp, dan pastikan seluruh aliran data berjalan stabil sebelum berkendara.',
    checklist: [
      {
        id: 'c-test-1',
        label: 'Mulai navigasi di Google Maps dan pastikan banner belokan aktif di status bar HP',
        detail: 'Chronos mengekstrak panah manuver, jarak ke belokan, ETA, dan nama jalan dari notifikasi navigasi aktif Google Maps.',
      },
      {
        id: 'c-test-2',
        label: 'Uji layar HP terkunci selama 2 menit saat navigasi berjalan',
        detail: 'Pastikan data jarak belokan dan notifikasi WhatsApp tetap masuk ke layar ESP32 CYD tanpa jeda.',
      },
    ],
  },
];

interface TroubleItem {
  id: string;
  symptom: string;
  cause: string;
  fix: string;
}

const TROUBLESHOOTING_ITEMS: TroubleItem[] = [
  {
    id: 'nav-toggle-off',
    symptom: 'Opsi Navigation di aplikasi Chronos abu-abu atau tidak mengirim data belokan',
    cause: 'Izin Notification Access belum aktif atau state koneksi awal belum memicu listener navigasi di Chronos.',
    fix: 'Buka tab Notifications di Chronos → ketuk Self Test untuk memverifikasi izin akses notifikasi. Masuk ke Device Settings Chronos lalu toggle OFF kemudian ON kembali pada opsi Navigation.',
  },
  {
    id: 'gmaps-banner',
    symptom: 'Google Maps sedang rute, tapi nav.active di ESP32 tetap false',
    cause: 'Notifikasi Google Maps di-mute pada pengaturan sistem Android atau menggunakan Android 15 ProgressStyle baru yang memodifikasi template notifikasi.',
    fix: 'Pastikan notifikasi Google Maps (kategori Navigation) diizinkan tampil di status bar Android. Jika hanya muncul ikon dasar di Android terbaru, perbarui library fbiego/ChronosESP32 & aplikasi Chronos ke rilis terbaru.',
  },
  {
    id: 'wa-not-showing',
    symptom: 'Navigasi tampil normal, tetapi pesan WhatsApp tidak diteruskan ke CYD',
    cause: 'Aplikasi WhatsApp belum dicentang di daftar filter Notifications Chronos atau mode Jangan Ganggu (DND) aktif.',
    fix: 'Buka Chronos → Notifications → App List → aktifkan WhatsApp (com.whatsapp) / WA Business. Pastikan callback setNotificationCallback() menangani UTF-8 dengan aman.',
  },
  {
    id: 'ble-disconnect',
    symptom: 'Koneksi BLE sering terputus beberapa menit setelah layar HP dikunci',
    cause: 'Sistem penghemat baterai OEM Android membunuh background service Chronos.',
    fix: 'Setel baterai aplikasi Chronos ke "Tanpa pembatasan" (Unrestricted), aktifkan Autostart jika tersedia, dan kunci aplikasi Chronos di menu Recent Apps.',
  },
];

export const ChronosIntegrationGuide: React.FC<ChronosIntegrationGuideProps> = ({
  config,
  onGoToBleTest,
  onGoToPreview,
  onGoToWiring,
}) => {
  const [activeStep, setActiveStep] = useState<number>(1);
  const [checkedMap, setCheckedMap] = useState<Record<string, boolean>>({});
  const [useHwGpsFallback, setUseHwGpsFallback] = useState<boolean>(config.enableHwGps);
  const [enabledChannels, setEnabledChannels] = useState({
    navigation: true,
    whatsapp: true,
    ringer: true,
    music: true,
  });
  const [selectedEvent, setSelectedEvent] = useState<ChronosEventKey>('navigation');
  const [codeTab, setCodeTab] = useState<CodeTabKey>('bridge');
  const [copiedId, setCopiedId] = useState<string | null>(null);
  const [selectedTroubleId, setSelectedTroubleId] = useState<string>('nav-toggle-off');

  const totalChecklistItems = useMemo(
    () => GUIDE_STEPS.reduce((acc, s) => acc + s.checklist.length, 0),
    []
  );

  const completedCount = useMemo(
    () => Object.values(checkedMap).filter(Boolean).length,
    [checkedMap]
  );

  const progressPct = Math.round((completedCount / totalChecklistItems) * 100);
  const currentStepData = GUIDE_STEPS.find((s) => s.id === activeStep) || GUIDE_STEPS[0];

  const toggleCheck = (id: string) => {
    setCheckedMap((prev) => ({ ...prev, [id]: !prev[id] }));
  };

  const toggleChannel = (key: keyof typeof enabledChannels) => {
    setEnabledChannels((prev) => ({ ...prev, [key]: !prev[key] }));
  };

  const chronosBridgeCode = useMemo(
    () => `#include <ChronosESP32.h>
#include "nav_types.h"

ChronosESP32 watch("${config.bleDeviceName}");

void onChronosNotification(Notification n) {
  // Terima WhatsApp / SMS / Telegram dari Chronos App
  if (n.app.indexOf("WhatsApp") >= 0) {
    activeNotify.active = true;
    activeNotify.kind = NOTIFY_WHATSAPP;
    activeNotify.shownAtMs = millis();
    activeNotify.durationMs = 3800;
    strlcpy(activeNotify.sender, n.title.c_str(), sizeof(activeNotify.sender));
    strlcpy(activeNotify.message, n.message.c_str(), sizeof(activeNotify.message));
  }
}

void onChronosRinger(String caller, bool incoming) {
  if (incoming) {
    activeNotify.active = true;
    activeNotify.kind = NOTIFY_CALL;
    activeNotify.shownAtMs = millis();
    activeNotify.durationMs = 6000;
    strlcpy(activeNotify.sender, caller.c_str(), sizeof(activeNotify.sender));
    strlcpy(activeNotify.message, "Panggilan Masuk", sizeof(activeNotify.message));
  } else if (activeNotify.kind == NOTIFY_CALL) {
    activeNotify.active = false;
  }
}

void setupChronosBridge() {
  watch.setNotificationCallback(onChronosNotification);
  watch.setRingerCallback(onChronosRinger);
  watch.begin();
}

void loopChronosBridge(NavState& navState, HwGpsData& hwGps) {
  watch.loop();
  if (!watch.isConnected()) return;

  navState.phoneBattery = watch.getPhoneBattery();
  snprintf(navState.timeStr, sizeof(navState.timeStr), "%02d:%02d",
           watch.getHourC(), watch.getMinute());

  Navigation& cNav = watch.getNavigation();
  if (cNav.active) {
    navState.valid = true;
    strlcpy(navState.instruction, cNav.directions.c_str(), sizeof(navState.instruction));
    strlcpy(navState.street, cNav.title.c_str(), sizeof(navState.street));
    navState.distanceM = cNav.distance.toInt();
    navState.speedKmh = hwGps.fixValid ? hwGps.speedKmh : cNav.speed.toFloat();
  }
}`,
    [config.bleDeviceName]
  );

  const platformioSnippet = useMemo(
    () => `[env:esp32-cyd-moto]
platform = espressif32
board = esp32dev
framework = arduino
board_build.partitions = huge_app.csv

lib_deps =
  bodmer/TFT_eSPI@^2.5.43
  h2zero/NimBLE-Arduino@^2.1.2
  fbiego/ChronosESP32@^1.7.0
  paulstoffregen/XPT2046_Touchscreen@^1.4
  mikalhart/TinyGPSPlus@^1.1.0`,
    []
  );

  const eventSimulationMap: Record<
    ChronosEventKey,
    {
      label: string;
      chronosStruct: string;
      cydMapping: string;
      payloadSize: string;
    }
  > = {
    navigation: {
      label: 'Google Maps via Chronos',
      chronosStruct: `Navigation nav = watch.getNavigation();
nav.active     = true;
nav.title      = "Jl. Jend. Sudirman";
nav.directions = "Belok kanan ke Jl. Jend. Sudirman";
nav.distance   = "120 m";
nav.duration   = "9 min";
nav.eta        = "14:25";
nav.speed      = "${useHwGpsFallback ? '54 km/h (NEO-6M + Chronos)' : '54 km/h'}";
nav.hasIcon    = true; // 48x48 1-bit bitmap (288 bytes)`,
      cydMapping: `NavState (DisplayConnect CYD):
  valid        = true
  street       = "Jl. Jend. Sudirman"
  instruction  = "Belok kanan ke Jl. Jend. Sudirman"
  distanceM    = 120
  speedKmh     = 54.0
  phoneBattery = ${86}% (watch.getPhoneBattery())
  buzzerAlert  = ${config.enableTurnBuzzer ? `Aktif saat <= ${config.turnAlertDistM}m (GPIO ${config.buzzerPin})` : 'Nonaktif'}`,
      payloadSize: '288 B icon + ~96 B teks',
    },
    whatsapp: {
      label: 'Notifikasi WhatsApp',
      chronosStruct: `void notificationCallback(Notification n) {
  n.app     = "WhatsApp";
  n.time    = "14:18";
  n.title   = "Raka (Touring)";
  n.message = "Titik kumpul SPBU depan 500m lagi";
  n.icon    = 8;
}`,
      cydMapping: `NotifyState (Toast Bawah CYD):
  active     = true
  kind       = NOTIFY_WHATSAPP
  sender     = "Raka (Touring)"
  message    = "Titik kumpul SPBU depan 500m lagi"
  durationMs = 3800`,
      payloadSize: '~84 bytes BLE frame',
    },
    ringer: {
      label: 'Panggilan Telepon Masuk',
      chronosStruct: `void ringerCallback(String caller, bool state) {
  caller = "Budi (+62 812-3456-7890)";
  state  = true; // true = ringing, false = ended
}`,
      cydMapping: `NotifyState (Banner Prioritas CYD):
  active     = true
  kind       = NOTIFY_CALL
  sender     = "Budi (+62 812-3456-7890)"
  message    = "Panggilan Masuk"
  durationMs = 6000`,
      payloadSize: '~42 bytes BLE frame',
    },
    music: {
      label: 'Kontrol Musik Dua Arah',
      chronosStruct: `// Dari layar sentuh CYD mode MUS ke Chronos App:
watch.musicControl(MUSIC_NEXT);
watch.musicControl(MUSIC_TOGGLE);
watch.musicControl(VOLUME_UP);`,
      cydMapping: `Touch CYD (Mode VIEW_MEDIA_PLAYER):
  Tap NEXT   -> watch.musicControl(MUSIC_NEXT)
  Tap PLAY   -> watch.musicControl(MUSIC_TOGGLE)
  Tap VOL+   -> watch.musicControl(VOLUME_UP)
  TX UUID    -> 6E400003-B5A3-F393-E0A9-E50E24DCCA9E`,
      payloadSize: '7 bytes command packet',
    },
  };

  const activeSim = eventSimulationMap[selectedEvent];

  const handleCopy = async (text: string, id: string) => {
    try {
      await navigator.clipboard.writeText(text);
      setCopiedId(id);
      setTimeout(() => setCopiedId(null), 1500);
    } catch {
      // Clipboard fallback ignored cleanly
    }
  };

  const activeTrouble =
    TROUBLESHOOTING_ITEMS.find((item) => item.id === selectedTroubleId) || TROUBLESHOOTING_ITEMS[0];

  return (
    <div className="max-w-7xl mx-auto px-6 py-6 space-y-8">
      {/* Header */}
      <div className="flex flex-col lg:flex-row lg:items-end justify-between gap-4 pb-5 border-b border-slate-800">
        <div className="space-y-1.5">
          <div className="flex flex-wrap items-center gap-2 text-xs text-cyan-400 font-medium">
            <span>Android Companion: Chronos App</span>
            <span aria-hidden="true">·</span>
            <span>fbiego/chronos-esp32</span>
            <span aria-hidden="true">·</span>
            <span className="font-mono tabular-nums">{config.bleDeviceName}</span>
          </div>
          <h1 className="text-2xl font-bold tracking-tight text-white">
            Panduan Integrasi Aplikasi Chronos ke DisplayConnect CYD
          </h1>
          <p className="text-sm text-slate-400 max-w-3xl">
            Gunakan aplikasi resmi Chronos di smartphone Android tanpa perlu aplikasi pendamping tambahan untuk meneruskan navigasi Google Maps, WhatsApp, panggilan masuk, dan kontrol musik secara real-time ke ESP32 CYD.
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-3 shrink-0">
          <div className="text-right pr-3 border-r border-slate-800">
            <div className="text-[11px] text-slate-400">Progres Setup</div>
            <div className="text-sm font-bold font-mono tabular-nums text-white">
              {completedCount}/{totalChecklistItems} langkah ({progressPct}%)
            </div>
          </div>
          <a
            href="https://github.com/fbiego/chronos-esp32"
            target="_blank"
            rel="noopener noreferrer"
            className="flex items-center gap-1.5 px-3.5 py-2 text-xs font-medium text-slate-200 bg-slate-900 border border-slate-800 rounded-lg hover:bg-slate-800 transition-colors whitespace-nowrap shrink-0"
          >
            <span>Repo chronos-esp32</span>
            <ExternalLink className="w-3.5 h-3.5" />
          </a>
        </div>
      </div>

      {/* Progress & Stepper */}
      <div className="space-y-3">
        <div className="h-1.5 w-full bg-slate-900 rounded-full overflow-hidden border border-slate-800">
          <div
            className="h-full bg-cyan-500 transition-transform duration-300 origin-left"
            style={{ transform: `scaleX(${Math.max(0.02, progressPct / 100)})` }}
          />
        </div>

        <div className="grid grid-cols-2 sm:grid-cols-5 gap-2">
          {GUIDE_STEPS.map((step) => {
            const stepDone = step.checklist.filter((c) => checkedMap[c.id]).length;
            const isAllDone = stepDone === step.checklist.length;
            const isActive = activeStep === step.id;
            return (
              <button
                key={step.id}
                onClick={() => setActiveStep(step.id)}
                className={`flex items-center justify-between gap-2 px-3.5 py-2.5 rounded-lg border text-left transition-colors ${
                  isActive
                    ? 'bg-cyan-500/10 border-cyan-500 text-white'
                    : 'bg-slate-900/60 border-slate-800 text-slate-400 hover:text-slate-200 hover:border-slate-700'
                }`}
              >
                <div className="min-w-0">
                  <div className="text-xs font-semibold truncate">
                    {step.id}. {step.shortTitle}
                  </div>
                  <div className="text-[11px] text-slate-400 font-mono tabular-nums mt-0.5">
                    {stepDone}/{step.checklist.length} selesai
                  </div>
                </div>
                {isAllDone && <Check className="w-4 h-4 text-emerald-400 shrink-0" />}
              </button>
            );
          })}
        </div>
      </div>

      {/* 2-Column Workspace */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8 items-start">
        {/* Left Column: Step Details + Interactive Widgets + Checklist */}
        <div className="lg:col-span-7 space-y-6">
          <div className="rounded-xl border border-slate-800 bg-slate-900/60 p-6 space-y-5">
            <div className="space-y-2">
              <h2 className="text-lg font-bold text-white">{currentStepData.title}</h2>
              <p className="text-xs text-slate-300 leading-relaxed">{currentStepData.summary}</p>
            </div>

            {/* Step 1 Interactive: GPS Source & Protocol Compatibility */}
            {activeStep === 1 && (
              <div className="pt-3 border-t border-slate-800 space-y-3">
                <div className="text-xs font-semibold text-slate-200">
                  Sumber Data GPS &amp; Koordinat pada Konfigurasi Anda:
                </div>
                <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                  <button
                    onClick={() => setUseHwGpsFallback(false)}
                    className={`p-3.5 rounded-lg border text-left transition-colors ${
                      !useHwGpsFallback
                        ? 'border-cyan-500 bg-cyan-500/10 text-white'
                        : 'border-slate-800 bg-slate-950 text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <div className="text-xs font-semibold">Chronos Saja (Tanpa Modul GPS)</div>
                    <div className="text-[11px] text-slate-400 mt-1 leading-relaxed">
                      Mengambil instruksi belokan Google Maps, jarak, ETA, durasi, kecepatan teks, &amp; ikon 48×48 langsung dari HP via Chronos App.
                    </div>
                  </button>
                  <button
                    onClick={() => setUseHwGpsFallback(true)}
                    className={`p-3.5 rounded-lg border text-left transition-colors ${
                      useHwGpsFallback
                        ? 'border-cyan-500 bg-cyan-500/10 text-white'
                        : 'border-slate-800 bg-slate-950 text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <div className="text-xs font-semibold">Chronos + GPS NEO-6M (CN1/P3)</div>
                    <div className="text-[11px] text-slate-400 mt-1 leading-relaxed">
                      Chronos mengirim belokan &amp; notifikasi; modul NEO-6M di GPIO {config.gpsRxPin}/{config.gpsTxPin} memasok lat/lon, kompas, &amp; speedometer presisi.
                    </div>
                  </button>
                </div>

                <div className="rounded-lg border border-slate-800 bg-slate-950 p-3.5 text-xs text-slate-300 leading-relaxed">
                  <span className="font-semibold text-cyan-300">Catatan Protokol BLE: </span>
                  Chronos menggunakan UUID Nordic UART yang sama (<code className="text-cyan-400">6E400001-B5A3-F393-E0A9-E50E24DCCA9E</code>) dengan DisplayConnect, tetapi format pesannya memakai frame biner <code className="text-cyan-400">ChronosESP32</code>—bukan JSON teks mentah. Karena itu di sisi ESP32 perlu memakai library <code className="text-cyan-400">fbiego/ChronosESP32</code> (lihat Langkah 4).
                </div>
              </div>
            )}

            {/* Step 2 Interactive: Chronos Android Permissions Table */}
            {activeStep === 2 && (
              <div className="pt-3 border-t border-slate-800 space-y-2.5">
                <div className="text-xs font-semibold text-slate-200 flex items-center gap-2">
                  <ShieldCheck className="w-4 h-4 text-cyan-400" />
                  Pengaturan Wajib di Aplikasi Chronos Android (v3.7.5+)
                </div>
                <div className="overflow-x-auto rounded-lg border border-slate-800">
                  <table className="w-full text-left text-xs">
                    <thead className="bg-slate-950 text-slate-400 border-b border-slate-800">
                      <tr>
                        <th className="px-3 py-2 font-semibold">Menu Chronos / Android</th>
                        <th className="px-3 py-2 font-semibold">Data yang Diteruskan</th>
                        <th className="px-3 py-2 font-semibold">Status</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-slate-800 bg-slate-950/60">
                      <tr>
                        <td className="px-3 py-2 font-medium text-cyan-300">Notification Access</td>
                        <td className="px-3 py-2 text-slate-300">Banner belokan Google Maps &amp; WhatsApp</td>
                        <td className="px-3 py-2 text-emerald-400">Wajib Aktif</td>
                      </tr>
                      <tr>
                        <td className="px-3 py-2 font-medium text-cyan-300">Device Settings → Navigation</td>
                        <td className="px-3 py-2 text-slate-300">Aktifkan parser ikon 48×48, ETA &amp; jarak</td>
                        <td className="px-3 py-2 text-emerald-400">Toggle ON</td>
                      </tr>
                      <tr>
                        <td className="px-3 py-2 font-medium text-cyan-300">Phone &amp; Contacts Permission</td>
                        <td className="px-3 py-2 text-slate-300">Nama/nomor penelepon ke setRingerCallback</td>
                        <td className="px-3 py-2 text-emerald-400">Izinkan</td>
                      </tr>
                      <tr>
                        <td className="px-3 py-2 font-medium text-cyan-300">Battery → Unrestricted</td>
                        <td className="px-3 py-2 text-slate-300">Jaga koneksi BLE saat layar HP terkunci</td>
                        <td className="px-3 py-2 text-emerald-400">Tanpa Batas</td>
                      </tr>
                    </tbody>
                  </table>
                </div>
              </div>
            )}

            {/* Step 3 Interactive: BLE Pairing & App Filter */}
            {activeStep === 3 && (
              <div className="pt-3 border-t border-slate-800 space-y-3">
                <div className="flex items-center justify-between">
                  <span className="text-xs font-semibold text-slate-200">
                    Parameter Koneksi BLE Chronos ↔ ESP32 CYD
                  </span>
                  <button
                    onClick={() =>
                      handleCopy(
                        `DEVICE=${config.bleDeviceName}\nCS_SERVICE_UUID=6e400001-b5a3-f393-e0a9-e50e24dcca9e\nCS_CHARACTERISTIC_UUID_RX=6e400002-b5a3-f393-e0a9-e50e24dcca9e\nCS_CHARACTERISTIC_UUID_TX=6e400003-b5a3-f393-e0a9-e50e24dcca9e`,
                        'chronos-uuids'
                      )
                    }
                    className="flex items-center gap-1.5 text-[11px] font-medium text-cyan-400 hover:text-cyan-300"
                  >
                    {copiedId === 'chronos-uuids' ? (
                      <>
                        <Check className="w-3.5 h-3.5 text-emerald-400" />
                        <span>Tersalin</span>
                      </>
                    ) : (
                      <>
                        <Copy className="w-3.5 h-3.5" />
                        <span>Salin UUID</span>
                      </>
                    )}
                  </button>
                </div>

                <div className="rounded-lg border border-slate-800 bg-slate-950 p-3.5 font-mono text-[11px] space-y-1.5 text-slate-300 overflow-x-auto">
                  <div>
                    <span className="text-slate-500">BLE Advertising Name : </span>
                    <span className="text-white font-semibold">{config.bleDeviceName}</span>
                  </div>
                  <div>
                    <span className="text-slate-500">CS_SERVICE_UUID      : </span>
                    <span className="text-cyan-300">6e400001-b5a3-f393-e0a9-e50e24dcca9e</span>
                  </div>
                  <div>
                    <span className="text-slate-500">CS_CHARACTERISTIC_RX : </span>
                    <span className="text-emerald-300">6e400002-b5a3-f393-e0a9-e50e24dcca9e</span>
                  </div>
                  <div>
                    <span className="text-slate-500">CS_CHARACTERISTIC_TX : </span>
                    <span className="text-amber-300">6e400003-b5a3-f393-e0a9-e50e24dcca9e</span>
                  </div>
                </div>
              </div>
            )}

            {/* Step 4 Interactive: Ready-to-copy ChronosESP32 Bridge Code */}
            {activeStep === 4 && (
              <div className="pt-3 border-t border-slate-800 space-y-3">
                <div className="flex flex-wrap items-center justify-between gap-2">
                  <div className="flex items-center gap-1.5 bg-slate-950 p-1 rounded-lg border border-slate-800">
                    <button
                      onClick={() => setCodeTab('bridge')}
                      className={`px-3 py-1 text-xs font-medium rounded-md transition-colors ${
                        codeTab === 'bridge'
                          ? 'bg-cyan-600 text-white'
                          : 'text-slate-400 hover:text-white'
                      }`}
                    >
                      chronos_bridge.cpp
                    </button>
                    <button
                      onClick={() => setCodeTab('platformio')}
                      className={`px-3 py-1 text-xs font-medium rounded-md transition-colors ${
                        codeTab === 'platformio'
                          ? 'bg-cyan-600 text-white'
                          : 'text-slate-400 hover:text-white'
                      }`}
                    >
                      platformio.ini
                    </button>
                  </div>

                  <button
                    onClick={() =>
                      handleCopy(
                        codeTab === 'bridge' ? chronosBridgeCode : platformioSnippet,
                        'code-copy'
                      )
                    }
                    className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium text-cyan-400 bg-slate-950 border border-slate-800 rounded-lg hover:text-cyan-300"
                  >
                    {copiedId === 'code-copy' ? (
                      <>
                        <Check className="w-3.5 h-3.5 text-emerald-400" />
                        <span>Tersalin</span>
                      </>
                    ) : (
                      <>
                        <Copy className="w-3.5 h-3.5" />
                        <span>Salin Kode</span>
                      </>
                    )}
                  </button>
                </div>

                <pre className="rounded-lg border border-slate-800 bg-slate-950 p-4 text-[11px] font-mono text-cyan-200 overflow-x-auto max-h-72 leading-relaxed">
                  {codeTab === 'bridge' ? chronosBridgeCode : platformioSnippet}
                </pre>
              </div>
            )}

            {/* Step 5 Interactive: Quick Actions */}
            {activeStep === 5 && (
              <div className="pt-3 border-t border-slate-800 flex flex-wrap gap-3">
                <button
                  onClick={onGoToPreview}
                  className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 transition-colors"
                >
                  <Play className="w-3.5 h-3.5" />
                  <span>Pratinjau HUD CYD</span>
                </button>
                <button
                  onClick={onGoToWiring}
                  className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-slate-200 bg-slate-800 rounded-lg hover:bg-slate-700 transition-colors"
                >
                  <Cpu className="w-3.5 h-3.5" />
                  <span>Wiring GPS NEO-6M &amp; Buzzer</span>
                </button>
                <button
                  onClick={onGoToBleTest}
                  className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-slate-200 bg-slate-800 rounded-lg hover:bg-slate-700 transition-colors"
                >
                  <Bluetooth className="w-3.5 h-3.5" />
                  <span>Uji Web BLE</span>
                </button>
              </div>
            )}

            {/* Step Checklist */}
            <div className="space-y-2.5 pt-3 border-t border-slate-800">
              <div className="text-xs font-semibold text-slate-200">
                Checklist Verifikasi Langkah Ini
              </div>
              <div className="space-y-2">
                {currentStepData.checklist.map((item) => {
                  const checked = !!checkedMap[item.id];
                  return (
                    <button
                      key={item.id}
                      onClick={() => toggleCheck(item.id)}
                      className={`w-full flex items-start gap-3 p-3 rounded-lg border text-left transition-colors ${
                        checked
                          ? 'bg-emerald-500/10 border-emerald-500/40'
                          : 'bg-slate-950/70 border-slate-800 hover:border-slate-700'
                      }`}
                    >
                      <span
                        className={`mt-0.5 w-4 h-4 rounded flex items-center justify-center border shrink-0 transition-colors ${
                          checked
                            ? 'bg-emerald-500 border-emerald-400 text-slate-950'
                            : 'border-slate-600 bg-slate-900'
                        }`}
                      >
                        {checked && <Check className="w-3 h-3 stroke-[3]" />}
                      </span>
                      <span className="space-y-1">
                        <span
                          className={`block text-xs font-medium ${
                            checked ? 'text-emerald-200 line-through' : 'text-white'
                          }`}
                        >
                          {item.label}
                        </span>
                        <span className="block text-[11px] text-slate-400 leading-relaxed">
                          {item.detail}
                        </span>
                      </span>
                    </button>
                  );
                })}
              </div>
            </div>

            {/* Prev / Next Navigation */}
            <div className="flex items-center justify-between pt-3 border-t border-slate-800">
              <button
                onClick={() => setActiveStep((s) => Math.max(1, s - 1))}
                disabled={activeStep === 1}
                className="flex items-center gap-1.5 px-3.5 py-2 text-xs font-medium text-slate-300 bg-slate-950 border border-slate-800 rounded-lg hover:bg-slate-800 disabled:opacity-40 transition-colors"
              >
                <ChevronLeft className="w-4 h-4" />
                <span>Sebelumnya</span>
              </button>

              <span className="text-xs font-mono tabular-nums text-slate-400">
                Langkah {activeStep} / {GUIDE_STEPS.length}
              </span>

              <button
                onClick={() => setActiveStep((s) => Math.min(GUIDE_STEPS.length, s + 1))}
                disabled={activeStep === GUIDE_STEPS.length}
                className="flex items-center gap-1.5 px-3.5 py-2 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 disabled:opacity-40 transition-colors"
              >
                <span>Lanjut</span>
                <ChevronRight className="w-4 h-4" />
              </button>
            </div>
          </div>
        </div>

        {/* Right Column: Chronos -> CYD Real-Time Flow & Callback Simulator */}
        <div className="lg:col-span-5 space-y-6">
          <div className="rounded-xl border border-slate-800 bg-slate-900/60 p-5 space-y-4">
            <div className="flex items-center justify-between">
              <h3 className="text-sm font-bold text-white">Alur Data Chronos App → CYD</h3>
              <span className="text-xs font-mono text-cyan-400">Single-App Android</span>
            </div>

            <div className="space-y-2 text-xs">
              <div className="flex items-center justify-between p-3 rounded-lg border border-slate-800 bg-slate-950">
                <div className="flex items-center gap-2.5">
                  <Smartphone className="w-4 h-4 text-cyan-400 shrink-0" />
                  <div>
                    <div className="font-semibold text-white">Google Maps · WhatsApp · Telepon</div>
                    <div className="text-[11px] text-slate-400">
                      Notifikasi &amp; sesi media aktif di Android
                    </div>
                  </div>
                </div>
                <ArrowRight className="w-4 h-4 text-slate-500 shrink-0" />
              </div>

              <div className="flex items-center justify-between p-3 rounded-lg border border-slate-800 bg-slate-950">
                <div className="flex items-center gap-2.5">
                  <Bell className="w-4 h-4 text-amber-400 shrink-0" />
                  <div>
                    <div className="font-semibold text-white">Chronos Android App (v3.7.5+)</div>
                    <div className="text-[11px] text-slate-400">
                      Kirim frame biner Chronos via BLE 6E400002
                    </div>
                  </div>
                </div>
                <ArrowRight className="w-4 h-4 text-slate-500 shrink-0" />
              </div>

              <div className="flex items-center justify-between p-3 rounded-lg border border-cyan-500/40 bg-cyan-500/5">
                <div className="flex items-center gap-2.5">
                  <Cpu className="w-4 h-4 text-emerald-400 shrink-0" />
                  <div>
                    <div className="font-semibold text-white">
                      ChronosESP32 + DisplayConnect HUD ({config.bleDeviceName})
                    </div>
                    <div className="text-[11px] text-slate-400">
                      {useHwGpsFallback
                        ? `Chronos Nav/Notif + GPS NEO-6M (GPIO ${config.gpsRxPin})`
                        : 'Mode Chronos Murni (Tanpa GPS Eksternal)'}
                    </div>
                  </div>
                </div>
                <span className="text-[11px] font-mono tabular-nums text-emerald-400">60 FPS</span>
              </div>
            </div>

            {/* Channel Filter Toggles */}
            <div className="pt-2 border-t border-slate-800 space-y-2">
              <div className="text-xs font-semibold text-slate-300">
                Fitur Chronos yang Diaktifkan di CYD:
              </div>
              <div className="grid grid-cols-2 gap-2 text-xs">
                <button
                  onClick={() => toggleChannel('navigation')}
                  className={`flex items-center justify-between px-3 py-2 rounded-lg border transition-colors ${
                    enabledChannels.navigation
                      ? 'border-cyan-500/50 bg-cyan-500/10 text-white'
                      : 'border-slate-800 bg-slate-950 text-slate-500'
                  }`}
                >
                  <span className="flex items-center gap-1.5">
                    <Navigation className="w-3.5 h-3.5" />
                    Navigasi Maps
                  </span>
                  <span className="font-mono text-[11px]">{enabledChannels.navigation ? 'ON' : 'OFF'}</span>
                </button>

                <button
                  onClick={() => toggleChannel('whatsapp')}
                  className={`flex items-center justify-between px-3 py-2 rounded-lg border transition-colors ${
                    enabledChannels.whatsapp
                      ? 'border-emerald-500/50 bg-emerald-500/10 text-white'
                      : 'border-slate-800 bg-slate-950 text-slate-500'
                  }`}
                >
                  <span className="flex items-center gap-1.5">
                    <MessageSquare className="w-3.5 h-3.5" />
                    WhatsApp
                  </span>
                  <span className="font-mono text-[11px]">{enabledChannels.whatsapp ? 'ON' : 'OFF'}</span>
                </button>

                <button
                  onClick={() => toggleChannel('ringer')}
                  className={`flex items-center justify-between px-3 py-2 rounded-lg border transition-colors ${
                    enabledChannels.ringer
                      ? 'border-rose-500/50 bg-rose-500/10 text-white'
                      : 'border-slate-800 bg-slate-950 text-slate-500'
                  }`}
                >
                  <span className="flex items-center gap-1.5">
                    <PhoneIncoming className="w-3.5 h-3.5" />
                    Panggilan
                  </span>
                  <span className="font-mono text-[11px]">{enabledChannels.ringer ? 'ON' : 'OFF'}</span>
                </button>

                <button
                  onClick={() => toggleChannel('music')}
                  className={`flex items-center justify-between px-3 py-2 rounded-lg border transition-colors ${
                    enabledChannels.music
                      ? 'border-amber-500/50 bg-amber-500/10 text-white'
                      : 'border-slate-800 bg-slate-950 text-slate-500'
                  }`}
                >
                  <span className="flex items-center gap-1.5">
                    <Music className="w-3.5 h-3.5" />
                    Musik Deck
                  </span>
                  <span className="font-mono text-[11px]">{enabledChannels.music ? 'ON' : 'OFF'}</span>
                </button>
              </div>
            </div>
          </div>

          {/* ChronosESP32 Callback & CYD Mapping Simulator */}
          <div className="rounded-xl border border-slate-800 bg-slate-950 overflow-hidden">
            <div className="px-4 py-3 border-b border-slate-800 bg-slate-900/80 flex flex-wrap items-center justify-between gap-2">
              <div className="flex items-center gap-2 text-xs font-bold text-white">
                <Code2 className="w-4 h-4 text-cyan-400" />
                <span>Simulasi Data ChronosESP32</span>
              </div>
              <div className="flex items-center gap-1.5">
                {(['navigation', 'whatsapp', 'ringer', 'music'] as ChronosEventKey[]).map((key) => (
                  <button
                    key={key}
                    onClick={() => setSelectedEvent(key)}
                    className={`px-2.5 py-1 text-[11px] font-medium rounded transition-colors ${
                      selectedEvent === key
                        ? 'bg-cyan-600 text-white'
                        : 'bg-slate-800 text-slate-400 hover:text-white'
                    }`}
                  >
                    {key === 'navigation'
                      ? 'Maps'
                      : key === 'whatsapp'
                      ? 'WhatsApp'
                      : key === 'ringer'
                      ? 'Telepon'
                      : 'Musik'}
                  </button>
                ))}
              </div>
            </div>

            <div className="p-4 space-y-3">
              <div>
                <div className="text-[11px] font-semibold text-slate-400 mb-1.5">
                  1. Data Diterima oleh Library ChronosESP32 ({activeSim.label})
                </div>
                <pre className="p-3 rounded-lg bg-slate-900/90 border border-slate-800 text-[11px] font-mono text-cyan-200 overflow-x-auto leading-relaxed">
                  {activeSim.chronosStruct}
                </pre>
              </div>

              <div>
                <div className="text-[11px] font-semibold text-slate-400 mb-1.5">
                  2. Hasil Pemetaan ke Layar DisplayConnect CYD
                </div>
                <pre className="p-3 rounded-lg bg-slate-900/90 border border-slate-800 text-[11px] font-mono text-emerald-200 overflow-x-auto leading-relaxed">
                  {activeSim.cydMapping}
                </pre>
              </div>
            </div>

            <div className="px-4 py-2.5 border-t border-slate-800 bg-slate-900/60 flex items-center justify-between text-[11px] text-slate-400">
              <span className="font-mono tabular-nums">Estimasi data: {activeSim.payloadSize}</span>
              <button
                onClick={() =>
                  handleCopy(`${activeSim.chronosStruct}\n\n${activeSim.cydMapping}`, 'sim-copy')
                }
                className="flex items-center gap-1.5 text-cyan-400 hover:text-cyan-300 font-medium"
              >
                {copiedId === 'sim-copy' ? (
                  <>
                    <Check className="w-3.5 h-3.5 text-emerald-400" />
                    <span>Tersalin</span>
                  </>
                ) : (
                  <>
                    <Copy className="w-3.5 h-3.5" />
                    <span>Salin Contoh</span>
                  </>
                )}
              </button>
            </div>
          </div>
        </div>
      </div>

      {/* Troubleshooting Matrix */}
      <div className="rounded-xl border border-slate-800 bg-slate-900/60 overflow-hidden">
        <div className="px-6 py-4 border-b border-slate-800 flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Wrench className="w-4 h-4 text-cyan-400" />
            <h3 className="text-sm font-bold text-white">
              Diagnostik Kendala Chronos App &amp; ESP32 CYD
            </h3>
          </div>
          <span className="text-xs text-slate-400">Klik gejala untuk melihat solusi</span>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-12 divide-y lg:divide-y-0 lg:divide-x divide-slate-800">
          <div className="lg:col-span-5 divide-y divide-slate-800">
            {TROUBLESHOOTING_ITEMS.map((item) => {
              const active = item.id === activeTrouble.id;
              return (
                <button
                  key={item.id}
                  onClick={() => setSelectedTroubleId(item.id)}
                  className={`w-full px-5 py-3.5 text-left transition-colors flex items-start gap-3 ${
                    active ? 'bg-cyan-500/10 text-white' : 'hover:bg-slate-800/40 text-slate-300'
                  }`}
                >
                  <AlertTriangle
                    className={`w-4 h-4 shrink-0 mt-0.5 ${
                      active ? 'text-cyan-400' : 'text-amber-400/70'
                    }`}
                  />
                  <span className="text-xs font-medium leading-relaxed">{item.symptom}</span>
                </button>
              );
            })}
          </div>

          <div className="lg:col-span-7 p-6 space-y-4 bg-slate-950/50">
            <div className="space-y-1">
              <div className="text-[11px] font-semibold text-amber-400">Penyebab Utama</div>
              <p className="text-xs text-slate-200 leading-relaxed">{activeTrouble.cause}</p>
            </div>
            <div className="pt-3 border-t border-slate-800 space-y-1">
              <div className="text-[11px] font-semibold text-emerald-400">Langkah Perbaikan</div>
              <p className="text-xs text-slate-300 leading-relaxed">{activeTrouble.fix}</p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
