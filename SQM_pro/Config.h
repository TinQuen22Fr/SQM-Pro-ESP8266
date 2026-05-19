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
// Depuis la v2.3.0 :
//   - WIFI_SSID / WIFI_PASSWORD ne sont plus nécessaires : les credentials
//     WiFi sont saisis par l'utilisateur via le portail captif au premier
//     boot (cf. WiFiPortal.ino) et persistés en flash.
//   - SENSOR_ID n'est plus utilisé : l'identifiant de la station est saisi
//     par l'utilisateur via le portail captif (champ "Nom de la station")
//     et stocké en EEPROM. Un nom par défaut `SQM-XXXXXX` est généré à
//     partir du chip ID si l'utilisateur n'en saisit pas.
//   - Seuls SENSOR_KEY, OTA_HOSTNAME et OTA_PASSWORD restent dans secrets.h.
//     SENSOR_KEY est injectée par la CI GitHub Actions depuis le secret du
//     repo, ce qui fait du .bin un binaire "pré-câblé" pour le serveur
//     central du projet coopératif.
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
// EEPROM
// -----------------------------------------------------------------------------
// Taille totale du buffer EEPROM virtuel de l'ESP8266 (en flash).
// 192 octets : 30 octets de slots historiques + 33 nom de station (v2.3.0)
// + 33 SSID secours + 65 password secours (v2.3.1) + marge.
// Si vous ajoutez de nouveaux champs, augmentez cette valeur en consequence.
#define EEPROM_SIZE 192

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

// WiFi credentials : SAISIS PAR L'UTILISATEUR via le portail captif
// (WiFiPortal.ino) au premier boot et persistés en flash par ESP8266WiFi.
// Pour forcer une reconfiguration : DOUBLE RESET physique (couper/rallumer
// le courant 2 fois dans les 5 secondes) → le firmware relance le portail.

// -----------------------------------------------------------------------------
// Sensor identity
// -----------------------------------------------------------------------------
// L'identifiant unique de la sonde (envoyé comme paramètre ID au backend) est
// saisi par l'utilisateur dans le portail captif (champ "Nom de la station")
// et stocké en EEPROM. La variable globale `gStationName` (cf. WiFiPortal.ino)
// est lue dynamiquement par wifi_main() à chaque push.
//
// La clé API reste hardcodée car elle est commune à TOUS les utilisateurs
// du projet coopératif : c'est le "passe-partout" vers le serveur central
// (sqm.quentin-astro.fr). Elle est injectée dans secrets.h par la CI GitHub
// depuis le secret de dépôt `SENSOR_KEY`.
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
// Battery monitoring (18650 Li-Ion via voltage divider on A0)
// -----------------------------------------------------------------------------
//
// Hardware setup (post-v2.2.8):
//
//   18650 (B+ TP4056) ---[ R1=100k ]---+---[ R2=100k ]--- GND
//                                      |
//                                      +----> A0 pin (NodeMCU)
//                                              |
//                                              +-> internal NodeMCU divider
//                                                  (220k + 100k typical)
//                                              +-> ESP8266 ADC (0-1V, 0-1023)
//
// The combination of the external divider + NodeMCU on-board divider + the
// ESP8266 ADC source-impedance loading gives a non-trivial transfer ratio
// that does NOT match the simple theoretical formula. We therefore use
// EMPIRICAL CALIBRATION rather than theory.
//
// CALIBRATION PROCEDURE:
//   1) Charge the battery and measure B+ with a multimeter.  ex: 3.99 V
//   2) Read the ADC raw value (printed on the OLED in WAIT mode and on
//      the Serial monitor every loop). Assume raw value: R.
//   3) Compute:
//        BATTERY_VOLTS_PER_RAW = Vmultimeter / R
//   4) Update the value below and reflash.
//
// Initial calibration from user measurement:
//   - Vmultimetre = 3.99 V (LiPo Yunique chargee a 100% via B6 V3)
//   - raw_avg empirique mesure via CP2102 ~ 6.82
//   - factor       = 3.99 / 6.82 ~= 0.585 V per raw unit
//
#define BATTERY_VOLTS_PER_RAW   0.585f
//
// Battery voltage range (your specific cell).
//
//   BATTERY_VMAX = voltage when YOUR charger says "100% / fully charged"
//                  (measure with multimeter, may differ from datasheet).
//                  Examples:
//                    - Std LiPo 1S + B6/iMax CCCV @ 4.20V cutoff -> 4.20
//                    - Std LiPo 1S + B6 with conservative 4.00V cutoff -> 4.00
//                    - Yunique 3000mAh (user setup) measured at 3.99V -> 3.99
//                    - 18650 Li-Ion stnd -> 4.20
//                    - LiFePO4 1S -> 3.65
//   BATTERY_VMIN = voltage at which you consider the cell "empty" (0%).
//                  Below this you risk damaging the cell.
//                  Typical 18650/LiPo Co/Mn: 3.00V hard cutoff, 3.20V safe.
//
// IMPORTANT: do NOT just keep the LiPo "standard" 4.20V here if your
// charger only reaches 3.99V or 4.00V. The display will never show 100%
// otherwise.
//
#define BATTERY_VMAX            3.99f   // user's measured "fully charged" V
#define BATTERY_VMIN            3.00f   // V - cutoff (do NOT discharge below)
#define BATTERY_LOW_THRESHOLD   3.20f   // V - low-battery warning threshold
//
// Number of ADC samples averaged per reading (reduces noise on high-impedance
// dividers; ESP8266 ADC is only 10-bit and rather noisy by itself).
#define BATTERY_OVERSAMPLES     8

