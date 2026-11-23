import express from 'express';
import http from 'http';
import { createServer as createViteServer } from 'vite';
import path from 'path';
import fs from 'fs';
import { fileURLToPath } from 'url';
import dotenv from 'dotenv';
import { GoogleGenAI } from '@google/genai';

dotenv.config();

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(express.json());

const ai = new GoogleGenAI({
  apiKey: process.env.GEMINI_API_KEY || '',
  httpOptions: {
    headers: {
      'User-Agent': 'aistudio-build',
    },
  },
});

interface FirmwareFile {
  name: string;
  path: string;
  language: string;
  content: string;
  bytes: number;
}

function getLanguageFromExt(filename: string): string {
  if (filename.endsWith('.ino') || filename.endsWith('.cpp') || filename.endsWith('.h')) return 'cpp';
  if (filename.endsWith('.kt')) return 'kotlin';
  if (filename.endsWith('.xml')) return 'xml';
  if (filename.endsWith('.md')) return 'markdown';
  if (filename.endsWith('.ini')) return 'ini';
  return 'text';
}

function collectFirmwareFiles(baseDir: string, currentDir: string, out: FirmwareFile[]) {
  if (!fs.existsSync(currentDir)) return;
  const entries = fs.readdirSync(currentDir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(currentDir, entry.name);
    if (entry.isDirectory()) {
      collectFirmwareFiles(baseDir, fullPath, out);
    } else if (entry.isFile()) {
      const relPath = path.relative(baseDir, fullPath).replace(/\\/g, '/');
      const content = fs.readFileSync(fullPath, 'utf-8');
      out.push({
        name: entry.name,
        path: relPath,
        language: getLanguageFromExt(entry.name),
        content,
        bytes: Buffer.byteLength(content, 'utf-8'),
      });
    }
  }
}

app.get('/api/firmware/files', (_req, res) => {
  try {
    const fwDir = path.resolve(__dirname, 'esp32_cyd_firmware');
    const files: FirmwareFile[] = [];
    collectFirmwareFiles(fwDir, fwDir, files);

    // Put main .ino, config.h, platformio.ini, User_Setup_CYD.h first
    const priority = [
      'DisplayConnect_Moto_CYD/DisplayConnect_Moto_CYD.ino',
      'DisplayConnect_Moto_CYD/config.h',
      'DisplayConnect_Moto_CYD/User_Setup_CYD.h',
      'platformio.ini',
      'DisplayConnect_Moto_CYD/map_renderer.cpp',
      'DisplayConnect_Moto_CYD/map_renderer.h',
      'DisplayConnect_Moto_CYD/chronos_bridge.cpp',
      'DisplayConnect_Moto_CYD/chronos_bridge.h',
      'DisplayConnect_Moto_CYD/nav_protocol.cpp',
      'DisplayConnect_Moto_CYD/nav_protocol.h',
      'DisplayConnect_Moto_CYD/gps_hardware.cpp',
      'DisplayConnect_Moto_CYD/gps_hardware.h',
      'DisplayConnect_Moto_CYD/touch_cyd.cpp',
      'DisplayConnect_Moto_CYD/touch_cyd.h',
      'DisplayConnect_Moto_CYD/maps_theme.cpp',
      'DisplayConnect_Moto_CYD/maps_theme.h',
      'DisplayConnect_Moto_CYD/nav_types.h',
    ];

    files.sort((a, b) => {
      const ia = priority.indexOf(a.path);
      const ib = priority.indexOf(b.path);
      if (ia !== -1 && ib !== -1) return ia - ib;
      if (ia !== -1) return -1;
      if (ib !== -1) return 1;
      return a.path.localeCompare(b.path);
    });

    res.json({ files });
  } catch (error: any) {
    res.status(500).json({ error: error.message || 'Failed to read firmware files' });
  }
});

