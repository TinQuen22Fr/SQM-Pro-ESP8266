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
// Enable cloud upload
#define WIFI_ON

// Backend endpoint (HTTPS is mandatory: the server redirects HTTP -> HTTPS)
const char* host = "sqm.quentin-astro.fr";
String app = "/api/sqm_push";   // ingestion endpoint on the backend
#define HTTP_PORT 443            // HTTPS

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
// The backend accepts any ID (SQM-001, SQM-002, ...) so several devices
// can push to the same API key; they will be stored under distinct device_id.
const char* SensorID = "SQM-001";

// API key generated from the "Configuration" tab of the SQM Nightwatch dashboard
// It is sent both as a query-string parameter (KEY=...) for GET
// and as a header (X-API-Key: ...) for POST.
const char* sensor_key = "mhRddaq4R-3_P1ony-mFz0xD-YJ_sktmiB5N-e2nfIs";

// -----------------------------------------------------------------------------
// Reporting cadence
// -----------------------------------------------------------------------------
#define SLEEP_SEC 300 // Reserved for future use (deep-sleep scenarios)

// -----------------------------------------------------------------------------
// OLED display - select ONLY ONE
// -----------------------------------------------------------------------------
#define SH1106_ON    // SH1106 1.3" 128x64 OLED display (default)
#define SSD1306_OFF  // SSD1306 0.96" 128x64 OLED display

// -----------------------------------------------------------------------------
// Extended USB protocol (weather info over serial) - disable to save flash
// -----------------------------------------------------------------------------
#define EXTENDET_PROTOCOL_ON

// -----------------------------------------------------------------------------
// Defaults (overridden by EEPROM values when present)
// -----------------------------------------------------------------------------
#define DEFALUT_CONTRAS   0     // default display contrast
#define SQM_CAL_OFFSET   -1.0   // default SQM calibration offset
#define TEMP_CAL_OFFSET   0.0   // default temperature calibration offset

#endif // CONFIG_H
