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
byte getBatteryPercent(float v) {
  if (v >= BATTERY_VMAX) return 100;
  if (v <= BATTERY_VMIN) return 0;
  return (byte)((v - BATTERY_VMIN) / (BATTERY_VMAX - BATTERY_VMIN) * 100.0f + 0.5f);
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
  byte  pcent  = getBatteryPercent(vbat);
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
