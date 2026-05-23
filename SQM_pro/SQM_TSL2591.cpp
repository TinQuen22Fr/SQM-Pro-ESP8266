/**************************************************************************/
/*!
    @file     SQM_TSL2591.cpp
    @author   gshau

    Forked from original Adafruit TSL2591 libraries
    @author KT0WN (adafruit.com)
    @author wbphelps (wm@usa.net)
*/
/**************************************************************************/
#ifdef ESP8266
  #include <pgmspace.h>
#else
  #include <avr/pgmspace.h>
#endif

#if defined(__AVR__)
  #include <util/delay.h>
#endif
#include <stdlib.h>

#include "SQM_TSL2591.h"

// ---------------------------------------------------------------------------
// Optional dark-sky integration floor (v2.2.17)
// ---------------------------------------------------------------------------
// Pull the user-configurable minimum total integration time from Config.h
// (only when this header is reachable from the SQM_pro sketch context).
// In stand-alone use of this library the macro defaults to 0 (= disabled),
// preserving the original gshau / Adafruit behaviour for downstream users.
//
// Comparison with the genuine Unihedron SQM-LU (TSL237-based):
//   * SQM-LU uses a TSL237 frequency-output sensor with a typical 300 ms
//     pulse-counting window per reading.
//   * We use a TSL2591 ADC sensor with a native maximum integration time
//     of 600 ms per reading. With 10 accumulated readings (default 6 s
//     floor) we integrate ~20x more signal than the SQM-LU per cycle,
//     which improves the SNR in very dark skies. Calibration offset is
//     then applied to reach the same mpsas value (validated empirically
//     at 19.72 vs 19.77 mpsas on the same dark site).
// ---------------------------------------------------------------------------
#if __has_include("Config.h")
  #include "Config.h"
#endif
#ifndef TSL_MIN_TOTAL_INTEGRATION_MS
  #define TSL_MIN_TOTAL_INTEGRATION_MS  0UL
#endif


SQM_TSL2591::SQM_TSL2591(int32_t sensorID) {
  _initialized       = false;
  _integration       = TSL2591_INTEGRATIONTIME_400MS;
  _gain              = TSL2591_GAIN_LOW;
  _sensorID          = sensorID;
  _calibrationOffset = 0.;
}

boolean SQM_TSL2591::begin(void) {
  Wire.begin();

  uint8_t id = read8(0x12);
  if (id != 0x50) {
    return false;
  }

  _initialized = true;

  setTiming(_integration);
  setGain(_gain);
  setCalibrationOffset(_calibrationOffset);

  disable();

  verbose = true;
  return _initialized;
}

void SQM_TSL2591::enable(void) {
  if (!_initialized) {
    if (!begin()) return;
  }
  write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE,
         TSL2591_ENABLE_POWERON | TSL2591_ENABLE_AEN | TSL2591_ENABLE_AIEN);
}

void SQM_TSL2591::disable(void) {
  if (!_initialized) {
    if (!begin()) return;
  }
  write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE,
         TSL2591_ENABLE_POWEROFF);
}

void SQM_TSL2591::setTemperatureCalibration(const temperatureCalibration &calibrationData) {
  _temperatureCalibration = calibrationData;
}

void SQM_TSL2591::setTemperature(float temperature) {
  _hasTemperature = true;
  _temperature    = temperature;
}

void SQM_TSL2591::resetTemperature() {
  _hasTemperature = false;
}

void SQM_TSL2591::setGain(tsl2591Gain_t gain) {
  if (!_initialized) { if (!begin()) return; }

  enable();
  _gain = gain;
  write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL, _integration | _gain);
  disable();

  switch (_gain) {
    case TSL2591_GAIN_LOW:  gainValue = 1.0F;    break;
    case TSL2591_GAIN_MED:  gainValue = 25.0F;   break;
    case TSL2591_GAIN_HIGH: gainValue = 425.0F;  break;
    case TSL2591_GAIN_MAX:  gainValue = 9876.0F; break;
    default:
      if (verbose) Serial.println("Gain not found!");
      break;
  }
}

void SQM_TSL2591::setCalibrationOffset(float calibrationOffset) {
  _calibrationOffset = calibrationOffset;
}

tsl2591Gain_t SQM_TSL2591::getGain() { return _gain; }

