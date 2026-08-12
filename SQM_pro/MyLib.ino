// MyLib.ino
// Library of helper functions for SQM
//
// Copyright (c) 2025-2026 Quentin Dumont
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

void _blk_change_status() {
  if (Blik) {
    Blik = false;
  } else {
    Blik = true;
  }
} // end of _blk_change_status()

void buzzer(int _long) {
  for (signed int _i = 0; _i < _long / 2; _i++) {
    digitalWrite(BuzzerPin, 1);
    delay(1);
    digitalWrite(BuzzerPin, 0);
    delay(1);
  }
} // end of buzzer(int _long)

void ReadWeather() {
  if (bme.measure()) {
    do {
      delay(100);
    } while (!bme.hasValue());
    pres = bme.getPressure();
    temp = bme.getTemperature();
    if (Humidity) hum = bme.getHumidity();
    else          hum = 0;
    // v2.3.4 : applique les 3 offsets de calibration runtime (persistés EEPROM,
    // configurables via portail captif). Une sonde dans un boîtier fermé sub-
    // estime/surestime de quelques degrés/% par effet de chauffe interne ;
    // ces offsets compensent linéairement la dérive observée vs station de
    // référence.
    temp = temp + TempCalOffset;
    if (Humidity) hum = hum + HumCalOffset;
    pres = pres + PresCalOffset;
  }
}

// -----------------------------------------------------------------------------
// Battery (18650 Li-Ion) helpers
// -----------------------------------------------------------------------------
// Reads A0 BATTERY_OVERSAMPLES times, returns the EMPIRICALLY-calibrated
// battery voltage (see BATTERY_VOLTS_PER_RAW in Config.h for the calibration
// procedure when the divider hardware changes).
// -----------------------------------------------------------------------------
float readBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < BATTERY_OVERSAMPLES; i++) {
    sum += analogRead(A0);
    delay(2);
  }
  float raw = (float)sum / (float)BATTERY_OVERSAMPLES;
  return raw * BATTERY_VOLTS_PER_RAW;
}

// Battery percent calculation.
//
// Two strategies available (selected via BATTERY_CURVE_* macro in Config.h):
//
//   BATTERY_CURVE_LINEAR    -> proportional from VMIN (0%) to VMAX (100%).
//                              Works for ANY chemistry / any VMAX.
//                              Recommended when your cell does not reach
//                              4.20V (custom charger, BMS, etc.).
//
//   BATTERY_CURVE_LIPO_REAL -> piecewise approximation of a 1S LiPo
//                              discharge curve, hardcoded around 4.20V.
//                              Only valid if BATTERY_VMAX is ~4.20V.
//
byte getBatteryPercent(float v) {
#ifdef BATTERY_CURVE_LIPO_REAL
  // Realistic 1S LiPo discharge curve. Breakpoints assume 4.20V full.
  if (v >= 4.20f) return 100;
  if (v >= 4.10f) return  90 + (byte)((v - 4.10f) / 0.10f * 10);
  if (v >= 4.00f) return  80 + (byte)((v - 4.00f) / 0.10f * 10);
  if (v >= 3.90f) return  70 + (byte)((v - 3.90f) / 0.10f * 10);
  if (v >= 3.80f) return  55 + (byte)((v - 3.80f) / 0.10f * 15);
  if (v >= 3.70f) return  35 + (byte)((v - 3.70f) / 0.10f * 20);
  if (v >= 3.60f) return  20 + (byte)((v - 3.60f) / 0.10f * 15);
  if (v >= 3.50f) return  10 + (byte)((v - 3.50f) / 0.10f * 10);
  if (v >= 3.40f) return   5 + (byte)((v - 3.40f) / 0.10f * 5);
  if (v >= 3.30f) return   2 + (byte)((v - 3.30f) / 0.10f * 3);
  if (v >= BATTERY_VMIN) return (byte)((v - BATTERY_VMIN) / (3.30f - BATTERY_VMIN) * 2);
  return 0;
#else
  // Linear interpolation (default). Works with any VMAX/VMIN values.
  if (v >= BATTERY_VMAX) return 100;
  if (v <= BATTERY_VMIN) return 0;
  return (byte)((v - BATTERY_VMIN) / (BATTERY_VMAX - BATTERY_VMIN) * 100.0f + 0.5f);
#endif
}

