// EEPROM.ino
// EEPROM helpers for SQM calibration & display settings
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

// EEPROM data positions
#define EEPROM_SQM_CAL_INDEX_C     1
#define EEPROM_SQM_CAL_INDEX_F     3
#define EEPROM_TEMP_CAL_INDEX_C    7
#define EEPROM_TEMP_CAL_INDEX_F    8
#define EEPROM_AUTO_TEMP_INDEX_C   14
#define EEPROM_AUTO_CONTRAS_INDEX_C 15
#define EEPROM_CONTRAS_INDEX_C     17
#define EEPROM_CONTRAS_INDEX_B     20

// -----------------------------------------------------------------------------
// SQM calibration offset
// -----------------------------------------------------------------------------
float ReadEESqmCalOffset() {
  float f;
  if (EEPROM.read(EEPROM_SQM_CAL_INDEX_C) == 'M') {
    f = EEPROM_readFloat(EEPROM_SQM_CAL_INDEX_F);
  } else {
    f = SQM_CAL_OFFSET;
  }
  return f;
}

void WriteEESqmCalOffset(float f) {
  if ((f > 25) || (f < -25)) return; // value out of range
  EEPROM.write(EEPROM_SQM_CAL_INDEX_C, 'M');
  EEPROM_writeFloat(EEPROM_SQM_CAL_INDEX_F, f);
}

// -----------------------------------------------------------------------------
// Temperature calibration offset
// -----------------------------------------------------------------------------
float ReadEETempCalOffset() {
  float f;
  if (EEPROM.read(EEPROM_TEMP_CAL_INDEX_C) == 't') {
    f = EEPROM_readFloat(EEPROM_TEMP_CAL_INDEX_F);
  } else {
    f = TEMP_CAL_OFFSET;
  }
  return f;
}

void WriteEETempCalOffset(float f) {
  if ((f > 50) || (f < -50)) return; // value out of range
  EEPROM.write(EEPROM_TEMP_CAL_INDEX_C, 't');
  EEPROM_writeFloat(EEPROM_TEMP_CAL_INDEX_F, f);
}

// -----------------------------------------------------------------------------
// Auto-contrast flag
// -----------------------------------------------------------------------------
boolean ReadEEAutoContras() {
  if (EEPROM.read(EEPROM_AUTO_CONTRAS_INDEX_C) == 'N')
    return false;
  else
    return true;
}

void WriteEEAutoContras(boolean _b) {
  if (_b)
    EEPROM.write(EEPROM_AUTO_CONTRAS_INDEX_C, 'Y');
  else
    EEPROM.write(EEPROM_AUTO_CONTRAS_INDEX_C, 'N');
}

// -----------------------------------------------------------------------------
// Auto temperature calibration flag
// -----------------------------------------------------------------------------
boolean ReadEEAutoTempCal() {
  if (EEPROM.read(EEPROM_AUTO_TEMP_INDEX_C) == 'Y')
    return true;
  else
    return false;
}

void WriteEEAutoTempCal(boolean _b) {
  if (_b)
    EEPROM.write(EEPROM_AUTO_TEMP_INDEX_C, 'Y');
  else
    EEPROM.write(EEPROM_AUTO_TEMP_INDEX_C, 'N');
}

// -----------------------------------------------------------------------------
// Display contrast
// -----------------------------------------------------------------------------
uint8_t ReadEEcontras() {
  uint8_t _f;
  if (EEPROM.read(EEPROM_CONTRAS_INDEX_C) == 'C') {
    _f = EEPROM.read(EEPROM_CONTRAS_INDEX_B);
  } else {
    _f = DEFALUT_CONTRAS;
  }
  return _f;
}

void WriteEEScontras(uint8_t _f) {
  EEPROM.write(EEPROM_CONTRAS_INDEX_C, 'C');
  EEPROM.write(EEPROM_CONTRAS_INDEX_B, _f);
}

// -----------------------------------------------------------------------------
// Low-level 4-byte helpers
// -----------------------------------------------------------------------------
void EEPROM_writeQuad(byte i, byte *v) {
  EEPROM.write(i + 0, *v); v++;
  EEPROM.write(i + 1, *v); v++;
  EEPROM.write(i + 2, *v); v++;
  EEPROM.write(i + 3, *v);
}

void EEPROM_readQuad(int i, byte *v) {
  *v = EEPROM.read(i + 0); v++;
  *v = EEPROM.read(i + 1); v++;
  *v = EEPROM.read(i + 2); v++;
  *v = EEPROM.read(i + 3);
}

void EEPROM_writeFloat(byte i, float f) {
  EEPROM_writeQuad(i, (byte*)&f);
}

float EEPROM_readFloat(byte i) {
  float f;
  EEPROM_readQuad(i, (byte*)&f);
  return f;
}
