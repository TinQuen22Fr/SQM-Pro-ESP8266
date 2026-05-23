/*
  SQM_pro.ino  Sky Quality Meter

  Copyright (c) 2025 Quentin Dumont

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.

  Description:
    Sky Quality Meter using the TSL2591
      base on https://github.com/gshau/SQM_TSL2591/
    and BME280 weather sensor
    and 128x64 OLED I2C display 0.96" (SSD1306) or 1.3" (SH1106)
    and NEO-6 GPS module (optional)
    and ESP8266 (NodeMCU) Wi-Fi uplink to the SQM Nightwatch backend
    at https://sqm.quentin-astro.fr (endpoint /api/sqm_push)

    Wiring diagram / PCB: https://easyeda.com/hujer.roman/sqm-hr
*/
#define Version       "2.3.13"
#define SERIAL_NUMBER "20200604"

#include "Config.h"
#include "Setup.h"
#include "Validate.h"

#include <Wire.h>
#ifdef WIFI_ON
  #include <ESP8266WiFi.h>
#endif
#include <BMx280I2C.h>
#include <U8x8lib.h>
#include <EEPROM.h>
#include "SQM_TSL2591.h"
#ifdef GPS_ON
  #include <SoftwareSerial.h>
  #include "TinyGPS++.h"
  TinyGPSPlus gps;
  // SoftwareSerial gpsSerial(rxPin, txPin):
  //   rxPin = GPIO2 (D4) - NodeMCU RX,  receives NMEA from GPS NEO-6 TXD
  //   txPin = GPIO13 (D7) - NodeMCU TX, sends commands to GPS NEO-6 RXD
  SoftwareSerial gpsSerial(2, 13);
#endif
#ifdef OTA_ON
  #include <ArduinoOTA.h>
#endif

// -----------------------------------------------------------------------------
// OLED display
// -----------------------------------------------------------------------------
#ifdef SH1106_ON
  U8X8_SH1106_128X64_NONAME_HW_I2C OledDisp(/* reset=*/ U8X8_PIN_NONE);
#endif
#ifdef SSD1306_ON
  U8X8_SSD1306_128X64_NONAME_HW_I2C OledDisp(/* reset=*/ U8X8_PIN_NONE);
#endif

// -----------------------------------------------------------------------------
// Sensors
// -----------------------------------------------------------------------------
BMx280I2C   bme(BME_I2C_ADDRESS);
SQM_TSL2591 sqm = SQM_TSL2591(2591);

float SqmCalOffset  = SQM_CAL_OFFSET;  // from EEPROM if present
float TempCalOffset = TEMP_CAL_OFFSET; // from EEPROM if present
float HumCalOffset  = 0.0f;            // v2.3.4 - from EEPROM if present
float PresCalOffset = 0.0f;            // v2.3.4 - from EEPROM if present (Pa)

boolean InitError      = false;
boolean USBmodeON      = false;
boolean SerialOK       = false;
boolean Blik           = false;
boolean Humidity       = true;
boolean WiFiConnected  = false;
byte    SQWcount       = 0;

float   temp    = 0;
float   hum     = 0;
float   pres    = 0;
uint16_t counter = 0;
byte    page    = 0;
String  BME_Msg;
String  TSL_Msg;
char    oled[6] = "A5,11";