// Smoothed battery percent reading: avoids the displayed value bouncing
// around with ADC noise (especially with a switching regulator like the
// MT3608 boost converter that injects HF noise on Vin).
// Update only if the new value differs by more than 2% from the last shown.
static byte _last_shown_pct = 255;  // 255 = not initialized
byte getBatteryPercentSmoothed(float v) {
  byte raw = getBatteryPercent(v);
  if (_last_shown_pct == 255) {
    _last_shown_pct = raw;
  } else {
    int delta = (int)raw - (int)_last_shown_pct;
    if (delta > 2 || delta < -2) {
      _last_shown_pct = raw;
    }
  }
  return _last_shown_pct;
}

// Returns the raw ADC reading averaged over BATTERY_OVERSAMPLES samples.
// Useful to recalibrate BATTERY_VOLTS_PER_RAW: print this value, measure
// the actual battery voltage, divide measurement by raw -> new V/raw factor.
float readBatteryRawAvg() {
  long sum = 0;
  for (int i = 0; i < BATTERY_OVERSAMPLES; i++) {
    sum += analogRead(A0);
    delay(2);
  }
  return (float)sum / (float)BATTERY_OVERSAMPLES;
}

// -----------------------------------------------------------------------------
// Time-throttled battery cache (v2.3.12, backport of main-branch v2.2.16)
// -----------------------------------------------------------------------------
// The main loop runs at a few Hz, which made the OLED battery display flicker
// between successive readings (high source impedance + MT3608 switching
// noise). To stabilize the display we only re-sample the battery every
// BATTERY_DISPLAY_INTERVAL_MS (default 5 s).
//   batteryRefreshIfDue() -> returns true if a fresh sample was taken now
//   getCachedBatteryVoltage()
//   getCachedBatteryPercent()
//   getCachedBatteryRawAvg()
// -----------------------------------------------------------------------------
static unsigned long _bat_last_update_ms = 0;
static float _bat_cached_vbat   = -1.0f;
static byte  _bat_cached_pcent  = 255;
static float _bat_cached_rawavg = 0.0f;

bool batteryRefreshIfDue() {
  unsigned long now = millis();
  bool firstCall = (_bat_cached_vbat < 0.0f);
  bool intervalElapsed = (now - _bat_last_update_ms) >= BATTERY_DISPLAY_INTERVAL_MS;
  bool millisRolled = (now < _bat_last_update_ms);
  if (firstCall || intervalElapsed || millisRolled) {
    long sum = 0;
    for (int i = 0; i < BATTERY_OVERSAMPLES; i++) {
      sum += analogRead(A0);
      delay(2);
    }
    _bat_cached_rawavg = (float)sum / (float)BATTERY_OVERSAMPLES;
    _bat_cached_vbat   = _bat_cached_rawavg * BATTERY_VOLTS_PER_RAW;
    _bat_cached_pcent  = getBatteryPercentSmoothed(_bat_cached_vbat);
    _bat_last_update_ms = now;
    return true;
  }
  return false;
}

float getCachedBatteryVoltage() {
  if (_bat_cached_vbat < 0) batteryRefreshIfDue();
  return _bat_cached_vbat;
}
byte getCachedBatteryPercent() {
  if (_bat_cached_pcent == 255) batteryRefreshIfDue();
  return _bat_cached_pcent;
}
float getCachedBatteryRawAvg() {
  if (_bat_cached_vbat < 0) batteryRefreshIfDue();
  return _bat_cached_rawavg;
}