void SQM_TSL2591::setTiming(tsl2591IntegrationTime_t integration) {
  if (!_initialized) { if (!begin()) return; }

  enable();
  _integration = integration;
  write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL, _integration | _gain);
  disable();

  switch (_integration) {
    case TSL2591_INTEGRATIONTIME_100MS: integrationValue = 100.F; break;
    case TSL2591_INTEGRATIONTIME_200MS: integrationValue = 200.F; break;
    case TSL2591_INTEGRATIONTIME_300MS: integrationValue = 300.F; break;
    case TSL2591_INTEGRATIONTIME_400MS: integrationValue = 400.F; break;
    case TSL2591_INTEGRATIONTIME_500MS: integrationValue = 500.F; break;
    case TSL2591_INTEGRATIONTIME_600MS: integrationValue = 600.F; break;
    default:
      integrationValue = 999.F;
      if (verbose) {
        Serial.println("Integration not found!");
        Serial.println(_integration);
      }
      break;
  }
}

tsl2591IntegrationTime_t SQM_TSL2591::getTiming() { return _integration; }

void SQM_TSL2591::configSensor() {
  setGain(config.gain);
  setTiming(config.time);
}

void SQM_TSL2591::showConfig() {
  Serial.print("Integration: ");
  Serial.print(integrationValue);
  Serial.println(" ms");
  Serial.print("Gain: ");
  Serial.print(gainValue);
  Serial.println("x");
}

uint32_t SQM_TSL2591::getFullLuminosity(void) {
  if (!_initialized) { if (!begin()) return 0; }

  enable();
  for (uint8_t d = 0; d <= _integration; d++) delay(120);

  uint32_t x;
  x  = read16(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CHAN1_LOW);
  x <<= 16;
  x |= read16(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CHAN0_LOW);

  disable();
  return x;
}

void SQM_TSL2591::bumpGain(int bumpDirection) {
  switch (config.gain) {
    case TSL2591_GAIN_LOW:
      config.gain = (bumpDirection > 0) ? TSL2591_GAIN_MED  : TSL2591_GAIN_LOW;  break;
    case TSL2591_GAIN_MED:
      config.gain = (bumpDirection > 0) ? TSL2591_GAIN_HIGH : TSL2591_GAIN_LOW;  break;
    case TSL2591_GAIN_HIGH:
      config.gain = (bumpDirection > 0) ? TSL2591_GAIN_MAX  : TSL2591_GAIN_MED;  break;
    case TSL2591_GAIN_MAX:
      config.gain = (bumpDirection > 0) ? TSL2591_GAIN_MAX  : TSL2591_GAIN_HIGH; break;
    default: break;
  }
  setGain(config.gain);
}

void SQM_TSL2591::bumpTime(int bumpDirection) {
  switch (config.time) {
    case TSL2591_INTEGRATIONTIME_200MS:
      config.time = (bumpDirection > 0) ? TSL2591_INTEGRATIONTIME_400MS : TSL2591_INTEGRATIONTIME_200MS; break;
    case TSL2591_INTEGRATIONTIME_400MS:
      config.time = (bumpDirection > 0) ? TSL2591_INTEGRATIONTIME_600MS : TSL2591_INTEGRATIONTIME_200MS; break;
    case TSL2591_INTEGRATIONTIME_600MS:
      config.time = (bumpDirection > 0) ? TSL2591_INTEGRATIONTIME_600MS : TSL2591_INTEGRATIONTIME_400MS; break;
    default: break;
  }
  setTiming(config.time);
}

void SQM_TSL2591::calibrateReadingsForTemperature(uint16_t &ir, uint16_t &full) {
  if (_hasTemperature) {
    if (verbose) {
      Serial.print("Values before temperature calibration: ir=");
      Serial.print(ir); Serial.print(", full="); Serial.println(full);
    }
    float irCalibrationFactor   = _temperature * _temperatureCalibration.irSlope
                                + _temperatureCalibration.irIntercept;
    float fullCalibrationFactor = _temperature * _temperatureCalibration.fullLuminositySlope
                                + _temperatureCalibration.fullLuminosityIntercept;
    ir   = static_cast<uint16_t>(static_cast<float>(ir)   * irCalibrationFactor);
    full = static_cast<uint16_t>(static_cast<float>(full) * fullCalibrationFactor);
    if (verbose) {
      Serial.print("Values after temperature calibration: ir=");
      Serial.print(ir); Serial.print(", full="); Serial.println(full);
    }
  }
}