app.get('/api/android/files', (_req, res) => {
  try {
    const androidDir = path.resolve(__dirname, 'android_displayconnect_patch');
    const files: FirmwareFile[] = [];
    collectFirmwareFiles(androidDir, androidDir, files);

    const priority = [
      'README_ANDROID_PATCH.md',
      'app/src/main/java/com/example/displayconnect/notifications/MediaControlBridge.kt',
      'app/src/main/java/com/example/displayconnect/notifications/WhatsAppNotificationListenerService.kt',
      'app/src/main/java/com/example/displayconnect/notifications/IncomingCallMonitor.kt',
      'app/src/main/java/com/example/displayconnect/notifications/NavMessageNotify.kt',
      'app/src/main/java/com/example/displayconnect/notifications/NotificationPermissionHelper.kt',
      'app/src/main/AndroidManifest_PATCH.xml',
      'app/src/main/java/com/example/displayconnect/DisplayConnectApp_PATCH.kt',
    ];

    files.sort((a, b) => {
      const ia = priority.indexOf(a.path);
      const ib = priority.indexOf(b.path);
      if (ia !== -1 && ib !== -1) return ia - ib;
      if (ia !== -1) return -1;
      if (ib !== -1) return 1;
      return a.path.localeCompare(b.path);
    });

    res.json({ files });
  } catch (error: any) {
    res.status(500).json({ error: error.message || 'Failed to read Android patch files' });
  }
});

// Optional AI Assistant endpoint for ESP32 CYD wiring/compilation troubleshooting
app.post('/api/ai/firmware-help', async (req, res) => {
  try {
    const { question, boardVariant } = req.body;
    if (!process.env.GEMINI_API_KEY) {
      return res.status(500).json({ error: 'GEMINI_API_KEY belum dikonfigurasi.' });
    }
    const prompt = `Kamu adalah engineer ESP32 dan PlatformIO/Arduino yang ahli pada board ESP32-2432S028R (Cheap Yellow Display / CYD) dan repositori malaq88/DisplayConnect.
Board varian user: ${boardVariant || 'ESP32-2432S028R ILI9341 1-USB'}.
Pertanyaan user: "${question}"
Berikan jawaban teknis yang singkat, akurat, dan langsung ke solusi dalam Bahasa Indonesia.`;

    const response = await ai.models.generateContent({
      model: 'gemini-4-hydrogen',
      contents: prompt,
    });
    res.json({ answer: response.text || '' });
  } catch (error: any) {
    res.status(500).json({ error: error.message || 'Gagal memproses pertanyaan' });
  }
});

async function startServer() {
  const isProduction = process.env.NODE_ENV === 'production';
  const port = process.env.PORT || 3000;
  const httpServer = http.createServer(app);

  if (!isProduction) {
    process.env.DISABLE_HMR = 'true';
    const vite = await createViteServer({
      server: {
        middlewareMode: true,
        hmr: false,
        ws: false,
      },
      appType: 'spa',
    });
    app.get('/dev-sw.js', (_req, res) => {
      res.type('application/javascript').setHeader('Cache-Control', 'no-store');
      res.send(`
        self.addEventListener('install', () => self.skipWaiting());
        self.addEventListener('activate', (event) => {
          event.waitUntil(
            caches.keys().then((keys) => Promise.all(keys.map((k) => caches.delete(k))))
              .then(() => self.registration.unregister())
          );
        });
      `);
    });
    app.get('/@vite/client', async (_req, res, next) => {
      try {
        const result = await vite.transformRequest('/@vite/client');
        if (result?.code) {
          const patched = result.code
            .replace(
              'transport.connect(createHMRHandler(handleMessage));',
              '/* HMR disabled */'
            )
            .replace(
              'else throw new SendBeforeConnectError("send was called before connect");',
              'else return;'
            )
            .replace(
              'else throw new SendBeforeConnectError("invoke was called before connect");',
              'else return;'
            );
          res.type('application/javascript').send(patched);
          return;
        }
      } catch {
        // Fallback to Vite default middleware
      }
      next();
    });
    app.use(vite.middlewares);
  } else {
    const distPath = path.resolve(__dirname, 'dist');
    app.use(express.static(distPath));
    app.get('*', (_req, res) => {
      res.sendFile(path.join(distPath, 'index.html'));
    });
  }

  httpServer.listen(port, () => {
    console.log(`DisplayConnect CYD Firmware Server running on http://localhost:${port}`);
  });
}

startServer();