// -----------------------------------------------------------------------------
// SQM "equivalent Hz" computation (v2.2.14)
// -----------------------------------------------------------------------------
// The Unihedron SQM-LU uses a TSL237 light-to-frequency sensor and reports
// the raw frequency in Hz on the `rx`/`ux` response. UDM displays this value
// in its UI and internally relates it to mpsas via:
//
//     mpsas = 22.0 - 2.5 * log10(Hz - DarkHz)
//
// Our DIY uses a TSL2591 (light-to-digital), so there is no "real" frequency
// to report. We compute an *equivalent* Hz by inverting the above formula:
//
//     Hz_equiv = 10 ^ ((22.0 - mpsas) / 2.5)
//
// This way:
//   - UDM displays a non-zero Hz field (previously always 0000000000Hz)
//   - If UDM recomputes mpsas from Hz via its standard formula, it gets
//     back our exact mpsas -> end-to-end consistency
//   - It does NOT add precision (Hz is derived from mpsas, not measured)
//
// Returns a left-zero-padded 10-character String for direct insertion in
// the rx/ux response.
// -----------------------------------------------------------------------------
String sqmHzEquivalent(double mpsas) {
  double hz = 0.0;
  if (!isnan(mpsas) && !isinf(mpsas)) {
    if (mpsas >= 22.0) {
      hz = 1.0;  // dark sky floor
    } else if (mpsas <= 0.0) {
      hz = 9999999999.0;  // saturation cap (10-digit max)
    } else {
      hz = pow(10.0, (22.0 - mpsas) / 2.5);
    }
  }
  if (hz < 0) hz = 0;
  if (hz > 9999999999.0) hz = 9999999999.0;

  // Convert to 10-char zero-padded string (handles values up to 10^10)
  char buf[16];
  dtostrf(hz, 10, 0, buf);  // width 10, 0 decimals (may pad with spaces)
  String s(buf);
  s.replace(' ', '0');
  return s;
}

// -----------------------------------------------------------------------------
// OLED pages
// -----------------------------------------------------------------------------
void DisplFirstPage() {
  OledDisp.setContrast(ReadEEcontras());
  if (page != 1) {
    OledDisp.clear();
    buzzer(200);
  }
  page = 1;
  OledDisp.setCursor(0, 0);
  // v2.3.11 : raccourci "SQM Ready V" -> "SQM v" pour tenir sur 16 cols
  // (sinon les versions 2 chiffres comme 2.3.10 sont tronquées)
  OledDisp.print("SQM v");
  OledDisp.print(Version);
  OledDisp.setCursor(0, 2);
  OledDisp.print("SN: ");
  OledDisp.print(SERIAL_NUMBER);
  OledDisp.setCursor(0, 4);
  OledDisp.print(TSL_Msg);
  OledDisp.setCursor(0, 6);
  OledDisp.print(BME_Msg);
}

void DisplCalData() {
  OledDisp.setContrast(ReadEEcontras());
  if (page != 2) {
    OledDisp.clear();
    buzzer(200);
  }
  page = 2;
  OledDisp.setCursor(0, 0);
  OledDisp.print("Calibration data");
  OledDisp.setCursor(0, 2);
  OledDisp.print("SQ offset:");
  if (SqmCalOffset >= 0) OledDisp.print(' ');
  OledDisp.print(String(SqmCalOffset, 2));
  OledDisp.print('M');
  OledDisp.setCursor(0, 4);
  OledDisp.print("TE offset:");
  if (TempCalOffset >= 0) OledDisp.print(' ');
  OledDisp.print(String(TempCalOffset, 1));
  OledDisp.print(char(0xb0));
  OledDisp.print('C');
  OledDisp.setCursor(0, 6);
  OledDisp.print("TC:");
  OledDisp.print((ReadEEAutoTempCal()) ? 'Y' : 'M');
  OledDisp.print(" DMMR:");
  if (ReadEEAutoContras())
    OledDisp.print("Auto");
  else {
    uint8_t _c = ReadEEcontras();
    if (_c < 10)       OledDisp.print(" 00");
    else if (_c < 100) OledDisp.print(" 0");
    OledDisp.print(_c);
  }
}

