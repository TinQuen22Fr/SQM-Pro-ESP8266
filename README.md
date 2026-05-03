# SQM Pro — ESP8266 DIY Sky Quality Meter

Firmware for a **DIY Sky Quality Meter** built around an ESP8266 (NodeMCU),
a TSL2591 light sensor, a BME280 weather sensor, a 128×64 OLED display
(SH1106 or SSD1306) and an optional NEO-6 GPS module.

This firmware pushes measurements over HTTPS to the
**[SQM Nightwatch](https://sqm.quentin-astro.fr)** dashboard
(the `magnitude-tracker` backend).

```
              ┌────────────────────────┐
              │  ESP8266 (NodeMCU)     │
   TSL2591 ──▶│  + OLED + BME280 + GPS │── Wi-Fi ──▶ https://sqm.quentin-astro.fr/api/sqm_push
              └────────────────────────┘
```

---

## 1. Hardware

| Component                 | Details                                            |
|---------------------------|----------------------------------------------------|
| MCU                       | ESP8266 NodeMCU v1.0                               |
| Light sensor              | Adafruit TSL2591 (I²C, 0x29)                       |
| Weather sensor            | BME280 (I²C, 0x76 or 0x77, configurable)           |
| Display                   | SH1106 1.3" **or** SSD1306 0.96" (I²C, HW)          |
| GPS (optional)            | u-blox NEO-6M on `D7` (RX=GPIO13) / `D8` (TX=GPIO15) using `SoftwareSerial` |
| Mode switch               | On `GPIO2` (`ModePin`), pulled up internally       |
| Buzzer                    | On `GPIO12` (`BuzzerPin`)                          |
| Battery monitor           | `A0` (voltage divider giving a ×11 ratio)          |

See wiring diagram / PCB here: <https://easyeda.com/hujer.roman/sqm-hr>.

---

## 2. Arduino IDE setup

1. Install **Arduino IDE** ≥ 2.0.
2. In *File → Preferences → Additional boards manager URLs* add:
   ```
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
3. In *Tools → Board → Boards Manager*, install **esp8266 by ESP8266 Community** ≥ 3.1.x.
4. Select the board: *Tools → Board → ESP8266 Boards → **NodeMCU 1.0 (ESP-12E Module)***.
5. Install the following libraries from *Tools → Manage Libraries…*:

| Library                                | Tested version |
|----------------------------------------|----------------|
| Adafruit Unified Sensor                | ≥ 1.1.14       |
| BMx280MI (by Gregor Christandl)        | ≥ 1.2.3        |
| U8g2 *(provides `U8x8lib`)*            | ≥ 2.34.x       |
| TinyGPSPlus                            | ≥ 1.0.3        |

The TSL2591 support is bundled with this repo (`SQM_TSL2591.h/.cpp`)
so nothing to install for it.

6. `ESP8266WiFi` and `WiFiClientSecure` come with the ESP8266 core — no extra install.

---

## 3. Configure your sensor

Open **`Config.h`** and adjust:

```cpp
// Wi-Fi (primary and optional fallback)
const char* ssid     = "YourWiFiSSID";
const char* password = "YourWiFiPassword";
#define ALT_SSID_ON
const char* ssid2     = "BackupSSID";
const char* password2 = "BackupPassword";

// Sensor identity (each physical device must have a unique ID)
const char* SensorID    = "SQM-001";   // change to SQM-002, SQM-003, ...
const char* sensor_key  = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

// Display: pick exactly one
#define SH1106_ON      // 1.3" panel
#define SSD1306_OFF    // 0.96" panel
```

The `sensor_key` is generated from the **Configuration** tab of the
[SQM Nightwatch dashboard](https://sqm.quentin-astro.fr). **The same key
can be used for several physical sensors**: just give each one a
distinct `SensorID` (e.g. `SQM-001`, `SQM-002`, …). The backend stores
each stream separately under `device_id`.

---

## 4. Flash

1. Plug the NodeMCU in USB.
2. *Tools → Port* → select the correct COM / tty.
3. *Sketch → Upload*.

Typical compile/flash sizes on NodeMCU (ESP-12E, 4 MB flash):

```
Sketch uses ~370 KB  (35 %) of program storage.
Global variables use ~33 KB (40 %) of dynamic memory.
```

---

## 5. How it runs

### Two modes, switched by the mode button (`ModePin`)

**Normal mode (button NOT pressed):**
* Reads TSL2591 (`mpsas`, `dmpsas`), BME280 (T/H/P), GPS (lat/lng/alt/sat).
* Displays on the OLED.
* Every ~10 s (5 cycles of 2 s), when Wi-Fi is connected, sends an
  HTTPS GET to:
  ```
  https://sqm.quentin-astro.fr/api/sqm_push?
      ID=<SensorID>&KEY=<sensor_key>
    & T=<temp °C>&H=<hum %>&P=<pressure hPa>
    & S=<mpsas>&D=<error>&V=<battery V>
    & L=<lux>
    & Alt=<GPS alt m>&Lat=<GPS lat>&Lon=<GPS lng>     (if GPS locked)
  ```
  TLS is used but certificate validation is disabled (`setInsecure()`)
  because the ESP8266 does not have enough RAM to validate a full CA
  chain. The payload is still encrypted on the wire.

**USB mode (button pressed):**
Implements the Unihedron-compatible serial protocol (`i`, `r`, `u`,
`w` *(extended with weather)*, `g`, `z…` calibration commands,
`A50/A51/A5d/A5e/A5`). See `SQM_pro.ino` for the full command list.

### OLED pages

1. First page: boot banner (version, S/N, TSL/BME status)
2. Calibration page: stored offsets (SQM, temperature, auto-contrast)
3. Measurement page: date/time UT, magnitude, temp, humidity, pressure,
   altitude, sat count, lat/lon
4. Wait page (GPS not yet locked / USB mode)

---

## 6. Repository layout

```
.
├── LICENSE
├── README.md
├── .gitignore
└── SQM_pro/
    ├── SQM_pro.ino        ← main sketch (setup + loop + USB protocol)
    ├── Config.h           ← user configuration (WiFi, SensorID, key, ...)
    ├── Setup.h            ← hardware pin-out / BME I²C address / OLED font
    ├── Validate.h         ← compile-time checks
    ├── EEPROM.ino         ← persistence of calibrations & display settings
    ├── GPS.ino            ← NEO-6 helpers
    ├── MyLib.ino          ← OLED pages + BME280 reader + buzzer
    ├── WiFi.ino           ← Wi-Fi STA + HTTPS uplink to /api/sqm_push
    ├── SQM_TSL2591.h      ← TSL2591 driver (header)
    └── SQM_TSL2591.cpp    ← TSL2591 driver (implementation)
```

---

## 7. Troubleshooting

| Symptom                                    | Likely cause / fix                                                 |
|--------------------------------------------|--------------------------------------------------------------------|
| Boot loop + rapid buzzer (init error)      | BME280 or TSL2591 wiring — check I²C address in `Setup.h`           |
| `OLED Err` on serial                        | Wrong display model — toggle `SH1106_ON` / `SSD1306_ON` in Config.h |
| Wi-Fi connects but no data on dashboard    | `sensor_key` wrong or `SensorID` not yet declared server-side       |
| `HTTPS Timeout !` in debug serial          | Firewall/NAT blocking port 443, or slow DNS — retry 2–3 loops       |
| TLS handshake fails                        | Not enough free heap — disable `DEBUG_WIFI_ON` / `EXTENDET_PROTOCOL_ON` |
| Compile error: `WiFiClientSecure.h: No such file` | Update the ESP8266 core to ≥ 2.5.0                            |
| No GPS lock                                | Comment `#define GPS_ON` to disable GPS, or improve sky view        |

Enable serial debug by setting `DEBUG_WIFI_ON` (and/or `DEBUG_ON`,
`DEBUG_GPS_ON`) in `Config.h`, then open Serial Monitor at
**74880 baud** (or 115200 if you change `SERIAL_BAUD`).

---

## 8. License

GPL-3.0 — see `LICENSE`. The TSL2591 driver is distributed under the
original BSD license from Adafruit (preserved in the header comments).

Copyright © 2025 Quentin Dumont.
