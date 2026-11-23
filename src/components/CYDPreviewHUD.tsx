import React, { useState, useEffect } from 'react';
import { CYDHardwareConfig } from '../types';
import {
  Play,
  Pause,
  RotateCcw,
  AlertTriangle,
  Bluetooth,
  Satellite,
  Sun,
  Moon,
  MessageSquare,
  PhoneIncoming,
  SkipBack,
  SkipForward,
  Square,
  Volume2,
  Music,
  SunDim,
} from 'lucide-react';

interface CYDPreviewHUDProps {
  config: CYDHardwareConfig;
  onGoToCode: () => void;
}

type ScreenState = 'nav' | 'loading' | 'standby';
type ViewMode = 'vector' | 'hud' | 'media';
type ThemeMode = 'night' | 'day';
type ManeuverType = 'straight' | 'right' | 'left' | 'uturn' | 'arrive';
type PlaybackState = 'playing' | 'paused' | 'stopped';

const SIM_TRACKS = [
  { title: 'Midnight Expressway', artist: 'Tokyo Drift FM', app: 'SPOTIFY' },
  { title: 'City Lights Touring', artist: 'Lo-Fi Rider Mix', app: 'YT MUSIC' },
  { title: 'Wangan Horizon', artist: 'Synthwave 88', app: 'SPOTIFY' },
];

interface SimStep {
  maneuver: ManeuverType;
  instruction: string;
  street: string;
  distanceM: number;
  bearing: number;
  userX: number;
  userY: number;
}

const SIM_STEPS: SimStep[] = [
  {
    maneuver: 'straight',
    instruction: 'Lurus ikuti Jl. Jend. Sudirman',
    street: 'Jl. Jend. Sudirman',
    distanceM: 320,
    bearing: 12,
    userX: 120,
    userY: 186,
  },
  {
    maneuver: 'right',
    instruction: 'Belok kanan ke Jl. M.H. Thamrin',
    street: 'Bundaran HI',
    distanceM: 110,
    bearing: 68,
    userX: 126,
    userY: 150,
  },
  {
    maneuver: 'left',
    instruction: 'Belok kiri ke Jl. Kebon Sirih',
    street: 'Jl. M.H. Thamrin',
    distanceM: 85,
    bearing: 298,
    userX: 108,
    userY: 132,
  },
  {
    maneuver: 'uturn',
    instruction: 'Putar balik di Patung Kuda',
    street: 'Jl. Medan Merdeka Barat',
    distanceM: 60,
    bearing: 185,
    userX: 120,
    userY: 122,
  },
  {
    maneuver: 'arrive',
    instruction: 'Tiba di tujuan (Monas)',
    street: 'Area Monumen Nasional',
    distanceM: 0,
    bearing: 0,
    userX: 120,
    userY: 106,
  },
];

// Palettes matched directly to maps_theme.cpp RGB565 values
const PALETTE: Record<
  ThemeMode,
  {
    label: string;
    land: string;
    park: string;
    grid: string;
    roadEdge: string;
    roadFill: string;
    routeEdge: string;
    route: string;
    card: string;
    cardAlt: string;
    cardBorder: string;
    cardShadow: string;
    text: string;
    muted: string;
    accent: string;
    blue: string;
    switchTrack: string;
    switchKnob: string;
    danger: string;
    success: string;
  }
> = {
  night: {
    label: 'NGT · Dark Slate',
    land: '#12161F',
    park: '#171E2B',
    grid: '#1C2433',
    roadEdge: '#232D40',
    roadFill: '#38465E',
    routeEdge: '#0A2C5C',
    route: '#2997FF',
    card: '#1A2130',
    cardAlt: '#222B3E',
    cardBorder: '#2E3B52',
    cardShadow: '#0A0D13',
    text: '#F4F7FC',
    muted: '#8B9BB4',
    accent: '#FF7A1A',
    blue: '#2997FF',
    switchTrack: '#242E42',
    switchKnob: '#FF7A1A',
    danger: '#FF453A',
    success: '#30D158',
  },
  day: {
    label: 'DAY · Daylight Maps',
    land: '#EBEFF4',
    park: '#D8E8D9',
    grid: '#DEE4ED',
    roadEdge: '#B8C2D1',
    roadFill: '#FFFFFF',
    routeEdge: '#0D47A1',
    route: '#1A73E8',
    card: '#FFFFFF',
    cardAlt: '#F1F5F9',
    cardBorder: '#CBD5E1',
    cardShadow: '#B6C0CE',
    text: '#0F172A',
    muted: '#64748B',
    accent: '#EA580C',
    blue: '#1A73E8',
    switchTrack: '#E2E8F0',
    switchKnob: '#1A73E8',
    danger: '#DC2626',
    success: '#16A34A',
  },
};