void DisplSqm(double mpsas, double dmpsas, float temp, byte hum, int pres, char blk) {
  char _tmp[20];
  float _lat, _lng;
  if (ReadEEAutoContras()) {
    if (mpsas < 10) {
      OledDisp.setContrast(150);
    } else if (mpsas < 15) {
      OledDisp.setContrast(50);
    } else {
      OledDisp.setContrast(0);
    }
  } else {
    OledDisp.setContrast(ReadEEcontras());
  }
  if (page != 3) {
    OledDisp.clear();
    buzzer(200);
  }
  page = 3;
  OledDisp.setCursor(0, 0);
  sprintf(_tmp, "%02d/%02d/%02d", g_year - 2000, g_month, g_day);
  OledDisp.print(_tmp);
  OledDisp.print(" UT");
  sprintf(_tmp, "%02d:%02d", g_hour, g_minute);
  OledDisp.print(_tmp);
#ifdef NIGHT_ONLY_PUSH_ON
  // v2.3.3 : l'indicateur OLED dépend désormais aussi du toggle runtime
  // `gNightOnlyPush`. En mode test (toggle OFF), on affiche "ALL" pour
  // signaler que le push est actif quelle que soit la luminosité.
  extern bool gNightOnlyPush;
  OledDisp.setCursor(13, 0);
  if (!gNightOnlyPush) {
    OledDisp.print("ALL");   // mode test → push toujours
  } else if (mpsas < NIGHT_THRESHOLD_MPSAS) {
    OledDisp.print("DAY");   // daytime detected -> push skipped
  } else {
    OledDisp.print("NGT");   // night -> push active
  }
#endif
  OledDisp.setCursor(0, 2);
  OledDisp.print('M');
  OledDisp.print(Blik ? blk : ' ');
  if (mpsas < 10) OledDisp.print('0');
  OledDisp.print(mpsas);
  OledDisp.print("mas");
  // v2.3.9 : température affichée avec 1 décimale (ex: 22.5C / -9.5C).
  // Le symbole `²` (après mas) et `°` (avant C) ont été retirés pour
  // libérer la colonne nécessaire à la décimale sur l'OLED 16 colonnes.
  {
    float _at = (temp < 0) ? -temp : temp;
    char  _sign = (temp < 0) ? '-' : ' ';
    if (_at < 10) OledDisp.print(' ');
    OledDisp.print(_sign);
    OledDisp.print(String(_at, 1));
    OledDisp.print('C');
  }
  OledDisp.setCursor(0, 4);
  OledDisp.print("H:");
  if (hum < 10)      { OledDisp.print("  "); }
  else if (hum < 100){ OledDisp.print(' '); }
  OledDisp.print(hum);
  OledDisp.print("% P:");
  pres = int((pres + g_alt / 8.3) + 0.5); // sea-level correction
  if (pres < 1000) { OledDisp.print(' '); }
  OledDisp.print(pres);
  OledDisp.print("hPa");
  if (GPS_sync) {
    OledDisp.setCursor(0, 5);
    OledDisp.print("Alt:");
    if (g_alt < 10)       { OledDisp.print("   "); }
    else if (g_alt < 100) { OledDisp.print("  ");  }
    else if (g_alt < 1000){ OledDisp.print(' ');   }
    OledDisp.print(int(g_alt + .5));
    OledDisp.print("m Sat:");
    OledDisp.print(g_sat < 10 ? g_sat : 9);
    OledDisp.setCursor(0, 6);
    OledDisp.print("Lat: ");
    _lat = g_lat < 0 ? -g_lat : g_lat;
    OledDisp.print(int(_lat));
    OledDisp.print(char(0xb0));
    OledDisp.print(i_g_min_s(_lat));
    OledDisp.print('\'');
    OledDisp.print(i_g_sec_s(_lat));
    OledDisp.print('\"');
    OledDisp.print(g_lat < 0 ? 'S' : 'N');
    OledDisp.setCursor(0, 7);
    OledDisp.print("Lon: ");
    _lng = g_lng < 0 ? -g_lng : g_lng;
    OledDisp.print(int(_lng));
    OledDisp.print(char(0xb0));
    OledDisp.print(i_g_min_s(_lng));
    OledDisp.print('\'');
    OledDisp.print(i_g_sec_s(_lng));
    OledDisp.print('\"');
    OledDisp.print(g_lng < 0 ? 'W' : 'E');
  } else {
    // No GPS fix yet: differentiate "no data on the serial wire at all"
    // (likely wiring/power issue) from "data received but no satellite
    // fix yet" (just need a clearer view of the sky / wait longer).
    OledDisp.setCursor(0, 6);
    if (!GPS_wiring_OK) {
      OledDisp.print("GPS no wire!     ");
    } else {
      OledDisp.print("GPS wait Sat:");
      OledDisp.print(g_sat);
      OledDisp.print("  ");
    }
  }
#ifdef ECLIPSE_MODE_ON
  // v2.3.14.1 — indicateur de progression du log éclipse.
  // Placé ICI, EN FIN DE DisplSqm() (appelée à chaque cycle du loop), pour
  // ne PAS être écrasé par les autres blocs d'affichage. Écrase la ligne 7
  // (Lon GPS) quand le mode est actif — perte acceptable car les infos GPS
  // essentielles (Alt, Sat, Lat) restent sur les lignes 5-6.
  {
    extern bool eclipse_isActive();
    extern uint32_t gEclipseLineCount;
    if (eclipse_isActive()) {
      OledDisp.setCursor(0, 7);
      OledDisp.print("ECL:");
      OledDisp.print(gEclipseLineCount);
      OledDisp.print("           ");  // trailing pad to clear leftovers
    }
  }
#endif
  _blk_change_status();
}