// =============================================================================
// Setup
// =============================================================================
void setup() {
  Wire.begin();
  pinMode(ModePin,   INPUT_PULLUP);
  pinMode(BuzzerPin, OUTPUT);
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(1000);
#ifdef DEBUG_ON
  delay(5000);
  Serial.println("Ready");
#endif

  // v2.3.0 : EEPROM agrandie pour le nom de station personnalisé.
  // EEPROM.begin() initialise le buffer interne ; les commit() effectifs
  // sont faits ponctuellement (cf. WiFiPortal.ino après config utilisateur).
  EEPROM.begin(EEPROM_SIZE);

#ifdef GPS_ON
  gpsSerial.begin(GPSBaud);
#endif

  // BME280
  if (bme.begin()) {
    if (bme.isBME280()) {
      Humidity = true;
      BME_Msg  = "BME280 OK";
      bme.resetToDefaults();
      bme.writeOversamplingPressure   (BMx280MI::OSRS_P_x16);
      bme.writeOversamplingTemperature(BMx280MI::OSRS_T_x16);
      bme.writeOversamplingHumidity   (BMx280MI::OSRS_H_x16);
    } else {
      Humidity = false;
      BME_Msg  = "no Humidity";
    }
  } else {
    BME_Msg   = "BME Err";
    InitError = true;
  }
  Serial.println(BME_Msg);

  // TSL2591
  if (sqm.begin()) {
#ifdef SH1106_ON
    TSL_Msg = "TSL2591 OK";
#endif
#ifdef SSD1306_ON
    TSL_Msg = "TSL";
#endif
    sqm.verbose      = false;
    sqm.config.gain  = TSL2591_GAIN_LOW;
    sqm.config.time  = TSL2591_INTEGRATIONTIME_200MS;
    sqm.configSensor();
#ifdef DEBUG_ON
    sqm.showConfig();
#endif
  } else {
    TSL_Msg   = "TSL2591 Err";
    InitError = true;
  }
  Serial.println(TSL_Msg);

  // OLED
  if (OledDisp.begin()) {
    OledDisp.setFont(OLED_FONT);
    OledDisp.setPowerSave(false);
    oled[3] = '1';
    oled[4] = (ReadEEAutoContras()) ? '1' : '0';
    DisplFirstPage();
  } else {
    InitError = true;
    Serial.println("OLED Err");
  }

  if (InitError) {
    for (byte _i = 0; _i < 20; _i++) {
      buzzer(50);
      delay(50);
    }
    while (true);
  }

  delay(750);

  SqmCalOffset  = ReadEESqmCalOffset();
  TempCalOffset = ReadEETempCalOffset();
  HumCalOffset  = ReadEEHumCalOffset();   // v2.3.4
  PresCalOffset = ReadEEPresCalOffset();  // v2.3.4
  sqm.setCalibrationOffset(SqmCalOffset);

  DisplCalData();
  delay(1500);

#ifdef WIFI_ON
  // v2.3.0 : portail captif WiFiManager + Double Reset Detection.
  // Au 1er boot ou après un double power-cycle, ouvre un AP `SQM-Setup-XXXXXX`
  // pour saisir SSID/password WiFi + nom de la station. Bloquant pendant
  // toute la durée du portail (timeout 5 min).
  wifiPortal_setup();
  // wifi_setup() reste appelé pour le côté "watchdog reconnexion".
  wifi_setup();
#endif
#ifdef OTA_ON
  ota_setup();
#endif

  DisplWait('#');
} // end of setup()

