import React from 'react';
import { ActiveSection } from '../types';
import { PWAInstallButton } from './PWAInstallButton';
import { Download } from 'lucide-react';

interface NavbarProps {
  activeSection: ActiveSection;
  setActiveSection: (section: ActiveSection) => void;
  onDownloadZip: () => void;
  isDownloading: boolean;
}

export const Navbar: React.FC<NavbarProps> = ({
  activeSection,
  setActiveSection,
  onDownloadZip,
  isDownloading,
}) => {
  const navItems: { id: ActiveSection; label: string }[] = [
    { id: 'code', label: 'Kode ESP32' },
    { id: 'preview', label: 'Pratinjau HUD' },
    { id: 'config', label: 'Konfigurasi' },
    { id: 'wiring', label: 'Pinout & Wiring' },
    { id: 'chronos', label: 'Chronos App' },
    { id: 'ble-test', label: 'Uji BLE' },
  ];

  return (
    <header className="sticky top-0 z-40 flex items-center justify-between gap-8 px-6 py-3.5 border-b border-slate-800 bg-slate-950/95 backdrop-blur-sm">
      {/* Zone 1: Single text element wordmark */}
      <a
        href="#code"
        onClick={(e) => {
          e.preventDefault();
          setActiveSection('code');
        }}
        className="text-lg font-bold tracking-tight text-white whitespace-nowrap shrink-0"
      >
        DisplayConnect CYD
      </a>

      {/* Zone 2: Clean single-line text navigation links */}
      <nav className="hidden md:flex items-center gap-6 text-sm font-medium">
        {navItems.map((item) => (
          <button
            key={item.id}
            onClick={() => setActiveSection(item.id)}
            className={`whitespace-nowrap shrink-0 pb-0.5 transition-colors border-b-2 ${
              activeSection === item.id
                ? 'text-cyan-400 border-cyan-400'
                : 'text-slate-400 border-transparent hover:text-slate-100'
            }`}
          >
            {item.label}
          </button>
        ))}
      </nav>

      {/* Zone 3: Primary CTA */}
      <div className="flex items-center gap-3 shrink-0">
        <PWAInstallButton />
        <button
          onClick={onDownloadZip}
          disabled={isDownloading}
          className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-cyan-600 rounded-lg hover:bg-cyan-500 disabled:opacity-60 transition-colors whitespace-nowrap shrink-0"
        >
          <Download className="w-4 h-4" />
          <span>{isDownloading ? 'Membuat ZIP...' : 'Download .ZIP'}</span>
        </button>
      </div>
    </header>
  );
};