void DisplWait(char blk) {
  OledDisp.setContrast(ReadEEcontras());
  if (page != 4) {
    OledDisp.clear();
    buzzer(200);
  }
  page = 4;
  OledDisp.setCursor(0, 2);
  if (blk == '#')
    OledDisp.print("Wait first data");
  else
    OledDisp.print("Wait USB data  ");
  OledDisp.print(Blik ? blk : ' ');
  _blk_change_status();

  // -------------------------------------------------------------------------
  // Battery readout (calibrated, oversampled, time-cached since v2.3.12)
  // Format: "Bat: X.XXV (YY%)" or warning/critical icon if low.
  //
  // Time-throttled cache (BATTERY_DISPLAY_INTERVAL_MS, default 5 s) :
  // successive DisplWait() calls within the interval reuse the previous
  // reading -> stable OLED display, reduces ADC noise visibility on the
  // high-impedance 100k+100k divider.
  //
  // [BAT] serial trace emitted only on actual refresh (~every 5 s), in
  // WiFi mode only (USB mode reserves Serial for Unihedron protocol --
  // fixed in v2.2.9, propagated here).
  // -------------------------------------------------------------------------
  bool batRefreshed = batteryRefreshIfDue();
  float vbat   = getCachedBatteryVoltage();
  byte  pcent  = getCachedBatteryPercent();
  if (batRefreshed && digitalRead(ModePin)) {
    Serial.print("[BAT] raw_avg=");
    Serial.print(getCachedBatteryRawAvg(), 2);
    Serial.print("  V=");
    Serial.print(vbat, 3);
    Serial.print("  pct=");
    Serial.println(pcent);
  }

  OledDisp.setCursor(0, 4);
  if (vbat < BATTERY_LOW_THRESHOLD) {
    // Warning state: blink the marker and beep softly
    OledDisp.print("BAT LOW! ");
    OledDisp.print(String(vbat, 2));
    OledDisp.print("V ");
    OledDisp.print(Blik ? '!' : ' ');
    // Beep only in WiFi mode -- in USB/Unihedron mode the buzzer would add
    // timing jitter that disturbs the host (UDM) communication window.
    if (Blik && digitalRead(ModePin)) buzzer(50);
  } else {
    OledDisp.print("Bat: ");
    OledDisp.print(String(vbat, 2));
    OledDisp.print("V (");
    if (pcent < 10)      OledDisp.print("  ");
    else if (pcent < 100) OledDisp.print(' ');
    OledDisp.print(pcent);
    OledDisp.print("%)");
  }
#ifdef WIFI_ON
  OledDisp.setCursor(0, 6);
  if (WiFiConnected) {
    OledDisp.print(WiFi.localIP());
  } else {
    OledDisp.print("Not connect!    ");
  }
#endif
}
