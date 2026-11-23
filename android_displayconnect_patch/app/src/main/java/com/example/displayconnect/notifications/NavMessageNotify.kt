package com.example.displayconnect.notifications

import org.json.JSONObject
import java.text.Normalizer

/**
 * JSON notification & media metadata builder compatible with DisplayConnect CYD ESP32 firmware.
 * Firmware expects compact single-line JSON terminated by '\n':
 * - {"type":"notify","kind":"whatsapp|call|info|clear","sender":"...","message":"..."}
 * - {"type":"media","state":"playing|paused|stopped","title":"...","artist":"...","app":"..."}
 */
object NavMessageNotify {

    private fun cleanText(input: String?, maxLen: Int): String {
        if (input.isNullOrBlank()) return ""
        val normalized = Normalizer.normalize(input, Normalizer.Form.NFC)
        return normalized
            .replace('\n', ' ')
            .replace('\r', ' ')
            .replace(Regex("\\s+"), " ")
            .trim()
            .take(maxLen)
    }

    fun whatsapp(sender: String, message: String): String {
        return JSONObject()
            .put("type", "notify")
            .put("kind", "whatsapp")
            .put("sender", cleanText(sender, 46).ifEmpty { "WhatsApp" })
            .put("message", cleanText(message, 94).ifEmpty { "Pesan baru" })
            .toString()
    }

    fun call(caller: String, subtitle: String = "Panggilan masuk"): String {
        return JSONObject()
            .put("type", "notify")
            .put("kind", "call")
            .put("sender", cleanText(caller, 46).ifEmpty { "Telepon Masuk" })
            .put("message", cleanText(subtitle, 94))
            .toString()
    }

    fun info(title: String, message: String): String {
        return JSONObject()
            .put("type", "notify")
            .put("kind", "info")
            .put("sender", cleanText(title, 46).ifEmpty { "Info" })
            .put("message", cleanText(message, 94))
            .toString()
    }

    fun clear(): String = """{"type":"notify","kind":"clear"}"""

    fun media(state: String, title: String, artist: String, app: String = "AUDIO"): String {
        val safeState = when (state.lowercase()) {
            "playing" -> "playing"
            "paused" -> "paused"
            else -> "stopped"
        }
        return JSONObject()
            .put("type", "media")
            .put("state", safeState)
            .put("title", cleanText(title, 60).ifEmpty { "Tidak ada lagu" })
            .put("artist", cleanText(artist, 44).ifEmpty { "Smartphone Audio" })
            .put("app", cleanText(app, 20).ifEmpty { "AUDIO" })
            .toString()
    }
}