export const CYDPreviewHUD: React.FC<CYDPreviewHUDProps> = ({ config, onGoToCode }) => {
  const [theme, setTheme] = useState<ThemeMode>('night');
  const [autoLdr, setAutoLdr] = useState<boolean>(true);
  const [ldrAdc, setLdrAdc] = useState<number>(2600); // >2150 = Night, <950 = Day
  const [viewMode, setViewMode] = useState<ViewMode>('vector');
  const [screenState, setScreenState] = useState<ScreenState>('nav');
  const [bleConnected, setBleConnected] = useState(true);
  const [hwGpsLocked, setHwGpsLocked] = useState(config.enableHwGps);
  const [offRoute, setOffRoute] = useState(false);
  const [brightnessIdx, setBrightnessIdx] = useState<0 | 1 | 2>(2);

  const [stepIdx, setStepIdx] = useState(1);
  const [distanceM, setDistanceM] = useState(SIM_STEPS[1].distanceM);
  const [speedKmh, setSpeedKmh] = useState(54);
  const [satellites, setSatellites] = useState(9);
  const [phoneBattery] = useState(86);
  const [isRunning, setIsRunning] = useState(false);
  const [tachPhase, setTachPhase] = useState(0);
  const [activeNotify, setActiveNotify] = useState<{
    kind: 'whatsapp' | 'call' | 'media';
    sender: string;
    message: string;
  } | null>(null);
  const [trackIdx, setTrackIdx] = useState(0);
  const [playbackState, setPlaybackState] = useState<PlaybackState>('playing');
  const [mediaVolume, setMediaVolume] = useState(75);
  const [lastBleCmd, setLastBleCmd] = useState<string>('TX: {"type":"media_cmd","cmd":"play_pause"}');

  const pal = PALETTE[theme];
  const currentStep = SIM_STEPS[stepIdx];
  const currentTrack = SIM_TRACKS[trackIdx];
  const brightnessLabel = brightnessIdx === 0 ? '25%' : brightnessIdx === 1 ? '65%' : '100%';
  const brightnessOpacity = brightnessIdx === 0 ? 0.58 : brightnessIdx === 1 ? 0.82 : 1;

  useEffect(() => {
    setHwGpsLocked(config.enableHwGps);
  }, [config.enableHwGps]);

  // Simulate LDR GPIO 34 hysteresis switching
  useEffect(() => {
    if (!autoLdr) return;
    if (ldrAdc <= 950) {
      setTheme('day');
    } else if (ldrAdc >= 2150) {
      setTheme('night');
    }
  }, [autoLdr, ldrAdc]);

  // Auto-dismiss transient toast notification after 3.5s (6s for call)
  useEffect(() => {
    if (!activeNotify) return;
    const ms = activeNotify.kind === 'call' ? 6000 : 3500;
    const t = setTimeout(() => setActiveNotify(null), ms);
    return () => clearTimeout(t);
  }, [activeNotify]);

  // JDM Tachometer Loading Sweep Loop (35ms smooth frame matching loading_screen.cpp)
  useEffect(() => {
    if (screenState !== 'loading') return;
    const timer = setInterval(() => {
      setTachPhase((p) => (p + 8) % 360);
    }, 35);
    return () => clearInterval(timer);
  }, [screenState]);

  // Ride simulation loop
  useEffect(() => {
    if (!isRunning || screenState !== 'nav') return;
    const timer = setInterval(() => {
      setSpeedKmh((prev) => {
        const delta = Math.round((Math.random() - 0.45) * 6);
        return Math.max(18, Math.min(88, prev + delta));
      });
      setSatellites((prev) =>
        hwGpsLocked ? Math.max(7, Math.min(12, prev + (Math.random() > 0.7 ? (Math.random() > 0.5 ? 1 : -1) : 0))) : 0
      );
      setDistanceM((prev) => {
        if (prev <= 15) {
          setStepIdx((sIdx) => {
            const next = (sIdx + 1) % SIM_STEPS.length;
            setDistanceM(SIM_STEPS[next].distanceM);
            return next;
          });
          return 0;
        }
        return prev - 15;
      });
    }, 850);
    return () => clearInterval(timer);
  }, [isRunning, screenState, hwGpsLocked]);

  const cycleBrightness = () => {
    setBrightnessIdx((prev) => ((prev + 1) % 3) as 0 | 1 | 2);
  };

  const toggleThemeManual = () => {
    setAutoLdr(false);
    setTheme((prev) => (prev === 'night' ? 'day' : 'night'));
  };

  const toggleViewMode = () => {
    setViewMode((prev) => {
      if (prev === 'vector') return 'hud';
      if (prev === 'hud') return 'media';
      return 'vector';
    });
    if (screenState === 'standby') {
      setScreenState('nav');
    }
  };

  const triggerMediaCommand = (cmd: 'prev' | 'play_pause' | 'next' | 'stop' | 'vol_down' | 'vol_up') => {
    setLastBleCmd(`TX BLE -> {"type":"media_cmd","cmd":"${cmd}"}`);
    if (cmd === 'prev') {
      const nextIdx = (trackIdx - 1 + SIM_TRACKS.length) % SIM_TRACKS.length;
      setTrackIdx(nextIdx);
      setPlaybackState('playing');
    } else if (cmd === 'next') {
      const nextIdx = (trackIdx + 1) % SIM_TRACKS.length;
      setTrackIdx(nextIdx);
      setPlaybackState('playing');
    } else if (cmd === 'play_pause') {
      setPlaybackState((s) => (s === 'playing' ? 'paused' : 'playing'));
    } else if (cmd === 'stop') {
      setPlaybackState('stopped');
    } else if (cmd === 'vol_up') {
      setMediaVolume((v) => Math.min(100, v + 10));
    } else if (cmd === 'vol_down') {
      setMediaVolume((v) => Math.max(0, v - 10));
    }
  };

  const handleSelectStep = (idx: number) => {
    setStepIdx(idx);
    setDistanceM(SIM_STEPS[idx].distanceM);
    setScreenState('nav');
  };

  // JDM Tachometer sweep math matching loading_screen.cpp
  const tachSweep = 0.5 - 0.5 * Math.cos((tachPhase * Math.PI) / 180);
  const tachCurAngle = 45 + tachSweep * 270; // 45..315 (0 = bottom)
  const tachNeedleRad = ((tachCurAngle + 90) * Math.PI) / 180;
  const tachNeedleX = 90 + Math.cos(tachNeedleRad) * 28;
  const tachNeedleY = 46 + Math.sin(tachNeedleRad) * 28;
  const arcRadius = 34.5;
  const arcCircumference = 2 * Math.PI * arcRadius;
  const maxArcLen = arcCircumference * 0.75;
  const activeArcLen = maxArcLen * tachSweep;

  const isLandscape = config.rotation === 1;

  const renderAntiAliasedArrowSVG = (maneuver: ManeuverType, sizeClass = 'w-20 h-20', showRing = true) => {
    const stem = pal.blue;
    const tip = offRoute ? pal.danger : pal.accent;

    return (
      <svg viewBox="0 0 100 100" className={sizeClass}>
        {showRing && <circle cx="50" cy="50" r="44" fill="none" stroke={pal.cardBorder} strokeWidth="1.5" />}

        {maneuver === 'right' && (
          <g strokeLinecap="round" strokeLinejoin="round" fill="none">
            <path d="M 32 76 L 32 42 L 68 42" stroke={stem} strokeWidth="10" />
            <path d="M 55 28 L 72 42 L 55 56" stroke={tip} strokeWidth="9" />
          </g>
        )}
        {maneuver === 'left' && (
          <g strokeLinecap="round" strokeLinejoin="round" fill="none">
            <path d="M 68 76 L 68 42 L 32 42" stroke={stem} strokeWidth="10" />
            <path d="M 45 28 L 28 42 L 45 56" stroke={tip} strokeWidth="9" />
          </g>
        )}
        {maneuver === 'straight' && (
          <g strokeLinecap="round" strokeLinejoin="round" fill="none">
            <line x1="50" y1="78" x2="50" y2="30" stroke={stem} strokeWidth="10" />
            <path d="M 32 45 L 50 24 L 68 45" stroke={tip} strokeWidth="9" />
          </g>
        )}
        {maneuver === 'uturn' && (
          <g strokeLinecap="round" strokeLinejoin="round" fill="none">
            <path d="M 67 76 L 67 42 A 17 17 0 0 0 33 42 L 33 66" stroke={stem} strokeWidth="9" />
            <path d="M 21 54 L 33 70 L 45 54" stroke={tip} strokeWidth="8" />
          </g>
        )}
        {maneuver === 'arrive' && (
          <g>
            <circle cx="50" cy="50" r="24" fill="none" stroke={tip} strokeWidth="6" />
            <circle cx="50" cy="50" r="11" fill={stem} />
            <circle cx="50" cy="50" r="4.5" fill="#ffffff" />
          </g>
        )}
      </svg>
    );
  };

  const renderBottomTelemetryPill = () => (
    <div className="px-2 pb-2">
      <div
        className="px-3 py-2 rounded-[14px] border flex items-center justify-between gap-2"
        style={{
          backgroundColor: pal.card,
          borderColor: pal.cardBorder,
          boxShadow: `2px 3px 0 ${pal.cardShadow}`,
        }}
      >
        <div className="shrink-0">
          <div className="text-[22px] font-extrabold leading-none tabular-nums" style={{ color: pal.accent }}>
            {speedKmh}
          </div>
          <div className="text-[9px] font-semibold mt-0.5" style={{ color: pal.muted }}>
            km/h
          </div>
        </div>

        <div className="h-7 w-px shrink-0" style={{ backgroundColor: pal.cardBorder }} />

        <div className="min-w-0 flex-1 text-center tabular-nums">
          <div className="text-[11px] font-bold truncate" style={{ color: offRoute ? pal.danger : pal.text }}>
            {offRoute ? 'HITUNG ULANG' : '9m · 4.2km'}
          </div>
          <div className="text-[9px] truncate" style={{ color: pal.muted }}>
            14:28 · HP:{phoneBattery}%
          </div>
        </div>

        <button
          onClick={toggleThemeManual}
          title="Zona Sentuh Kanan Bawah: Ganti Day / Night Mode"
          className="h-7 px-2 rounded-full border flex items-center gap-1.5 text-[9px] font-bold active:scale-95 shrink-0"
          style={{
            backgroundColor: pal.switchTrack,
            borderColor: pal.cardBorder,
            color: pal.text,
          }}
        >
          {theme === 'night' ? (
            <>
              <span>NGT</span>
              <span className="w-3.5 h-3.5 rounded-full" style={{ backgroundColor: pal.switchKnob }} />
            </>
          ) : (
            <>
              <span className="w-3.5 h-3.5 rounded-full" style={{ backgroundColor: pal.switchKnob }} />
              <span>DAY</span>
            </>
          )}
        </button>
      </div>
    </div>
  );

  return (
    <div className="max-w-7xl mx-auto px-6 py-6 space-y-6">
      {/* Header Bar */}
      <div className="flex flex-col lg:flex-row lg:items-center justify-between gap-4 pb-5 border-b border-slate-800">
        <div className="space-y-1 max-w-2xl">
          <h2 className="text-2xl font-bold tracking-tight text-white">
            Pratinjau Layar CYD ESP32 (Automotive HUD)
          </h2>
          <p className="text-sm text-slate-400">
            Visualisasi langsung tampilan TFT 2.8&quot; ({isLandscape ? '320×240 Landscape' : '240×320 Portrait'}) dengan
            rendering peta anti-aliased (<code className="text-cyan-400 font-mono">drawWideLine</code> +{' '}
            <code className="text-cyan-400 font-mono">pushRotated</code> chevron), floating card HUD, dan auto-theme LDR GPIO 34.
          </p>
        </div>

        <div className="flex items-center gap-3 shrink-0">
          <button
            onClick={() => setIsRunning((r) => !r)}
            className={`flex items-center gap-2 px-4 py-2 rounded-lg text-xs font-semibold transition-colors whitespace-nowrap shrink-0 ${
              isRunning
                ? 'bg-amber-600 text-white hover:bg-amber-500'
                : 'bg-emerald-600 text-white hover:bg-emerald-500'
            }`}
          >
            {isRunning ? <Pause className="w-3.5 h-3.5" /> : <Play className="w-3.5 h-3.5" />}
            <span>{isRunning ? 'Jeda Simulasi' : 'Simulasi Berkendara'}</span>
          </button>
          <button
            onClick={onGoToCode}
            className="px-4 py-2 rounded-lg text-xs font-semibold text-slate-200 bg-slate-900 border border-slate-800 hover:bg-slate-800 transition-colors whitespace-nowrap shrink-0"
          >
            Kembali ke Kode
          </button>
        </div>
      </div>

      {/* Main Split: Interactive CYD TFT Hardware Frame + Simulation Controls */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8 items-start">
        {/* Left: Physical CYD ESP32-2432S028R Hardware Preview */}
        <div className="lg:col-span-6 flex flex-col items-center justify-center rounded-xl border border-slate-800 bg-slate-900/50 p-6 sm:p-8">
          <div
            className={`relative rounded-2xl p-5 border-2 shadow-2xl transition-all duration-200 bg-slate-900 border-slate-700 ${
              isLandscape ? 'w-full max-w-[460px]' : 'w-full max-w-[330px]'
            }`}
          >
            {/* PCB Silkscreen Header */}
            <div className="flex items-center justify-between text-[10px] font-mono font-semibold tracking-wider text-slate-300 mb-3 px-1">
              <span>
                ESP32-2432S028R · {config.tftDriver === 'ST7789_DRIVER' ? 'ST7789' : 'ILI9341'}
              </span>
              <span>{config.bleDeviceName}</span>
            </div>

            {/* Active TFT Screen Viewport */}
            <div
              className={`relative w-full overflow-hidden border-4 border-slate-950 rounded-lg font-sans select-none transition-all duration-200 ${
                isLandscape ? 'aspect-[4/3]' : 'aspect-[3/4]'
              }`}
              style={{ backgroundColor: pal.land, opacity: brightnessOpacity }}
            >
              {/* Cartographic Backdrop (Park blocks + subtle coordinate grid) */}
              <svg className="absolute inset-0 w-full h-full pointer-events-none" viewBox="0 0 240 320" preserveAspectRatio="none">
                <rect x="12" y="96" width="56" height="48" rx="8" fill={pal.park} />
                <rect x="172" y="134" width="58" height="62" rx="8" fill={pal.park} />
                <rect x="16" y="194" width="48" height="44" rx="8" fill={pal.park} />
                {[40, 80, 120, 160, 200].map((gx) => (
                  <line key={`vx-${gx}`} x1={gx} y1="0" x2={gx} y2="320" stroke={pal.grid} strokeWidth="0.7" />
                ))}
                {[40, 80, 120, 160, 200, 240, 280].map((gy) => (
                  <line key={`hy-${gy}`} x1="0" y1={gy} x2="240" y2={gy} stroke={pal.grid} strokeWidth="0.7" />
                ))}
              </svg>

              {/* Top Utility Pills (Brightness Left, BLE/GPS Center, Mode Switch Right) */}
              {screenState !== 'loading' && (
                <div className="relative z-20 flex items-center justify-between gap-1.5 px-2 pt-1.5">
                  <button
                    onClick={cycleBrightness}
                    title="Zona Sentuh Kiri Atas: Kecerahan Layar"
                    className="h-5 px-2 rounded-full border text-[9px] font-bold active:scale-95 shrink-0"
                    style={{
                      backgroundColor: pal.card,
                      borderColor: pal.cardBorder,
                      color: pal.text,
                    }}
                  >
                    {brightnessLabel}
                  </button>

                  <div
                    className="h-5 px-2.5 rounded-full border flex items-center gap-1.5 text-[9px] font-semibold truncate"
                    style={{
                      backgroundColor: pal.card,
                      borderColor: pal.cardBorder,
                      color: pal.muted,
                    }}
                  >
                    <span
                      className="w-1.5 h-1.5 rounded-full shrink-0"
                      style={{ backgroundColor: bleConnected ? pal.success : pal.danger }}
                    />
                    <span className="truncate">
                      {bleConnected
                        ? config.enableHwGps && hwGpsLocked
                          ? `BLE · SAT:${satellites}`
                          : autoLdr
                          ? 'BLE · AUTO'
                          : 'BLE · LINK'
                        : 'NO BLE'}
                    </span>
                  </div>

                  <button
                    onClick={toggleViewMode}
                    title="Zona Sentuh Kanan Atas: Ganti MAP / HUD / MUS"
                    className="h-5 px-2.5 rounded-full border text-[9px] font-bold active:scale-95 shrink-0"
                    style={{
                      backgroundColor: pal.card,
                      borderColor: pal.blue,
                      color: pal.blue,
                    }}
                  >
                    {viewMode === 'vector' ? 'HUD' : viewMode === 'hud' ? 'MUS' : 'MAP'}
                  </button>
                </div>
              )}

              {/* Transient 3.5s Toast Pop-Up Notification (WhatsApp / Call / Media) */}
              {activeNotify && (
                <button
                  onClick={() => setActiveNotify(null)}
                  title="Ketuk untuk menutup toast"
                  className="absolute top-7 left-2 right-2 z-30 text-left p-2 rounded-xl border flex items-stretch gap-2 transition-transform active:scale-98"
                  style={{
                    backgroundColor: pal.card,
                    borderColor:
                      activeNotify.kind === 'call'
                        ? pal.danger
                        : activeNotify.kind === 'whatsapp'
                        ? pal.success
                        : pal.accent,
                    boxShadow: `2px 3px 0 ${pal.cardShadow}`,
                  }}
                >
                  <div
                    className="w-1 rounded-full shrink-0"
                    style={{
                      backgroundColor:
                        activeNotify.kind === 'call'
                          ? pal.danger
                          : activeNotify.kind === 'whatsapp'
                          ? pal.success
                          : pal.accent,
                    }}
                  />
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center justify-between text-[8px] font-bold">
                      <span
                        style={{
                          color:
                            activeNotify.kind === 'call'
                              ? pal.danger
                              : activeNotify.kind === 'whatsapp'
                              ? pal.success
                              : pal.accent,
                        }}
                      >
                        {activeNotify.kind === 'call'
                          ? 'PANGGILAN MASUK'
                          : activeNotify.kind === 'whatsapp'
                          ? 'WHATSAPP'
                          : 'NOW PLAYING'}
                      </span>
                      <span style={{ color: pal.muted }}>TAP TUTUP</span>
                    </div>
                    <div className="text-[11px] font-bold truncate mt-0.5" style={{ color: pal.text }}>
                      {activeNotify.sender}
                    </div>
                    <div className="text-[9px] truncate" style={{ color: pal.muted }}>
                      {activeNotify.message}
                    </div>
                  </div>
                </button>
              )}

              {/* STATE 1: STANDBY / WAITING BLE SCREEN */}
              {screenState === 'standby' && (
                <div className="relative z-10 h-[calc(100%-26px)] flex flex-col justify-between px-3 py-2">
                  <div className="flex flex-col items-center justify-center flex-1 gap-3">
                    {/* Circular Automotive Status Emblem */}
                    <svg viewBox="0 0 48 48" className="w-10 h-10">
                      <path
                        d="M 10 38 A 18 18 0 1 1 38 38"
                        fill="none"
                        stroke={pal.blue}
                        strokeWidth="3.5"
                        strokeLinecap="round"
                      />
                      <circle cx="24" cy="24" r="6" fill={pal.accent} />
                      <circle cx="24" cy="24" r="2.5" fill="#ffffff" />
                    </svg>

                    <div
                      className="w-full p-4 rounded-[14px] border text-center space-y-1.5"
                      style={{
                        backgroundColor: pal.card,
                        borderColor: pal.cardBorder,
                        boxShadow: `2px 3px 0 ${pal.cardShadow}`,
                      }}
                    >
                      <div className="text-xs font-bold tracking-wide" style={{ color: pal.accent }}>
                        SIAP NAVIGASI
                      </div>
                      <div className="text-[11px] font-semibold" style={{ color: pal.text }}>
                        BLE: {config.bleDeviceName}
                      </div>
                      <div className="text-[10px]" style={{ color: bleConnected ? pal.success : pal.muted }}>
                        {bleConnected ? 'BLE TERHUBUNG · SIAP RUTE' : 'HUBUNGKAN APP DISPLAYCONNECT'}
                      </div>
                      {config.enableHwGps && (
                        <div className="text-[10px] tabular-nums pt-0.5" style={{ color: hwGpsLocked ? pal.success : pal.muted }}>
                          {hwGpsLocked ? `GPS LOCK: ${satellites} SAT · ${speedKmh} km/h` : 'GPS: MENCARI SATELIT...'}
                        </div>
                      )}
                    </div>
                  </div>

                  <div className="flex items-center justify-between pb-1">
                    <span className="text-[8px]" style={{ color: pal.muted }}>
                      TAP KANAN ATAS: MAP / HUD / MUSIK
                    </span>
                    <button
                      onClick={toggleThemeManual}
                      className="h-6 px-2 rounded-full border flex items-center gap-1 text-[9px] font-bold active:scale-95"
                      style={{
                        backgroundColor: pal.switchTrack,
                        borderColor: pal.cardBorder,
                        color: pal.text,
                      }}
                    >
                      <span>{theme === 'night' ? 'NGT' : 'DAY'}</span>
                    </button>
                  </div>
                </div>
              )}

              {/* STATE 2: FLICKER-FREE JDM TACHOMETER LOADING SCREEN */}
              {screenState === 'loading' && (
                <div className="relative z-10 h-full flex flex-col justify-between px-4 py-4">
                  <div className="flex justify-center">
                    <div
                      className="px-4 py-1 rounded-full border text-[9px] font-bold tracking-wider"
                      style={{ backgroundColor: pal.card, borderColor: pal.cardBorder, color: pal.blue }}
                    >
                      MOTO HUD // SYNC
                    </div>
                  </div>

                  {/* 180x96 Sprite Equivalent JDM Gauge */}
                  <div
                    className="mx-auto w-[78%] rounded-[14px] border p-2.5 flex flex-col items-center"
                    style={{
                      backgroundColor: pal.card,
                      borderColor: pal.cardBorder,
                      boxShadow: `2px 3px 0 ${pal.cardShadow}`,
                    }}
                  >
                    <svg viewBox="0 0 180 86" className="w-full h-24">
                      {/* Base Track Arc (270 deg) */}
                      <circle
                        cx="90"
                        cy="46"
                        r={arcRadius}
                        fill="none"
                        stroke={pal.cardAlt}
                        strokeWidth="7"
                        strokeDasharray={`${maxArcLen} ${arcCircumference}`}
                        strokeLinecap="round"
                        transform="rotate(135 90 46)"
                      />
                      {/* Redline Zone */}
                      <circle
                        cx="90"
                        cy="46"
                        r={arcRadius}
                        fill="none"
                        stroke={pal.danger}
                        strokeWidth="5"
                        strokeDasharray={`${maxArcLen * 0.18} ${arcCircumference}`}
                        strokeLinecap="round"
                        transform={`rotate(${135 + 270 * 0.82} 90 46)`}
                      />
                      {/* Active Sweeping Arc */}
                      <circle
                        cx="90"
                        cy="46"
                        r={arcRadius}
                        fill="none"
                        stroke={tachCurAngle > 265 ? pal.accent : pal.blue}
                        strokeWidth="7"
                        strokeDasharray={`${activeArcLen} ${arcCircumference}`}
                        strokeLinecap="round"
                        transform="rotate(135 90 46)"
                      />
                      {/* Rotating Needle */}
                      <line
                        x1="90"
                        y1="46"
                        x2={tachNeedleX}
                        y2={tachNeedleY}
                        stroke={pal.accent}
                        strokeWidth="3"
                        strokeLinecap="round"
                      />
                      <circle cx="90" cy="46" r="5" fill={pal.text} />
                      <circle cx="90" cy="46" r="2.5" fill={pal.accent} />
                      <text
                        x="90"
                        y="70"
                        textAnchor="middle"
                        fill={pal.text}
                        fontSize="12"
                        fontWeight="700"
                      >
                        {Math.round(tachSweep * 100)}%
                      </text>
                    </svg>
                    <div className="w-4/5 h-1.5 rounded-full overflow-hidden mt-1" style={{ backgroundColor: pal.cardAlt }}>
                      <div
                        className="h-full rounded-full"
                        style={{
                          width: `${Math.max(8, Math.round(tachSweep * 100))}%`,
                          backgroundColor: pal.blue,
                        }}
                      />
                    </div>
                  </div>

                  {/* Bottom Status Card */}
                  <div
                    className="p-3.5 rounded-[14px] border text-center space-y-1"
                    style={{
                      backgroundColor: pal.card,
                      borderColor: pal.cardBorder,
                      boxShadow: `2px 3px 0 ${pal.cardShadow}`,
                    }}
                  >
                    <div className="text-xs font-bold" style={{ color: pal.text }}>
                      MENGUNCI RUTE GPS...
                    </div>
                    <div className="text-[9px]" style={{ color: pal.muted }}>
                      SINKRONISASI PETA &amp; TELEMETRI
                    </div>
                    <div className="flex items-center justify-center gap-2.5 pt-1">
                      <span className="w-1.5 h-1.5 rounded-full" style={{ backgroundColor: pal.blue }} />
                      <span className="w-1.5 h-1.5 rounded-full" style={{ backgroundColor: pal.accent }} />
                      <span className="w-1.5 h-1.5 rounded-full" style={{ backgroundColor: pal.success }} />
                    </div>
                  </div>
                </div>
              )}

              {/* STATE 3A: MODERN AUTOMOTIVE VECTOR MAP MODE */}
              {screenState === 'nav' && viewMode === 'vector' && (
                <div className="relative z-10 h-[calc(100%-26px)] flex flex-col justify-between">
                  {/* Floating Turn-by-Turn Card (Top Overlay) */}
                  <div className="px-2 pt-1">
                    <div
                      className="p-1.5 rounded-[12px] border flex items-center gap-2"
                      style={{
                        backgroundColor: pal.card,
                        borderColor: offRoute ? pal.danger : pal.cardBorder,
                        boxShadow: `2px 3px 0 ${pal.cardShadow}`,
                      }}
                    >
                      <div
                        className="w-11 h-11 rounded-[10px] flex items-center justify-center shrink-0"
                        style={{ backgroundColor: pal.cardAlt }}
                      >
                        {renderAntiAliasedArrowSVG(currentStep.maneuver, 'w-8 h-8', false)}
                      </div>
                      <div className="min-w-0 flex-1">
                        <div
                          className="text-base font-extrabold leading-tight tabular-nums"
                          style={{ color: offRoute ? pal.danger : pal.text }}
                        >
                          {offRoute
                            ? 'OFF ROUTE'
                            : distanceM >= 1000
                            ? `${(distanceM / 1000).toFixed(1)} km`
                            : `${distanceM} m`}
                        </div>
                        <div className="text-[10px] font-medium truncate" style={{ color: pal.muted }}>
                          {currentStep.street}
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Anti-Aliased Vector Map Canvas */}
                  <div className="relative flex-1 -mt-2 -mb-2 pointer-events-none">
                    <svg viewBox="0 85 240 175" className="w-full h-full">
                      {/* Nearby Streets (outer casing then inner fill) */}
                      <g stroke={pal.roadEdge} strokeWidth="6.5" strokeLinecap="round">
                        <line x1="25" y1="186" x2="215" y2="186" />
                        <line x1="120" y1="90" x2="120" y2="255" />
                        <line x1="35" y1="138" x2="225" y2="138" />
                        <line x1="120" y1="138" x2="215" y2="95" />
                      </g>
                      <g stroke={pal.roadFill} strokeWidth="3.4" strokeLinecap="round">
                        <line x1="25" y1="186" x2="215" y2="186" />
                        <line x1="120" y1="90" x2="120" y2="255" />
                        <line x1="35" y1="138" x2="225" y2="138" />
                        <line x1="120" y1="138" x2="215" y2="95" />
                      </g>

                      {/* Smooth Rounded Active Route Polyline */}
                      <polyline
                        points="120,252 120,186 120,138 175,114 214,96"
                        fill="none"
                        stroke={offRoute ? pal.cardShadow : pal.routeEdge}
                        strokeWidth="8.5"
                        strokeLinecap="round"
                        strokeLinejoin="round"
                      />
                      <polyline
                        points="120,252 120,186 120,138 175,114 214,96"
                        fill="none"
                        stroke={offRoute ? pal.danger : pal.route}
                        strokeWidth="4.8"
                        strokeLinecap="round"
                        strokeLinejoin="round"
                      />

                      {/* Destination Beacon */}
                      <circle cx="214" cy="96" r="5.5" fill="#ffffff" />
                      <circle cx="214" cy="96" r="3" fill={pal.accent} />

                      {/* 24x24 Rotating 3D Chevron Sprite Equivalent + Halo Ring */}
                      <g transform={`translate(${currentStep.userX}, ${currentStep.userY})`}>
                        <circle r="15" fill="none" stroke={pal.blue} strokeWidth="1.8" opacity="0.85" />
                        <g transform={`rotate(${currentStep.bearing})`}>
                          {/* Drop shadow */}
                          <polygon points="0,-9 -8,9 0,5 8,9" fill="#080a0f" opacity="0.55" transform="translate(0, 2)" />
                          {/* Outer white rim */}
                          <polygon points="0,-11 -9.5,8.5 0,4 9.5,8.5" fill="#ffffff" />
                          {/* 3D Left & Right Bevel Wings */}
                          <polygon points="0,-9 -7.5,7 0,2.5" fill={pal.accent} />
                          <polygon points="0,-9 7.5,7 0,2.5" fill={pal.accent} opacity="0.78" />
                        </g>
                      </g>
                    </svg>
                  </div>

                  {renderBottomTelemetryPill()}
                </div>
              )}

              {/* STATE 3B: NAVIGASI MOTO BIG-ARROW HUD MODE */}
              {screenState === 'nav' && viewMode === 'hud' && (
                <div className="relative z-10 h-[calc(100%-26px)] flex flex-col justify-between pt-1.5">
                  <div className="px-2.5">
                    <div
                      className="p-3 rounded-[16px] border flex flex-col items-center justify-center"
                      style={{
                        backgroundColor: pal.card,
                        borderColor: offRoute ? pal.danger : pal.cardBorder,
                        boxShadow: `2px 3px 0 ${pal.cardShadow}`,
                      }}
                    >
                      {renderAntiAliasedArrowSVG(currentStep.maneuver, 'w-20 h-20', true)}
                      <div
                        className="text-2xl font-extrabold tracking-tight tabular-nums mt-1"
                        style={{ color: offRoute ? pal.danger : pal.text }}
                      >
                        {offRoute
                          ? 'OFF ROUTE!'
                          : distanceM >= 1000
                          ? `${(distanceM / 1000).toFixed(1)} KM`
                          : `${distanceM} M`}
                      </div>
                    </div>
                  </div>

                  <div className="px-2.5">
                    <div
                      className="p-2.5 rounded-[12px] border space-y-0.5"
                      style={{
                        backgroundColor: pal.card,
                        borderColor: pal.cardBorder,
                        boxShadow: `2px 3px 0 ${pal.cardShadow}`,
                      }}
                    >
                      <div className="text-xs font-bold truncate" style={{ color: pal.text }}>
                        {currentStep.instruction}
                      </div>
                      <div className="text-[10px] truncate" style={{ color: pal.muted }}>
                        {currentStep.street}
                      </div>
                    </div>
                  </div>

                  {renderBottomTelemetryPill()}
                </div>
              )}

              {/* STATE 3C: MUSIC PLAYER AUDIO DECK MODE */}
              {screenState === 'nav' && viewMode === 'media' && (
                <div className="relative z-10 h-[calc(100%-26px)] flex flex-col justify-between pt-1.5">
                  <div className="px-2">
                    <div
                      className="p-2.5 rounded-[12px] border flex items-stretch gap-2"
                      style={{
                        backgroundColor: pal.card,
                        borderColor: pal.cardBorder,
                        boxShadow: `2px 3px 0 ${pal.cardShadow}`,
                      }}
                    >
                      <div className="w-1 rounded-full shrink-0" style={{ backgroundColor: pal.blue }} />
                      <div className="min-w-0 flex-1 space-y-0.5">
                        <div className="flex items-center justify-between text-[9px] font-bold">
                          <span style={{ color: pal.muted }}>
                            {currentTrack.app} · VOL {mediaVolume}%
                          </span>
                          <span
                            style={{
                              color: !bleConnected
                                ? pal.danger
                                : playbackState === 'playing'
                                ? pal.success
                                : playbackState === 'paused'
                                ? pal.accent
                                : pal.muted,
                            }}
                          >
                            {!bleConnected ? 'NO BLE' : playbackState.toUpperCase()}
                          </span>
                        </div>
                        <div className="text-xs font-bold truncate" style={{ color: pal.text }}>
                          {currentTrack.title}
                        </div>
                        <div className="text-[10px] truncate" style={{ color: pal.muted }}>
                          {currentTrack.artist}
                        </div>
                      </div>
                    </div>
                  </div>

                  <div className="px-2 space-y-2">
                    <div className="grid grid-cols-3 gap-1.5">
                      <button
                        onClick={() => triggerMediaCommand('prev')}
                        className="py-2.5 rounded-[12px] border flex flex-col items-center justify-center gap-0.5 active:scale-95"
                        style={{
                          backgroundColor: pal.card,
                          borderColor: pal.cardBorder,
                          color: pal.text,
                          boxShadow: `1px 2px 0 ${pal.cardShadow}`,
                        }}
                      >
                        <SkipBack className="w-4 h-4" />
                        <span className="text-[9px] font-bold" style={{ color: pal.muted }}>
                          PREV
                        </span>
                      </button>

                      <button
                        onClick={() => triggerMediaCommand('play_pause')}
                        className="py-2.5 rounded-[12px] border flex flex-col items-center justify-center gap-0.5 active:scale-95"
                        style={{
                          backgroundColor: pal.card,
                          borderColor: playbackState === 'playing' ? pal.accent : pal.blue,
                          color: playbackState === 'playing' ? pal.accent : pal.blue,
                          boxShadow: `1px 2px 0 ${pal.cardShadow}`,
                        }}
                      >
                        {playbackState === 'playing' ? <Pause className="w-4 h-4" /> : <Play className="w-4 h-4" />}
                        <span className="text-[9px] font-bold">
                          {playbackState === 'playing' ? 'PAUSE' : 'PLAY'}
                        </span>
                      </button>

                      <button
                        onClick={() => triggerMediaCommand('next')}
                        className="py-2.5 rounded-[12px] border flex flex-col items-center justify-center gap-0.5 active:scale-95"
                        style={{
                          backgroundColor: pal.card,
                          borderColor: pal.cardBorder,
                          color: pal.text,
                          boxShadow: `1px 2px 0 ${pal.cardShadow}`,
                        }}
                      >
                        <SkipForward className="w-4 h-4" />
                        <span className="text-[9px] font-bold" style={{ color: pal.muted }}>
                          NEXT
                        </span>
                      </button>
                    </div>

                    <div className="grid grid-cols-3 gap-1.5">
                      <button
                        onClick={() => triggerMediaCommand('vol_down')}
                        className="py-2 rounded-[12px] border flex items-center justify-center text-[11px] font-bold active:scale-95"
                        style={{
                          backgroundColor: pal.card,
                          borderColor: pal.cardBorder,
                          color: pal.text,
                          boxShadow: `1px 2px 0 ${pal.cardShadow}`,
                        }}
                      >
                        VOL -
                      </button>

                      <button
                        onClick={() => triggerMediaCommand('stop')}
                        className="py-2 rounded-[12px] border flex items-center justify-center gap-1 text-[10px] font-bold active:scale-95"
                        style={{
                          backgroundColor: pal.card,
                          borderColor: pal.danger,
                          color: pal.danger,
                          boxShadow: `1px 2px 0 ${pal.cardShadow}`,
                        }}
                      >
                        <Square className="w-3 h-3 fill-current" />
                        <span>STOP</span>
                      </button>

                      <button
                        onClick={() => triggerMediaCommand('vol_up')}
                        className="py-2 rounded-[12px] border flex items-center justify-center text-[11px] font-bold active:scale-95"
                        style={{
                          backgroundColor: pal.card,
                          borderColor: pal.cardBorder,
                          color: pal.text,
                          boxShadow: `1px 2px 0 ${pal.cardShadow}`,
                        }}
                      >
                        VOL +
                      </button>
                    </div>
                  </div>

                  {renderBottomTelemetryPill()}
                </div>
              )}
            </div>

            {/* Bottom Hardware PCB Status Strip */}
            <div className="flex items-center justify-between mt-3 px-1 text-[10px] font-mono text-slate-400">
              <span>XPT2046 TOUCH AKTIF</span>
              <span>BL: GPIO 21 ({brightnessLabel})</span>
            </div>
          </div>
        </div>

        {/* Right: Interactive Simulation Controls */}
        <div className="lg:col-span-6 space-y-5">
          {/* Screen State & View Mode */}
          <div className="rounded-xl border border-slate-800 bg-slate-900/60 p-5 space-y-4">
            <h3 className="text-sm font-bold text-white">Status Layar &amp; Mode Tampilan</h3>

            <div className="grid grid-cols-3 gap-2">
              <button
                onClick={() => setScreenState('standby')}
                className={`px-3 py-2 rounded-lg text-xs font-semibold border transition-colors ${
                  screenState === 'standby'
                    ? 'bg-cyan-600/20 border-cyan-500 text-cyan-300'
                    : 'bg-slate-950 border-slate-800 text-slate-400 hover:text-white'
                }`}
              >
                Standby BLE
              </button>
              <button
                onClick={() => setScreenState('loading')}
                className={`px-3 py-2 rounded-lg text-xs font-semibold border transition-colors ${
                  screenState === 'loading'
                    ? 'bg-cyan-600/20 border-cyan-500 text-cyan-300'
                    : 'bg-slate-950 border-slate-800 text-slate-400 hover:text-white'
                }`}
              >
                Boot Tachometer
              </button>
              <button
                onClick={() => setScreenState('nav')}
                className={`px-3 py-2 rounded-lg text-xs font-semibold border transition-colors ${
                  screenState === 'nav'
                    ? 'bg-cyan-600/20 border-cyan-500 text-cyan-300'
                    : 'bg-slate-950 border-slate-800 text-slate-400 hover:text-white'
                }`}
              >
                Navigasi Aktif
              </button>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 gap-3 pt-2 border-t border-slate-800">
              <div className="space-y-1.5">
                <span className="text-[11px] font-semibold text-slate-400">Mode Layar CYD</span>
                <div className="flex p-1 bg-slate-950 rounded-lg border border-slate-800">
                  <button
                    onClick={() => {
                      setViewMode('vector');
                      setScreenState('nav');
                    }}
                    className={`flex-1 py-1.5 text-xs font-medium rounded-md transition-colors ${
                      viewMode === 'vector' ? 'bg-cyan-600 text-white' : 'text-slate-400 hover:text-white'
                    }`}
                  >
                    Peta
                  </button>
                  <button
                    onClick={() => {
                      setViewMode('hud');
                      setScreenState('nav');
                    }}
                    className={`flex-1 py-1.5 text-xs font-medium rounded-md transition-colors ${
                      viewMode === 'hud' ? 'bg-cyan-600 text-white' : 'text-slate-400 hover:text-white'
                    }`}
                  >
                    HUD
                  </button>
                  <button
                    onClick={() => {
                      setViewMode('media');
                      setScreenState('nav');
                    }}
                    className={`flex-1 flex items-center justify-center gap-1 py-1.5 text-xs font-medium rounded-md transition-colors ${
                      viewMode === 'media' ? 'bg-cyan-600 text-white' : 'text-slate-400 hover:text-white'
                    }`}
                  >
                    <Music className="w-3 h-3" />
                    <span>Musik</span>
                  </button>
                </div>
              </div>

              <div className="space-y-1.5">
                <span className="text-[11px] font-semibold text-slate-400">Tema Otomotif (Day / Night)</span>
                <div className="flex p-1 bg-slate-950 rounded-lg border border-slate-800">
                  <button
                    onClick={() => {
                      setAutoLdr(false);
                      setTheme('night');
                    }}
                    className={`flex-1 flex items-center justify-center gap-1 py-1.5 text-xs font-medium rounded-md transition-colors ${
                      theme === 'night' ? 'bg-blue-600 text-white' : 'text-slate-400 hover:text-white'
                    }`}
                  >
                    <Moon className="w-3 h-3" />
                    <span>Night</span>
                  </button>
                  <button
                    onClick={() => {
                      setAutoLdr(false);
                      setTheme('day');
                    }}
                    className={`flex-1 flex items-center justify-center gap-1 py-1.5 text-xs font-medium rounded-md transition-colors ${
                      theme === 'day' ? 'bg-amber-600 text-white' : 'text-slate-400 hover:text-white'
                    }`}
                  >
                    <Sun className="w-3 h-3" />
                    <span>Day</span>
                  </button>
                </div>
              </div>
            </div>

            {/* LDR Sensor GPIO 34 Hysteresis Simulator */}
            <div className="pt-2 border-t border-slate-800 space-y-2">
              <div className="flex items-center justify-between">
                <label className="flex items-center gap-2 text-xs text-slate-300 font-medium cursor-pointer">
                  <input
                    type="checkbox"
                    checked={autoLdr}
                    onChange={(e) => setAutoLdr(e.target.checked)}
                    className="rounded accent-cyan-500"
                  />
                  <SunDim className="w-3.5 h-3.5 text-amber-400" />
                  <span>Auto-Theme Sensor LDR (GPIO 34)</span>
                </label>
                <span className="font-mono text-[11px] text-cyan-400 tabular-nums">
                  ADC: {ldrAdc} {ldrAdc <= 950 ? '(Siang)' : ldrAdc >= 2150 ? '(Malam)' : '(Hysteresis)'}
                </span>
              </div>
              <input
                type="range"
                min={200}
                max={3800}
                step={50}
                value={ldrAdc}
                disabled={!autoLdr}
                onChange={(e) => setLdrAdc(Number(e.target.value))}
                className="w-full accent-amber-500 disabled:opacity-40"
              />
            </div>

            {viewMode === 'media' && (
              <div className="pt-2 border-t border-slate-800 flex items-center justify-between gap-2 text-[11px] font-mono">
                <span className="text-slate-400 flex items-center gap-1.5 shrink-0">
                  <Volume2 className="w-3.5 h-3.5 text-cyan-400" />
                  <span>Perintah BLE CYD → HP:</span>
                </span>
                <span className="text-emerald-400 truncate">{lastBleCmd}</span>
              </div>
            )}
          </div>

          {/* Connection, Speed & Off-Route Controls */}
          <div className="rounded-xl border border-slate-800 bg-slate-900/60 p-5 space-y-4">
            <h3 className="text-sm font-bold text-white">Telemetri Kecepatan &amp; Koneksi</h3>

            <div className="grid grid-cols-1 sm:grid-cols-3 gap-2.5">
              <button
                onClick={() => setBleConnected((b) => !b)}
                className={`flex items-center justify-center gap-1.5 px-3 py-2 rounded-lg text-xs font-semibold border transition-colors ${
                  bleConnected
                    ? 'bg-emerald-500/10 border-emerald-500/40 text-emerald-300'
                    : 'bg-slate-950 border-slate-800 text-slate-400'
                }`}
              >
                <Bluetooth className="w-3.5 h-3.5" />
                <span>{bleConnected ? 'BLE Terhubung' : 'BLE Putus'}</span>
              </button>

              <button
                onClick={() => setHwGpsLocked((g) => !g)}
                disabled={!config.enableHwGps}
                className={`flex items-center justify-center gap-1.5 px-3 py-2 rounded-lg text-xs font-semibold border transition-colors disabled:opacity-40 ${
                  hwGpsLocked && config.enableHwGps
                    ? 'bg-emerald-500/10 border-emerald-500/40 text-emerald-300'
                    : 'bg-slate-950 border-slate-800 text-slate-400'
                }`}
              >
                <Satellite className="w-3.5 h-3.5" />
                <span>{config.enableHwGps ? (hwGpsLocked ? `GPS ${satellites} Sat` : 'GPS No Fix') : 'GPS Nonaktif'}</span>
              </button>

              <button
                onClick={() => setOffRoute((o) => !o)}
                className={`flex items-center justify-center gap-1.5 px-3 py-2 rounded-lg text-xs font-semibold border transition-colors ${
                  offRoute
                    ? 'bg-rose-500/20 border-rose-500 text-rose-300'
                    : 'bg-slate-950 border-slate-800 text-slate-400 hover:text-white'
                }`}
              >
                <AlertTriangle className="w-3.5 h-3.5" />
                <span>{offRoute ? 'Off-Route Aktif' : 'Uji Off-Route'}</span>
              </button>
            </div>

            <div className="grid grid-cols-2 gap-4 pt-2 border-t border-slate-800">
              <div className="space-y-1.5">
                <div className="flex justify-between text-xs">
                  <span className="text-slate-400">Kecepatan</span>
                  <span className="font-mono font-bold text-cyan-400 tabular-nums">{speedKmh} km/h</span>
                </div>
                <input
                  type="range"
                  min={0}
                  max={120}
                  value={speedKmh}
                  onChange={(e) => setSpeedKmh(Number(e.target.value))}
                  className="w-full accent-cyan-500"
                />
              </div>

              <div className="space-y-1.5">
                <div className="flex justify-between text-xs">
                  <span className="text-slate-400">Jarak Belokan</span>
                  <span className="font-mono font-bold text-cyan-400 tabular-nums">{distanceM} m</span>
                </div>
                <input
                  type="range"
                  min={0}
                  max={1500}
                  step={10}
                  value={distanceM}
                  onChange={(e) => setDistanceM(Number(e.target.value))}
                  className="w-full accent-cyan-500"
                />
              </div>
            </div>

            <div className="pt-3 border-t border-slate-800 space-y-2">
              <div className="text-[11px] font-semibold text-slate-400">Uji Pop-Up Toast Sementara (3.5 detik)</div>
              <div className="grid grid-cols-3 gap-2">
                <button
                  onClick={() =>
                    setActiveNotify({
                      kind: 'whatsapp',
                      sender: 'Budi (Touring)',
                      message: 'Bro, tunggu di SPBU depan!',
                    })
                  }
                  className="flex items-center justify-center gap-1.5 px-2.5 py-2 rounded-lg text-xs font-semibold bg-emerald-500/10 border border-emerald-500/30 text-emerald-300 hover:bg-emerald-500/20 transition-colors whitespace-nowrap"
                >
                  <MessageSquare className="w-3.5 h-3.5" />
                  <span>WhatsApp</span>
                </button>
                <button
                  onClick={() =>
                    setActiveNotify({
                      kind: 'call',
                      sender: 'Ibu (+62 812...)',
                      message: 'Panggilan Masuk...',
                    })
                  }
                  className="flex items-center justify-center gap-1.5 px-2.5 py-2 rounded-lg text-xs font-semibold bg-rose-500/10 border border-rose-500/30 text-rose-300 hover:bg-rose-500/20 transition-colors whitespace-nowrap"
                >
                  <PhoneIncoming className="w-3.5 h-3.5" />
                  <span>Telepon</span>
                </button>
                <button
                  onClick={() =>
                    setActiveNotify({
                      kind: 'media',
                      sender: currentTrack.title,
                      message: currentTrack.artist,
                    })
                  }
                  className="flex items-center justify-center gap-1.5 px-2.5 py-2 rounded-lg text-xs font-semibold bg-amber-500/10 border border-amber-500/30 text-amber-300 hover:bg-amber-500/20 transition-colors whitespace-nowrap"
                >
                  <Music className="w-3.5 h-3.5" />
                  <span>Lagu Baru</span>
                </button>
              </div>
            </div>
          </div>

          {/* Step-by-Step Maneuver Selector */}
          <div className="rounded-xl border border-slate-800 bg-slate-900/60 p-5 space-y-3">
            <div className="flex items-center justify-between">
              <h3 className="text-sm font-bold text-white">Simulasi Instruksi Belokan</h3>
              <button
                onClick={() => handleSelectStep(0)}
                className="flex items-center gap-1 text-xs text-slate-400 hover:text-white"
              >
                <RotateCcw className="w-3 h-3" />
                <span>Reset Rute</span>
              </button>
            </div>

            <div className="space-y-2">
              {SIM_STEPS.map((step, idx) => (
                <button
                  key={idx}
                  onClick={() => handleSelectStep(idx)}
                  className={`w-full flex items-center justify-between gap-3 px-3.5 py-2.5 rounded-lg border text-left text-xs transition-colors ${
                    stepIdx === idx && screenState === 'nav'
                      ? 'bg-cyan-600/15 border-cyan-500 text-white'
                      : 'bg-slate-950 border-slate-800 text-slate-300 hover:border-slate-700'
                  }`}
                >
                  <span className="truncate font-medium">
                    {idx + 1}. {step.instruction}
                  </span>
                  <span className="font-mono text-[11px] text-cyan-400 tabular-nums shrink-0">
                    {step.distanceM} m
                  </span>
                </button>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
