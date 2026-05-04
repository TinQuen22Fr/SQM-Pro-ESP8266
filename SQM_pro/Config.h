// Config.h
// User specific configuration for SQM
//
// Copyright (c) 2025 Quentin Dumont
//
// Adapted to send data to the SQM Nightwatch backend
// (magnitude-tracker) at https://sqm.quentin-astro.fr
//
#ifndef CONFIG_H
#define CONFIG_H

// -----------------------------------------------------------------------------
// Debug flags
// -----------------------------------------------------------------------------
#define DEBUG_OFF
#define DEBUG_GPS_OFF
#define DEBUG_WIFI_ON

#define SERIAL_BAUD 74880 // Serial port baud. Default is ESP 74880 or 115200

// -----------------------------------------------------------------------------
// GPS (NEO-6 module)
// -----------------------------------------------------------------------------
#define GPS_ON // NEO-6 GPS module support
#define GPSBaud 9600

// -----------------------------------------------------------------------------
// WiFi / Cloud upload to SQM Nightwatch (magnitude-tracker)
// -----------------------------------------------------------------------------
#define WIFI_ON

// Backend endpoint (HTTPS is mandatory: the server redirects HTTP -> HTTPS)
const char* host = "sqm.quentin-astro.fr";
String app = "/api/sqm_push";
#define HTTP_PORT 443

// WiFi credentials (primary)
const char* ssid     = "AstroHR";
const char* password = "12345678";

// Alternative WiFi credentials (if primary fails)
#define ALT_SSID_ON
const char* ssid2     = "HujerIoT";
const char* password2 = "Hujer.I.0.T";

// -----------------------------------------------------------------------------
// Sensor identity
// -----------------------------------------------------------------------------
// Unique identifier for this SQM device on the backend.
const char* SensorID   = "SQM-001";
const char* sensor_key = "mhRddaq4R-3_P1ony-mFz0xD-YJ_sktmiB5N-e2nfIs";

// -----------------------------------------------------------------------------
// Night-only push (skip daytime measurements)
// -----------------------------------------------------------------------------
// SQM measurements are only meaningful at night. During the day, the TSL2591
// is saturated and the magnitude is meaningless (~0-5 mag/arcsec^2). Pushing
// these to the dashboard just clutters the database.
//
// When NIGHT_ONLY_PUSH_ON is defined, the firmware uses the TSL2591 reading
// itself as a daylight detector: if the measured magnitude is below
// NIGHT_THRESHOLD_MPSAS, the push to the cloud is skipped (the OLED keeps
// showing the live value, and in deep-sleep mode the chip still goes back
// to sleep normally).
//
// Suggested thresholds (mag/arcsec^2):
//   10.0 = civil twilight (-6 deg) - early dusk
//   12.0 = nautical twilight (-12 deg) - default, useful night begins
//   13.0 = astronomical twilight (-18 deg) - strict full night
//
// Set NIGHT_ONLY_PUSH_OFF to always push (legacy behaviour).
#define NIGHT_ONLY_PUSH_ON
#define NIGHT_THRESHOLD_MPSAS 12.0f

// -----------------------------------------------------------------------------
// OTA (Over-The-Air firmware update)
// -----------------------------------------------------------------------------
// When enabled, the device advertises itself on the local Wi-Fi network and
// a new firmware can be uploaded from Arduino IDE without USB cable
// (Tools -> Port -> network port "sqm-pro-XXX at 192.168.x.x").
//
// IMPORTANT: OTA is incompatible with DEEP_SLEEP_ON because the chip is
// off most of the time. If you enable deep-sleep, comment OTA_ON below.
#define OTA_ON

// Hostname advertised on the local network (mDNS / Bonjour).
// Set a unique value per physical device, e.g. "sqm-pro-001", "sqm-pro-002".
const char* ota_hostname = "sqm-pro-001";

// Password required by Arduino IDE before pushing a new firmware over OTA.
// CHANGE THIS to a secret value before flashing!
const char* ota_password = "changeme-ota-password";

// -----------------------------------------------------------------------------
// Deep-sleep (battery-powered, low-power operation)
// -----------------------------------------------------------------------------
// When DEEP_SLEEP_ON is defined, every loop iteration:
//   1) connects Wi-Fi,
//   2) takes ONE measurement (TSL2591 + BME280 + GPS-best-effort),
//   3) pushes it to the backend,
//   4) calls ESP.deepSleep(SLEEP_SEC * 1e6) -> chip is off ~20 uA.
//
// HARDWARE REQUIREMENT: GPIO16 (NodeMCU pin D0) MUST be wired to the RST
// pin so the deep-sleep timer can reset the chip. Without this wire, the
// chip will sleep but never wake up. Use a 470 ohm resistor (or a Schottky
// diode) in series so the USB programmer can still hold RST low.
//
// CONSEQUENCES (when DEEP_SLEEP_ON is enabled):
//   - OTA is NOT available (chip is off most of the time)
//   - The OLED display is essentially useless (visible only ~10 s per cycle)
//   - The USB / Unihedron serial mode is bypassed
//   - GPS lock-time is limited (cold start may yield no fix)
//
// Default: disabled (continuous mode, mains-powered, OLED active, OTA on).
#define DEEP_SLEEP_OFF
// #define DEEP_SLEEP_ON   // uncomment for battery-powered unattended operation

// -----------------------------------------------------------------------------
// Reporting cadence
// -----------------------------------------------------------------------------
// Used by deep-sleep mode (sleep duration in seconds between two pushes).
// Continuous mode pushes every ~10 s regardless of this value.
#define SLEEP_SEC 300 // 5 minutes

// -----------------------------------------------------------------------------
// OLED display - select ONLY ONE
// -----------------------------------------------------------------------------
#define SH1106_ON
#define SSD1306_OFF

// -----------------------------------------------------------------------------
// Extended USB protocol (weather info over serial) - disable to save flash
// -----------------------------------------------------------------------------
#define EXTENDET_PROTOCOL_ON

// -----------------------------------------------------------------------------
// Defaults (overridden by EEPROM values when present)
// -----------------------------------------------------------------------------
#define DEFALUT_CONTRAS   0
#define SQM_CAL_OFFSET   -1.0
#define TEMP_CAL_OFFSET   0.0

// -----------------------------------------------------------------------------
// Compile-time consistency: deep-sleep and OTA cannot coexist.
// -----------------------------------------------------------------------------
#if defined(DEEP_SLEEP_ON) && defined(OTA_ON)
  #warning "DEEP_SLEEP_ON and OTA_ON are both defined; OTA will rarely be reachable."
#endif

#endif // CONFIG_H