void SQM_TSL2591::takeReading(void) {
  uint32_t lum;
  niter = 1;
  configSensor();
  lum  = getFullLuminosity();
  ir   = lum >> 16;
  full = lum & 0xFFFF;
  calibrateReadingsForTemperature(ir, full);
  vis  = full - ir;
  if ((float)full < (float)ir) {
    if (verbose) Serial.println("Odd, full less than ir! Rechecking...");
    takeReading();
  }
  // When intensity is faint at current gain setting
  if ((float)vis < 128.) {
    if (_gain == TSL2591_GAIN_MAX) {
      if (_integration != TSL2591_INTEGRATIONTIME_600MS) {
        if (verbose) Serial.println("Bumping integration up");
        bumpTime(1);
        if (verbose) showConfig();
        configSensor();
        lum = getFullLuminosity();
        delay(50);
        takeReading();
      } else {
        uint32_t fullCumulative;
        uint16_t visCumulative, irCumulative;

        fullCumulative = full;
        irCumulative   = ir;
        visCumulative  = vis;
        // ------------------------------------------------------------------
        // Dark-sky cumulative accumulation (v2.2.17)
        // ------------------------------------------------------------------
        // Original gshau loop stopped as soon as vis >= 128. For very dark
        // skies (Bortle 1-2, mpsas > 21) this can leave the SNR rather low.
        // Inspired by FreeDSM (UDC, GPL 3.0) which averages 6 samples (~6 s
        // total integration), we now keep accumulating until BOTH :
        //   (1) vis >= 128  (legacy signal-strength gate),  AND
        //   (2) niter * 600 ms >= TSL_MIN_TOTAL_INTEGRATION_MS
        //       (total cumulative integration floor, default 6 s).
        // niter is hard-capped at 32 to bound worst-case readout time
        // (32 * ~650 ms = ~21 s, still finite).
        // Setting TSL_MIN_TOTAL_INTEGRATION_MS = 0 in Config.h reverts to
        // the legacy behaviour (no floor).
        // ------------------------------------------------------------------
        while (true) {
          bool signalOK   = ((float)visCumulative >= 128.);
          bool floorOK    = (((uint32_t)niter * 600UL) >= (uint32_t)TSL_MIN_TOTAL_INTEGRATION_MS);
          if (signalOK && floorOK) break;
          if (niter >= 32) break;
          niter++;
          delay(50);
          lum  = getFullLuminosity();
          ir   = lum >> 16;
          full = lum & 0xFFFF;
          calibrateReadingsForTemperature(ir, full);
          fullCumulative += full;
          irCumulative   += ir;
          visCumulative   = fullCumulative - irCumulative;
        }
        if ((float)fullCumulative > (float)irCumulative) {
          full = fullCumulative;
          ir   = irCumulative;
          vis  = visCumulative;
        } else {
          if (verbose) Serial.println("Odd, full less than ir! Rechecking...");
          takeReading();
        }
      }
    } else {
      if (verbose) Serial.println("Bumping gain up");
      bumpGain(1);
      if (verbose) showConfig();
      configSensor();
      lum = getFullLuminosity();
      delay(50);
      takeReading();
    }
  }
  // If saturated, bump down integration or gain
  else if (full == 0xFFFF || ir == 0xFFFF) {
    if ((_gain == TSL2591_GAIN_MAX) &&
        (_integration != TSL2591_INTEGRATIONTIME_200MS)) {
      if (verbose) Serial.println("Bumping integration down");
      bumpTime(-1);
      if (verbose) showConfig();
      configSensor();
      lum = getFullLuminosity();
      delay(50);
      takeReading();
    } else {
      if (verbose) Serial.println("Bumping gain down");
      bumpGain(-1);
      if (verbose) showConfig();
      configSensor();
      lum = getFullLuminosity();
      delay(50);
      takeReading();
    }
  }

  float IR  = (float)ir  / (gainValue * integrationValue / 200.F * niter);
  float VIS = (float)vis / (gainValue * integrationValue / 200.F * niter);
  mpsas  = 12.6 - 1.086 * log(VIS) + _calibrationOffset;
  dmpsas = 1.086 / sqrt((float)vis);
}

