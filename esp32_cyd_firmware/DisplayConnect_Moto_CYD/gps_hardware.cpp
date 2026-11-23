#include "gps_hardware.h"
#include "config.h"
#include <Arduino.h>

#if ENABLE_HW_GPS
#include <TinyGPS++.h>

static TinyGPSPlus s_gps;
static HardwareSerial s_gpsSerial(2); // UART2 on ESP32
static bool s_gpsInitialized = false;

void hwGpsInit() {
  s_gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  s_gpsInitialized = true;
  Serial.printf("Hardware GPS UART2 started (RX=%d, TX=%d, %d baud)\n",
                GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
}

void hwGpsPoll(HwGpsData& outData) {
  if (!s_gpsInitialized) return;

  bool updated = false;
  while (s_gpsSerial.available() > 0) {
    if (s_gps.encode(s_gpsSerial.read())) {
      updated = true;
    }
  }

  if (updated) {
    outData.satellites = s_gps.satellites.isValid() ? static_cast<uint8_t>(s_gps.satellites.value()) : 0;
    outData.fixValid = s_gps.location.isValid() && (s_gps.location.age() < 3000);
    if (outData.fixValid) {
      outData.lat = s_gps.location.lat();
      outData.lon = s_gps.location.lng();
      outData.speedKmh = s_gps.speed.isValid() ? static_cast<float>(s_gps.speed.kmph()) : 0.0f;
      outData.courseDeg = s_gps.course.isValid() ? static_cast<float>(s_gps.course.deg()) : 0.0f;
      outData.altitudeM = s_gps.altitude.isValid() ? static_cast<float>(s_gps.altitude.meters()) : 0.0f;
      outData.lastUpdateMs = millis();
    }
  }
}

#else

void hwGpsInit() {}
void hwGpsPoll(HwGpsData& outData) { (void)outData; }

#endif