// -----------------------------------------------------------------------------
// Battery percent curve selector
// -----------------------------------------------------------------------------
// Two strategies to convert tension -> percent:
//
//   BATTERY_CURVE_LINEAR    : simple proportional interpolation between
//                             BATTERY_VMIN (0%) and BATTERY_VMAX (100%).
//                             RECOMMENDED if your cell does not reach
//                             4.20V (custom charger profile, protected
//                             BMS with lower cutoff, non-standard LiPo,
//                             Li-Ion 18650 with safety limit, etc.).
//                             Just set BATTERY_VMAX to the voltage your
//                             multimeter reads when your charger says
//                             "100% / fully charged".
//
//   BATTERY_CURVE_LIPO_REAL : piecewise approximation of a typical 1S
//                             LiPo discharge curve (plateau 3.7-4.0V,
//                             knee at 3.6V, cutoff 3.0V). Hardcoded
//                             breakpoints, only valid if BATTERY_VMAX
//                             is close to 4.20V.
//
// Default: LINEAR (works with any chemistry / any VMAX).
//
#define BATTERY_CURVE_LINEAR
//#define BATTERY_CURVE_LIPO_REAL

// -----------------------------------------------------------------------------
// SQM Calibration certificate (optional, cosmetic)
// -----------------------------------------------------------------------------
// Inspired by the Unihedron SQM-LU calibration certificate. These values are
// returned by the `cx` (calibration info) UDM command to provide a
// "factory-style" identity to the DIY device.
//
// IMPORTANT - These macros do NOT affect actual measurements:
//   - The light-sensor offset actually APPLIED to readings comes from
//     `SqmCalOffset` (EEPROM, default = SQM_CAL_OFFSET above).
//   - The temperature compensation actually applied comes from
//     `TempCalOffset` (EEPROM, default = TEMP_CAL_OFFSET above).
//   - Live readings (rx, ux, wx) ALWAYS reflect the true sensor + EEPROM
//     calibration, not the values below.
//
// In other words: changing the macros below ONLY changes what `cx` returns
// to UDM (the calibration certificate fields). All live measurements
// continue to function normally regardless.
//
// You can leave these as-is if you don't need a custom certificate.
//
#define DIY_LIGHT_CAL_OFFSET      0.00f    // mags/arcsec^2 - 0 = use SqmCalOffset (recommended)
#define DIY_DARK_CAL_TIME_PERIOD  0.0f     // seconds - DIY has no dark sensor (0)
#define DIY_LIGHT_CAL_TEMP_FROM_BME280     // when defined: light cal temp = current BME280 reading
//#define DIY_LIGHT_CAL_TEMP    19.9f      // alternative: hard-coded value (uncomment + comment above)
#define DIY_DARK_CAL_OFFSET       0.00f    // mags/arcsec^2 - DIY has no dark sensor (0)
#define DIY_DARK_CAL_TEMP_FROM_BME280      // when defined: dark cal temp = current BME280 reading
//#define DIY_DARK_CAL_TEMP     20.9f      // alternative: hard-coded value (uncomment + comment above)

// -----------------------------------------------------------------------------
// Compile-time consistency: deep-sleep and OTA cannot coexist.
// -----------------------------------------------------------------------------
#if defined(DEEP_SLEEP_ON) && defined(OTA_ON)
  #warning "DEEP_SLEEP_ON and OTA_ON are both defined; OTA will rarely be reachable."
#endif

#endif // CONFIG_H
