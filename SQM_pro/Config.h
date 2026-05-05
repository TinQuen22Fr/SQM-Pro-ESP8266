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
// Local secrets (Wi-Fi credentials, API key, OTA password, hostname)
// -----------------------------------------------------------------------------
// To keep your credentials OUT of the git repository:
//   1. cp secrets.h.example secrets.h
//   2. Edit secrets.h with your real values (it is gitignored).
//
// If secrets.h is missing, harmless placeholder values are used so the
// firmware still compiles - it just won't connect to your real Wi-Fi until
// you create the file.
#if __has_include("secrets.h")
  #include "secrets.h"
#endif

// Fallback default values - DO NOT put real secrets here, edit secrets.h
#ifndef WIFI_SSID
  #define WIFI_SSID         "YourPrimarySSID"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD     "YourPrimaryPassword"
#endif
#ifndef WIFI_SSID_ALT
  #define WIFI_SSID_ALT     "YourBackupSSID"
#endif
#ifndef WIFI_PASSWORD_ALT
  #define WIFI_PASSWORD_ALT "YourBackupPassword"
#endif
#ifndef SENSOR_ID
  #define SENSOR_ID         "SQM-001"
#endif
#ifndef SENSOR_KEY
  #define SENSOR_KEY        "paste-your-real-api-key-here"
#endif
#ifndef OTA_HOSTNAME
  #define OTA_HOSTNAME      "sqm-pro-001"
#endif
#ifndef OTA_PASSWORD
  #define OTA_PASSWORD      "change-me-please"
#endif

// -----------------------------------------------------------------------------
// Debug flags
// -----------------------------------------------------------------------------
// Set the *_ON variant (instead of *_OFF) of any of these to enable verbose
// logs on the serial console. Disabling debug saves ~1-2 KB of IRAM, which
// is precious on ESP8266 (HTTPS + OTA + SoftwareSerial already consume
// most of the IRAM budget).
#define DEBUG_OFF
#define DEBUG_GPS_OFF
#define DEBUG_WIFI_OFF

// Serial port baud rate. 115200 8N1 matches the Unihedron SQM-LU/LR spec
// so that UDM (Unihedron Device Manager) can detect and dialog with the
// device when the front-panel switch is in USB position.
//
// Note: ESP8266 boot diagnostics are emitted by the chip itself at 74880
// (hardware-fixed) regardless of this setting. Open Serial Monitor at
// 74880 only if you want to read those boot messages; otherwise stay at
// 115200 to communicate with UDM and the firmware Unihedron protocol.
#define SERIAL_BAUD 115200

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

// WiFi credentials (primary) - actual values come from secrets.h
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Alternative WiFi credentials (if primary fails)
#define ALT_SSID_ON
const char* ssid2     = WIFI_SSID_ALT;
const char* password2 = WIFI_PASSWORD_ALT;

// -----------------------------------------------------------------------------
// Sensor identity
// -----------------------------------------------------------------------------
// Values come from secrets.h. Use a different SENSOR_ID per physical device
// (SQM-001, SQM-002, ...). Same API key may be shared by all devices.
const char* SensorID   = SENSOR_ID;
const char* sensor_key = SENSOR_KEY;

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
// The actual value comes from secrets.h (OTA_HOSTNAME).
const char* ota_hostname = OTA_HOSTNAME;

// Password required by Arduino IDE before pushing a new firmware over OTA.
// CHANGE THIS to a secret value in secrets.h before flashing!
const char* ota_password = OTA_PASSWORD;

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
// USB / Unihedron mode
// -----------------------------------------------------------------------------
// The SQM-HR PCB by Roman Hujer has a dedicated switch wired to ModePin (GPIO2)
// that lets the user toggle between:
//   - Normal mode   (switch OFF / not pressed): OLED + Wi-Fi push to dashboard
//   - USB mode      (switch ON  /     pressed): Unihedron-compatible serial
//                                               protocol (commands i, r, u, w,
//                                               g, z..., A5...) for SQM-LE
//                                               software over USB
//
// USB_MODE_ON  -> read ModePin every loop and switch between modes.
//                 DEFAULT: matches the SQM-HR PCB / front-panel-switch design.
//
// USB_MODE_OFF -> ModePin is ignored, firmware always runs in normal mode.
//                 Use this on a *bare* NodeMCU (without the SQM-HR PCB) where
//                 GPIO2 is also wired to the on-board blue LED and to Serial1
//                 TX during boot. The pin then flaps between LOW and HIGH on
//                 its own and the OLED would oscillate between the
//                 "Wait USB data" page and the measurement page.
#define USB_MODE_ON
// #define USB_MODE_OFF

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
