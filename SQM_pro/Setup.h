// Setup.h
//
// Copyright (c) 2025 Quentin Dumont
//
// Hardware specific configuration (NodeMCU v3 LoLin / ESP-12E)
//
// Pin mapping for the SQM-HR PCB:
//
//   ┌────────────┬────────────┬──────┬────────────────────────────────────┐
//   │ NodeMCU    │ GPIO       │ Use  │ Notes                              │
//   ├────────────┼────────────┼──────┼────────────────────────────────────┤
//   │ D1         │ GPIO5      │ SCL  │ I²C bus (TSL2591 + BME280 + OLED)  │
//   │ D2         │ GPIO4      │ SDA  │ I²C bus                            │
//   │ D3         │ GPIO0      │ SW   │ Front switch "flash" side: when    │
//   │            │            │      │ tied to GND at power-up, ESP boots │
//   │            │            │      │ in flash mode (USB firmware upload)│
//   │            │            │      │ NOT read by firmware code.         │
//   │ D4         │ GPIO2      │ GPS  │ GPS NEO-6 TXD -> NodeMCU RX        │
//   │            │            │      │ (also on-board blue LED + Serial1  │
//   │            │            │      │  boot TX, see note below)          │
//   │ D5         │ GPIO14     │ SW   │ Front switch "USB" side: when tied │
//   │            │            │      │ to GND, firmware switches to       │
//   │            │            │      │ Unihedron USB serial mode.         │
//   │ D6         │ GPIO12     │ BUZZ │ Buzzer + (active HIGH)             │
//   │ D7         │ GPIO13     │ GPS  │ NodeMCU TX -> GPS NEO-6 RXD        │
//   │ D0         │ GPIO16     │ -    │ Reserved: tie to RST for deep-sleep│
//   │            │            │      │ wake-up (cf. DEPLOY.md §11)        │
//   │ A0         │ ADC        │ BAT  │ Battery voltage (×11 divider)      │
//   └────────────┴────────────┴──────┴────────────────────────────────────┘
//
// Note on GPIO2 (D4): this pin is also tied to the on-board blue LED and is
// used by the ESP8266 as Serial1 TX during the boot diagnostic (74880 baud,
// first ~200 ms after reset). After boot, SoftwareSerial reconfigures D4 as
// input and the GPS NMEA stream is received normally. A few NMEA frames may
// be lost during boot but the GPS keeps emitting them every second so this
// is harmless. If you want to fully eliminate this contention, move GPS TXD
// to a free pin like D8 (GPIO15) and update gpsSerial(...) accordingly.
//
#ifndef SETUP_H
#define SETUP_H

// -----------------------------------------------------------------------------
// Front-panel switch (SQM-HR PCB)
// -----------------------------------------------------------------------------
// The switch is a 3-position center-off SPDT:
//   - Center (default): both GPIO14 and GPIO0 are HIGH (internal pull-ups) →
//                       firmware runs in normal Wi-Fi push mode.
//   - Switched to D5:   GPIO14 is pulled LOW → firmware enters Unihedron USB
//                       serial mode (commands i, r, u, w, g, z..., A5...).
//   - Switched to D3:   GPIO0 is pulled LOW. If the device is rebooted in
//                       this position, the ESP8266 enters flash download
//                       mode (USB firmware update). Otherwise no effect.
#define ModePin   14   // D5 / GPIO14 - read by firmware as USB-mode toggle

// -----------------------------------------------------------------------------
// Buzzer
// -----------------------------------------------------------------------------
#define BuzzerPin 12   // D6 / GPIO12 - active HIGH

// -----------------------------------------------------------------------------
// BME280 weather sensor (I²C)
// -----------------------------------------------------------------------------
//  3V3 ----- VIN
//  GND ----- GND
//  D1  ----- SCL  (GPIO5,  hardware I²C)
//  D2  ----- SDA  (GPIO4,  hardware I²C)
//  3V3 ----- CSB  (enables the I²C interface)
//  GND ----- SDO  (selects address 0x76)
//  3V3 ----- SDO  (selects address 0x77)
#define BME_I2C_ADDRESS 0x76

// -----------------------------------------------------------------------------
// OLED font (U8x8)
// -----------------------------------------------------------------------------
//#define OLED_FONT u8x8_font_7x14B_1x2_f
//#define OLED_FONT u8x8_font_8x13_1x2_f
//#define OLED_FONT u8x8_font_amstrad_cpc_extended_f   // v<=2.3.9 (gras)
#define OLED_FONT u8x8_font_pxplusibmcga_f              // v2.3.10 : IBM CGA médium (moins gras)

// -----------------------------------------------------------------------------
// Cross-file extern declarations
// -----------------------------------------------------------------------------
// arduino-cli concatenates *.ino files but emits function prototypes only,
// NOT variable extern declarations. Variables defined in GPS.ino must be
// declared extern here so SQM_pro.ino can see them (used in the `g0x`
// handler since v2.2.13). Without this, arduino-cli refuses to compile
// even though the Arduino IDE GUI sometimes accepts it (different
// preprocessor logic between the two tools).
//
#ifdef GPS_ON
extern bool   GPS_sync;
extern bool   GPS_wiring_OK;
extern byte   g_sat;
extern float  g_alt;
extern int    g_year;
extern byte   g_month;
extern byte   g_day;
extern byte   g_hour;
extern byte   g_minute;
extern byte   g_second;
extern double g_lat;
extern double g_lng;
#endif

#endif // SETUP_H