// =============================================================================
// Main loop
// =============================================================================
void loop() {
  String response;

#ifdef WIFI_ON
  // v2.3.0 : maintient à jour le Double Reset Detector. Sans cet appel,
  // le flag DRD ne sera jamais effacé en RTC memory → un simple reboot
  // serait interprété comme un double reset au prochain boot.
  wifiPortal_loop();
#endif

#ifdef DEEP_SLEEP_ON
  // ---------------------------------------------------------------------------
  // Battery / deep-sleep mode: take ONE measurement, push, then sleep
  //   - GPS is given a few seconds to lock (best effort)
  //   - OLED is briefly used so the user sees something on power-up
  //   - USB / Unihedron mode is bypassed
  //   - OTA is unreachable in this mode
  // ---------------------------------------------------------------------------
  #ifdef WIFI_ON
    wifi_setup();
  #endif

  #ifdef GPS_ON
    // Try to get a GPS fix for up to 5 seconds (much shorter than a cold
    // start, just enough for warm starts). The push will go ahead anyway.
    for (uint8_t i = 0; i < 5; i++) {
      sqmGPS();
      if (GPS_sync) break;
    }
  #endif

  ReadWeather();
  if (ReadEEAutoTempCal()) sqm.setTemperature(temp);
  sqm.takeReading();
  DisplSqm(sqm.mpsas, sqm.dmpsas, temp, int(hum), int(pres / 100), ':');

  #ifdef WIFI_ON
    if (WiFiConnected) {
      wifi_main(sqm.mpsas, sqm.dmpsas, temp, hum, pres);
    }
  #endif

#ifdef DEBUG_WIFI_ON
  Serial.printf("Going to deep-sleep for %d seconds...\n", SLEEP_SEC);
  Serial.flush();
#endif
  // GPIO16 (D0) MUST be wired to RST for the chip to wake up.
  ESP.deepSleep((uint64_t)SLEEP_SEC * 1000000ULL);
  return; // unreachable: chip is reset on wake-up

#else
  // ---------------------------------------------------------------------------
  // Continuous mode: OLED, OTA, USB-mode toggle, periodic Wi-Fi push
  // ---------------------------------------------------------------------------
#ifdef OTA_ON
  ota_loop();
#endif
#ifdef GPS_ON
  sqmGPS();
#endif
#ifdef WIFI_ON
  wifi_setup();
#endif

  if (digitalRead(ModePin)) {
    SerialOK = false;
    if (USBmodeON) buzzer(200);
    USBmodeON = false;
  } else {
    if (!USBmodeON) buzzer(200);
    USBmodeON = true;
  }
#ifdef USB_MODE_OFF
  // Force normal mode on bare NodeMCU (GPIO2 is unreliable without the
  // SQM-HR PCB). The whole USB / Unihedron block below is bypassed.
  USBmodeON = false;
#endif

  if (!USBmodeON) {
    // -------------------------------------------------------------------------
    // Normal mode: OLED + WiFi upload
    // -------------------------------------------------------------------------
    OledDisp.setPowerSave(false);
    oled[3] = '1';
    ReadWeather();

    if (ReadEEAutoTempCal()) sqm.setTemperature(temp);

    sqm.takeReading();

    DisplSqm(sqm.mpsas, sqm.dmpsas, temp, int(hum), int(pres / 100), ':');

#ifdef WIFI_ON
    if (WiFiConnected) {
      // Push roughly every 5 loop iterations (~10s with the 2s delay below)
      if (SQWcount == 1) wifi_main(sqm.mpsas, sqm.dmpsas, temp, hum, pres);
      if (++SQWcount > 4) SQWcount = 0;
    } else {
      wifi_setup();
    }
    delay(2000);
#endif
  } else {
    // -------------------------------------------------------------------------
    // USB mode (Unihedron-style serial protocol)
    // -------------------------------------------------------------------------
    if (!SerialOK) {
      DisplWait('@');
      delay(50);
    }
    while (Serial.available()) {
      char _sign;
      SerialOK = true;

      if (digitalRead(ModePin)) break; // exit USB mode

      // ----------------------------------------------------------------------
      // Soft reset handling (v2.2.13): UDM may send a single byte 0x19 (EM,
      // End-of-Medium) to soft-reset the device (used by their response-time
      // tester before sending 50x U1x). We detect it BEFORE the command
      // parser because 0x19 has no 'x' terminator -> would otherwise block
      // readStringUntil for 1s.
      // ----------------------------------------------------------------------
      if (Serial.peek() == 0x19) {
        Serial.read();  // consume it
        delay(50);
        ESP.restart();
      }

      // ----------------------------------------------------------------------
      // PERF FIX (v2.2.6) : lire la commande EN PREMIER, avant toute lecture
      // capteur lente (TSL2591 takeReading peut bloquer jusqu'a ~1s en gain
      // eleve). UDM "Find" a un timeout court (~500ms) : si on lit les
      // capteurs avant de repondre a `ix`, UDM nous considere comme mort.
      // On ne lit les capteurs QUE si la commande recue en a besoin.
      // ----------------------------------------------------------------------
      String command = Serial.readStringUntil('x');

      // Determine si la commande necessite des donnees SQM (TSL2591 ~1s)
      bool needsSqmData = command.equals("r")
                       || command.equals("u")
                       || command.equals("U1")   // v2.2.13: stress-test variant of 'u'
                       || command.equals("w");

      // Determine si la commande necessite seulement la temperature BME280
      // (~100ms). v2.2.10: `cx` doit lire le BME280 sinon il renvoie 000.0C
      // a froid (avant tout `rx`/`ux`/`wx`).
      bool needsTempOnly = command.equals("c");

      String sqm_string  = "";
      String temp_string = "";
      String counter_string = "";
      String hz_string = "0000000000"; // SQM-LU equivalent Hz (computed below for r/u/U1)

      if (needsSqmData) {
        ReadWeather();
        if (ReadEEAutoTempCal()) sqm.setTemperature(temp);

        counter_string = String(counter++);
        while (counter_string.length() < 10) counter_string = '0' + counter_string;

        sqm.takeReading();
        sqm_string = String((sqm.mpsas < 0) ? -sqm.mpsas : sqm.mpsas, 2);
        while (sqm_string.length() < 5) sqm_string = '0' + sqm_string;
        _sign = (sqm.mpsas < 0) ? '-' : ' ';
        sqm_string = _sign + sqm_string;

        temp_string = String((temp < 0) ? -temp : temp, 1);
        while (temp_string.length() < 5) temp_string = '0' + temp_string;
        _sign = (temp < 0) ? '-' : ' ';
        temp_string = _sign + temp_string;

        // v2.2.14: compute SQM-LU equivalent Hz from current mpsas
        hz_string = sqmHzEquivalent(sqm.mpsas);
      } else if (needsTempOnly) {
        // BME280 lecture rapide (sans sqm.takeReading)
        ReadWeather();
        temp_string = String((temp < 0) ? -temp : temp, 1);
        while (temp_string.length() < 5) temp_string = '0' + temp_string;
        _sign = (temp < 0) ? '-' : ' ';
        temp_string = _sign + temp_string;
      }

      // Unit information request (note lower case "i")
      if (command.equals("i")) {
        Serial.print("i,00000002,00000003,00000001,");
        Serial.println(SERIAL_NUMBER);

      // ----------------------------------------------------------------------
      // UDM auto-detection sequence handlers (added in v2.2.7)
      // These commands are sent by UDM during `Find` and `GetVersion`; without
      // responses UDM times out and rejects the device.
      // Reference capture from a genuine SQM-LU on /dev/ttyUSB1 (v1962):
      //   ix   -> i,00000004,00000003,00000057,00001962
      //   m0x  -> m0,000
      //   m1x  -> m1,000
      //   m2x  -> m2,000
      //   Yx   -> Yrcpu
      //   Ix   -> 0000000000s,0000000000s,00000000.00m,00000000.00m
      // ----------------------------------------------------------------------

      // Report Interval Settings (CAPITAL I, different from lowercase ix)
      // Format: <period_s>s,<threshold_period_s>s,<threshold_mpsas_1>m,<threshold_mpsas_2>m
      // We have no report-interval feature on the DIY so we return zeros
      // (same as a freshly-reset SQM-LU).
      } else if (command.equals("I")) {
        Serial.println("0000000000s,0000000000s,00000000.00m,00000000.00m");

      // Manual parameter read commands (m0x, m1x, m2x)
      // On a real SQM-LU these return device-specific values; on a DIY we
      // echo the minimal format UDM expects so it continues its detection.
      } else if (command.equals("m0")) {
        Serial.println("m0,000");
      } else if (command.equals("m1")) {
        Serial.println("m1,000");
      } else if (command.equals("m2")) {
        Serial.println("m2,000");

      // ContCheck: advertises supported capabilities to UDM.
      // `Yrcpu` = reading + calibration + period + unaveraged support.
      } else if (command.equals("Y")) {
        Serial.println("Yrcpu");

      // Reading request
      } else if (command.equals("r")) {
        Serial.println("r," + sqm_string
                     + "m," + hz_string + "Hz,"
                     + counter_string
                     + "c,0000000.000s,"
                     + temp_string + 'C');

      // Unaveraged reading request
      } else if (command.equals("u")) {
        Serial.println("u," + sqm_string
                     + "m," + hz_string + "Hz,"
                     + counter_string
                     + "c,0000000.000s,"
                     + temp_string + 'C');

      // U1x: unaveraged reading variant used by UDM "response-time tester"
      // (sends U1x 50x in a row with 32ms sleep). Same format as 'u' reading.
      } else if (command.equals("U1")) {
        Serial.println("u," + sqm_string
                     + "m," + hz_string + "Hz,"
                     + counter_string
                     + "c,0000000.000s,"
                     + temp_string + 'C');

      // g0x: GPS position request (NMEA GGA-like format)
      // UDM parses this as comma-delimited:
      //   pieces[0] = title (ignored)
      //   pieces[1] = HHMMSS.sss UTC time
      //   pieces[2] = ddmm.mmmm latitude
      //   pieces[3] = N or S
      //   pieces[4] = dddmm.mmmm longitude
      //   pieces[5] = E or W
      //   pieces[6] = fix quality (0=invalid, 1=GPS SPS valid)
      //   pieces[7] = satellites used count
      // v2.2.13: this command is unique to our DIY -- the genuine SQM-LU does
      // NOT have GPS (except the DLS variant). UDM will display the position
      // in the GPS Response panel.
      } else if (command.equals("g0")) {
#ifdef GPS_ON
        char gpsBuf[120];
        double lat_abs = (g_lat < 0) ? -g_lat : g_lat;
        int    lat_deg = (int)lat_abs;
        float  lat_min = (float)(lat_abs - lat_deg) * 60.0f;
        char   lat_ns  = (g_lat < 0) ? 'S' : 'N';
        double lng_abs = (g_lng < 0) ? -g_lng : g_lng;
        int    lng_deg = (int)lng_abs;
        float  lng_min = (float)(lng_abs - lng_deg) * 60.0f;
        char   lng_ew  = (g_lng < 0) ? 'W' : 'E';
        byte   fix     = GPS_sync ? 1 : 0;
        snprintf(gpsBuf, sizeof(gpsBuf),
                 "GGA,%02d%02d%02d.000,%02d%07.4f,%c,%03d%07.4f,%c,%d,%02d,",
                 g_hour, g_minute, g_second,
                 lat_deg, lat_min, lat_ns,
                 lng_deg, lng_min, lng_ew,
                 (int)fix, (int)g_sat);
        Serial.println(gpsBuf);
#else
        // GPS not compiled in: return empty GGA so UDM doesn't time out
        Serial.println("GGA,000000.000,0000.0000,N,00000.0000,E,0,00,");
#endif

#ifdef EXTENDET_PROTOCOL_ON
      // Extension request for weather information
      } else if (command.equals("w")) {
        String ir_string  = String(sqm.ir);
        while (ir_string.length()  < 5) ir_string  = '0' + ir_string;
        String vis_string = String(sqm.vis);
        while (vis_string.length() < 5) vis_string = '0' + vis_string;

        String hum_string = String(int(hum));
        while (hum_string.length() < 3) hum_string = '0' + hum_string;
        String pres_string = String(int(pres / 100));
        while (pres_string.length() < 4) pres_string = '0' + pres_string;

        Serial.println("w," + sqm_string + "m,"
                     + String(sqm.dmpsas, 2) + "e,"
                     + ir_string  + "i,"
                     + vis_string + "v,"
                     + counter_string + "c,"
                     + oled + ','
                     + hum_string  + "h,"
                     + pres_string + "p,"
                     + temp_string + 'C');
#endif

      // Read config information request
      } else if (command.equals("g")) {
        sqm_string = String((SqmCalOffset < 0) ? -SqmCalOffset : SqmCalOffset, 2);
        while (sqm_string.length() < 6) sqm_string = '0' + sqm_string;
        _sign = (SqmCalOffset < 0) ? '-' : ' ';
        sqm_string = _sign + sqm_string;
        temp_string = String((TempCalOffset < 0) ? -TempCalOffset : TempCalOffset, 1);
        while (temp_string.length() < 5) temp_string = '0' + temp_string;
        _sign = (TempCalOffset < 0) ? '-' : ' ';
        temp_string = _sign + temp_string;
        oled[4] = (ReadEEAutoContras()) ? '1' : '0';
        Serial.print("g," + sqm_string + "m,"
                   + temp_string + "C,TC:"
                   + ((ReadEEAutoTempCal()) ? "Y," : "N,")
                   + oled
                   + ",DC:");
        Serial.println(ReadEEcontras());

      // Calibration information request (required by Unihedron UDM for device discovery)
      // Response format (matches genuine SQM-LU output observed via UDM logs):
      //   c,LLLLLLLL.LLm,SSSSSSS.SSSs, TTL.TC,DDDDDDDD.DDm, TTD.TC
      //   - Light calibration offset
      //   - Dark calibration time period (NOT a "light dark period")
      //   - Temperature recorded during light calibration
      //   - Dark calibration offset
      //   - Temperature recorded during dark calibration
      // Reference SQM-LU response: c,00000019.92m,0000300.000s, 019.9C,00000008.71m, 020.9C
      //
      // v2.2.10: each field is now configurable via Config.h DIY_* macros.
      // Note: these are COSMETIC ONLY for UDM display; live measurements
      // (rx/ux) always use the live SqmCalOffset / temp regardless.
      } else if (command.equals("c")) {
        // Light calibration offset: 0 in macro = use live SqmCalOffset
        float lightOff = (DIY_LIGHT_CAL_OFFSET != 0.0f)
                       ? DIY_LIGHT_CAL_OFFSET
                       : ((SqmCalOffset < 0) ? -SqmCalOffset : SqmCalOffset);
        String lightCal = String(lightOff, 2);
        while (lightCal.length() < 11) lightCal = '0' + lightCal;

        // Dark calibration time period (DIY has no dark sensor by default)
        char darkPeriodBuf[16];
        dtostrf(DIY_DARK_CAL_TIME_PERIOD, 11, 3, darkPeriodBuf);
        // dtostrf left-pads with spaces; replace leading spaces with zeros
        for (int i = 0; darkPeriodBuf[i] == ' '; i++) darkPeriodBuf[i] = '0';
        String darkPeriod = String(darkPeriodBuf);

        // Light calibration temperature
        String lightTempStr;
#ifdef DIY_LIGHT_CAL_TEMP_FROM_BME280
        lightTempStr = temp_string;  // built from live BME280 reading above
#else
        lightTempStr = String((DIY_LIGHT_CAL_TEMP < 0) ? -DIY_LIGHT_CAL_TEMP : DIY_LIGHT_CAL_TEMP, 1);
        while (lightTempStr.length() < 5) lightTempStr = '0' + lightTempStr;
        lightTempStr = ((DIY_LIGHT_CAL_TEMP < 0) ? '-' : ' ') + lightTempStr;
#endif

        // Dark calibration offset
        String darkCal = String(
          (DIY_DARK_CAL_OFFSET < 0) ? -DIY_DARK_CAL_OFFSET : DIY_DARK_CAL_OFFSET, 2);
        while (darkCal.length() < 11) darkCal = '0' + darkCal;

        // Dark calibration temperature
        String darkTempStr;
#ifdef DIY_DARK_CAL_TEMP_FROM_BME280
        darkTempStr = temp_string;  // same live BME280 reading
#else
        darkTempStr = String((DIY_DARK_CAL_TEMP < 0) ? -DIY_DARK_CAL_TEMP : DIY_DARK_CAL_TEMP, 1);
        while (darkTempStr.length() < 5) darkTempStr = '0' + darkTempStr;
        darkTempStr = ((DIY_DARK_CAL_TEMP < 0) ? '-' : ' ') + darkTempStr;
#endif

        Serial.println("c," + lightCal + "m," + darkPeriod + "s,"
                     + lightTempStr + "C,"
                     + darkCal + "m,"
                     + darkTempStr + "C");

      // Logging parameter readouts (A1x..A4x).
      // Real SQM-LU returns logging buffer pointers / mode flags. The DIY does
      // not implement on-device logging, but UDM expects valid responses to
      // enable certain UI buttons. We return values matching a freshly-reset
      // SQM-LU as observed in UDM logs.
      } else if (command.equals("A1")) {
        Serial.println("A,1,D,7,0,00128,08224");
      } else if (command.equals("A2") || command.equals("A2P")) {
        Serial.println("A,2,D,3,F,7,P");
      } else if (command.equals("A3") || command.equals("A31")) {
        Serial.println("A,3,D,0,1");
      } else if (command.equals("A4")) {
        Serial.println("A,4,0,7,31,-049,224,002");

      // Set Period / Set Threshold writes (UDM Report Interval tab).
      // We don't store these on the DIY (no logging feature), but we echo
      // the current "Ix" report so UDM doesn't stall waiting for a reply.
      } else if (command.length() >= 1 && command[0] == 'P') {
        Serial.println("0000000000s,0000000000s,00000000.00m,00000000.00m");
      } else if (command.length() >= 1 && command[0] == 'T') {
        Serial.println("0000000000s,0000000000s,00000000.00m,00000000.00m");

      // Configuration command
      } else if (command[0] == 'z') {
        response = command.substring(1, 4);

        if (response.equals("cal")) {
          char _x = command[4];
          if (_x == '1') { // SQM calibration offset
            response = command.substring(5);
            SqmCalOffset = response.toFloat();
            WriteEESqmCalOffset(SqmCalOffset);
            sqm_string = String((SqmCalOffset < 0) ? -SqmCalOffset : SqmCalOffset, 2);
            while (sqm_string.length() < 5) sqm_string = '0' + sqm_string;
            _sign = (SqmCalOffset < 0) ? '-' : ' ';
            sqm_string = _sign + sqm_string;
            Serial.println("z,1," + sqm_string + 'm');
          }
          else if (_x == '2') { // temperature calibration
            response = command.substring(5);
            TempCalOffset = response.toFloat();
            WriteEETempCalOffset(TempCalOffset);
            temp_string = String((TempCalOffset < 0) ? -TempCalOffset : TempCalOffset, 1);
            while (temp_string.length() < 5) temp_string = '0' + temp_string;
            _sign = (TempCalOffset < 0) ? '-' : ' ';
            temp_string = _sign + temp_string;
            Serial.println("z,2," + temp_string + 'C');
          }
          else if (_x == '3') { // display contrast
            response = command.substring(5);
            WriteEEScontras(response.toInt());
            Serial.println("z,3," + command.substring(5) + 'x');
          }
          else if (_x == 'e') { // enable temp calibration
            WriteEEAutoTempCal(true);
            Serial.println("zeaL");
          }
          else if (_x == 'd') { // disable temp calibration
            WriteEEAutoTempCal(false);
            sqm.resetTemperature();
            Serial.println("zdaL");
          }
          else if (_x == 'D') { // factory reset
            SqmCalOffset  = SQM_CAL_OFFSET;
            TempCalOffset = TEMP_CAL_OFFSET;
            WriteEETempCalOffset(TempCalOffset);
            WriteEESqmCalOffset(SqmCalOffset);
            WriteEEScontras(DEFALUT_CONTRAS);
            // Match genuine SQM-LU response (was "zxdL" in v2.2.7 -> wrong)
            Serial.println("zxdU");
          }
        }

      } else if (command.equals("A50")) {   // disable OLED
        oled[3] = '0';
        OledDisp.setPowerSave(true);
        Serial.println(oled);
      } else if (command.equals("A51")) {   // enable OLED
        oled[3] = '1';
        OledDisp.setPowerSave(false);
        Serial.println(oled);
      } else if (command.equals("A5d")) {   // disable auto-contrast
        oled[4] = '0';
        WriteEEAutoContras(false);
        Serial.println(oled);
      } else if (command.equals("A5e")) {   // enable auto-contrast
        oled[4] = '1';
        WriteEEAutoContras(true);
        Serial.println(oled);
      } else if (command.equals("A5")) {    // display status
        oled[4] = (ReadEEAutoContras()) ? '1' : '0';
        Serial.println(oled);
      }

      // Refresh current information on the OLED
      DisplSqm(sqm.mpsas, sqm.dmpsas, temp, int(hum), int(pres / 100), '.');
      SqmCalOffset  = ReadEESqmCalOffset();
      TempCalOffset = ReadEETempCalOffset();
      sqm.setCalibrationOffset(SqmCalOffset);
    } // end of while (Serial.available())
  } // end of if (digitalRead(ModePin))

  delay(5000);
#endif // !DEEP_SLEEP_ON
} // end of loop()
