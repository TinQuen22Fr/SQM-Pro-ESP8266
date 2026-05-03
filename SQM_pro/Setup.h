// Setup.h
//
// Copyright (c) 2025 Quentin Dumont
//
// Hardware specific configuration
//
#ifndef SETUP_H
#define SETUP_H

#define ModePin   2   // Mode Pin (USB <-> WiFi/OLED mode selector)
#define BuzzerPin 12  // Buzzer Pin

// -----------------------------------------------------------------------------
// BME280 weather sensor (I2C)
// -----------------------------------------------------------------------------
//  5V ------ CSB (enables the I2C interface)
//  GND ----- SDO (I2C Address 0x76)
//  5V ------ SDO (I2C Address 0x77)
#define BME_I2C_ADDRESS 0x76 // Default I2C address for BME280 sensor

// -----------------------------------------------------------------------------
// OLED font
// -----------------------------------------------------------------------------
//#define OLED_FONT u8x8_font_7x14B_1x2_f
//#define OLED_FONT u8x8_font_8x13_1x2_f
#define OLED_FONT u8x8_font_amstrad_cpc_extended_f

#endif // SETUP_H