uint8_t SQM_TSL2591::read8(uint8_t reg) {
  Wire.beginTransmission(TSL2591_ADDR);
  Wire.write(0x80 | 0x20 | reg);
  Wire.endTransmission();

  Wire.requestFrom(TSL2591_ADDR, 1);
  while (!Wire.available());
  return Wire.read();
}

uint16_t SQM_TSL2591::read16(uint8_t reg) {
  uint16_t x;
  uint16_t t;

  Wire.beginTransmission(TSL2591_ADDR);
#if ARDUINO >= 100
  Wire.write(reg);
#else
  Wire.send(reg);
#endif
  Wire.endTransmission();

  Wire.requestFrom(TSL2591_ADDR, 2);
#if ARDUINO >= 100
  t = Wire.read();
  x = Wire.read();
#else
  t = Wire.receive();
  x = Wire.receive();
#endif
  x <<= 8;
  x |= t;
  return x;
}

void SQM_TSL2591::write8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TSL2591_ADDR);
#if ARDUINO >= 100
  Wire.write(reg);
  Wire.write(value);
#else
  Wire.send(reg);
  Wire.send(value);
#endif
  Wire.endTransmission();
}

float SQM_TSL2591::calculateLux(uint16_t ch0, uint16_t ch1) /*wbp*/ {
  float atime, again;
  float cpl, lux1, lux2, lux;

  if ((ch0 == 0xFFFF) | (ch1 == 0xFFFF)) return 0.0; // overflow

  switch (_integration) {
    case TSL2591_INTEGRATIONTIME_100MS: atime = 100.0F; break;
    case TSL2591_INTEGRATIONTIME_200MS: atime = 200.0F; break;
    case TSL2591_INTEGRATIONTIME_300MS: atime = 300.0F; break;
    case TSL2591_INTEGRATIONTIME_400MS: atime = 400.0F; break;
    case TSL2591_INTEGRATIONTIME_500MS: atime = 500.0F; break;
    case TSL2591_INTEGRATIONTIME_600MS: atime = 600.0F; break;
    default: atime = 200.0F; break;
  }

  switch (_gain) {
    case TSL2591_GAIN_LOW:  again = 1.03F;   break; /*wbp*/
    case TSL2591_GAIN_MED:  again = 25.0F;   break;
    case TSL2591_GAIN_HIGH: again = 425.0F;  break; /*wbp*/
    case TSL2591_GAIN_MAX:  again = 7850.0F; break; /*wbp*/
    default: again = 1.0F; break;
  }

  cpl  = (atime * again) / TSL2591_LUX_DF;
  lux1 = ((float)ch0 - (TSL2591_LUX_COEFB * (float)ch1)) / cpl;
  lux2 = ((TSL2591_LUX_COEFC * (float)ch0) - (TSL2591_LUX_COEFD * (float)ch1)) / cpl;

  lux = lux1 > lux2 ? lux1 : lux2;
  return lux;
}

bool SQM_TSL2591::getEvent(sensors_event_t *event) {
  uint16_t ir, full;
  uint32_t lum = getFullLuminosity();
  lum   = getFullLuminosity(); // re-read (early-silicon workaround)
  ir    = lum >> 16;
  full  = lum & 0xFFFF;

  memset(event, 0, sizeof(sensors_event_t));
  event->version   = sizeof(sensors_event_t);
  event->sensor_id = _sensorID;
  event->type      = SENSOR_TYPE_LIGHT;
  event->timestamp = millis();
  event->light     = calculateLux(full, ir);
  return true;
}

void SQM_TSL2591::getSensor(sensor_t *sensor) {
  memset(sensor, 0, sizeof(sensor_t));
  strncpy(sensor->name, "TSL2591", sizeof(sensor->name) - 1);
  sensor->name[sizeof(sensor->name) - 1] = 0;
  sensor->version    = 1;
  sensor->sensor_id  = _sensorID;
  sensor->type       = SENSOR_TYPE_LIGHT;
  sensor->min_delay  = 0;
  sensor->max_value  = 88000.0;
  sensor->min_value  = 0.001;
  sensor->resolution = 0.001;
}
