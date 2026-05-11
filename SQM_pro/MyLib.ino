// MyLib.ino
// Library of helper functions for SQM
//
// Copyright (c) 2025 Quentin Dumont
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
    temp = temp + TempCalOffset;
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

// Linear estimation between BATTERY_VMIN (0%) and BATTERY_VMAX (100%).
// Note: 18650 discharge curve is NOT linear, but this is acceptable for a
// rough indication. A LiPo gauge IC (MAX17048 etc.) would be more accurate.
//
// v2.2.11: replaced naive linear interpolation by a piecewise approximation
// of a typical 1S LiPo discharge curve at light load (~50-200mA). The curve
// is much flatter between 3.7-4.0V (the "plateau") and drops rapidly below
// 3.6V (the "knee"). The previous linear model gave 82% at 3.99V which was
// technically correct but counter-intuitive (a freshly-charged LiPo is at
// 4.20V, not 3.99V).
//
// Curve points (V, %): based on combined Adafruit/Sparkfun/Maxim datasheets
//   4.20 -> 100%
//   4.10 ->  90%
//   4.00 ->  80%
//   3.90 ->  70%
//   3.80 ->  55%
//   3.70 ->  35%
//   3.60 ->  20%
//   3.50 ->  10%
//   3.40 ->   5%
//   3.30 ->   2%
//   3.00 ->   0%
byte getBatteryPercent(float v) {
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
  OledDisp.print("SQM Ready V");
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

void DisplSqm(double mpsas, double dmpsas, int temp, byte hum, int pres, char blk) {
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
  // Small indicator at the top-right showing whether the cloud push is
  // active (NIGHT) or skipped (DAY) based on the current magnitude.
  OledDisp.setCursor(13, 0);
  if (mpsas < NIGHT_THRESHOLD_MPSAS) {
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
  OledDisp.print(char(0xb2));
  if (((temp < 0) ? -temp : temp) < 10) OledDisp.print(' ');
  if (temp >= 0) OledDisp.print(' ');
  OledDisp.print(temp);
  OledDisp.print(char(0xb0));
  OledDisp.print('C');
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
  // Battery readout (calibrated, oversampled)
  // Format: "Bat: X.XXV (YY%)" or warning/critical icon if low.
  // Raw ADC value is printed on Serial ONLY in WiFi mode (when ModePin is
  // HIGH). In USB/Unihedron mode the Serial line is reserved for the protocol
  // (any debug print would corrupt UDM responses -- caused total handshake
  // failure in v2.2.8, fixed in v2.2.9).
  // -------------------------------------------------------------------------
  float vbat   = readBatteryVoltage();
  byte  pcent  = getBatteryPercentSmoothed(vbat);
  if (digitalRead(ModePin)) {  // HIGH = WiFi/normal mode -> debug allowed
    float rawAvg = readBatteryRawAvg();
    Serial.print("[BAT] raw_avg=");
    Serial.print(rawAvg, 2);
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
