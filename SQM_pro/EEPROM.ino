// EEPROM.ino
// EEPROM helpers for SQM calibration & display settings
//
// Copyright (c) 2025 Quentin Dumont
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

// v2.3.0 - Nom de station personnalisé (saisi via portail captif)
// Slot de 33 octets (1 marker 'N' + 32 chars + null implicite)
#define EEPROM_STATION_NAME_INDEX_C 30
#define EEPROM_STATION_NAME_INDEX_S 31
#define EEPROM_STATION_NAME_MAX     32

// Note : EEPROM_SIZE est défini dans Config.h pour être visible dans tous
// les .ino, indépendamment de l'ordre de concaténation arduino-cli.

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

// -----------------------------------------------------------------------------
// v2.3.0 - Nom de station personnalisé (portail captif)
// -----------------------------------------------------------------------------
// Lit le nom de station depuis l'EEPROM. Si absent (premier boot ou EEPROM
// vierge), `out[0]` est mis à 0 et l'appelant peut générer un nom par défaut.
void ReadEEStationName(char *out, size_t outSize) {
  if (outSize == 0) return;
  out[0] = 0;
  if (EEPROM.read(EEPROM_STATION_NAME_INDEX_C) != 'N') {
    return;  // pas de nom persisté
  }
  size_t maxRead = outSize - 1;
  if (maxRead > EEPROM_STATION_NAME_MAX) maxRead = EEPROM_STATION_NAME_MAX;
  size_t i = 0;
  for (; i < maxRead; i++) {
    char c = (char)EEPROM.read(EEPROM_STATION_NAME_INDEX_S + i);
    if (c == 0 || (uint8_t)c == 0xFF) break;
    // Sanity : on n'autorise que les chars imprimables ASCII pour éviter
    // d'envoyer un ID corrompu au backend.
    if (c < 0x20 || c > 0x7E) { i = 0; break; }
    out[i] = c;
  }
  out[i] = 0;
}

// Écrit le nom de station dans l'EEPROM. Appel suivi obligatoirement par
// EEPROM.commit() côté appelant (déjà fait dans wifiPortal_setup()).
void WriteEEStationName(const char *name) {
  if (!name) return;
  EEPROM.write(EEPROM_STATION_NAME_INDEX_C, 'N');
  size_t i = 0;
  for (; i < EEPROM_STATION_NAME_MAX && name[i] != 0; i++) {
    EEPROM.write(EEPROM_STATION_NAME_INDEX_S + i, (uint8_t)name[i]);
  }
  // null terminator
  if (i < EEPROM_STATION_NAME_MAX) {
    EEPROM.write(EEPROM_STATION_NAME_INDEX_S + i, 0);
  }
}
