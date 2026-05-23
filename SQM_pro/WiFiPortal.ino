// WiFiPortal.ino
// Portail captif d'auto-configuration WiFi pour les sondes SQM Pro DIY.
//
// Copyright (c) 2026 Quentin Dumont
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
// v2.3.0 :
//   Au premier boot (ou si l'EEPROM ne contient pas encore de credentials WiFi),
//   le firmware ouvre un point d'accès `SQM-Setup-XXXXXX` (où XXXXXX = 6 derniers
//   hex du chip ID ESP8266). L'utilisateur s'y connecte depuis son téléphone,
//   un portail captif s'ouvre automatiquement, et il saisit :
//     - le SSID de son WiFi domestique
//     - le mot de passe associé
//     - un nom personnalisé pour sa station
//
// v2.3.1 :
//   Ajout d'un WiFi de secours (facultatif) saisi via deux champs supplémentaires
//   du portail captif. La logique de boot devient :
//     1. Tentative connexion WiFi primaire (credentials persistés en flash)
//     2. Si échec : tentative WiFi secondaire (depuis EEPROM, si configuré)
//     3. Si échec : portail captif `SQM-Setup-XXXXXX`
//   Le WiFi secondaire est utile par exemple pour une sonde déplaçable
//   (résidence principale ⇄ résidence secondaire) ou en cas de coupure box
//   (smartphone partagé en hotspot temporaire).
//
// Double Reset (DRD) : pour FORCER une reconfiguration sans accès physique au
// boîtier (boîtier scellé / installé en extérieur) :
//   1. Couper le courant de la sonde
//   2. Rallumer  (le firmware démarre, attend 5 s)
//   3. Re-couper le courant DANS LES 5 SECONDES qui suivent
//   4. Rallumer → le firmware détecte le double reset et relance le portail
//
// Bibliothèques utilisées (à ajouter dans la CI) :
//   - WiFiManager           (tzapu, v2.0.16+)
//   - ESP_DoubleResetDetector (khoih-prog, v1.3.2+)
//
#ifdef WIFI_ON

#include <ESP8266WiFi.h>

#define ESP_DRD_USE_LITTLEFS    false
#define ESP_DRD_USE_SPIFFS      false
#define ESP_DRD_USE_EEPROM      false
#define ESP8266_DRD_USE_RTC     true
#define DOUBLERESETDETECTOR_DEBUG false
#include <ESP_DoubleResetDetector.h>

#define WM_NODEBUG
#include <WiFiManager.h>

// -----------------------------------------------------------------------------
// Paramètres
// -----------------------------------------------------------------------------
#define DRD_TIMEOUT 5
#define DRD_ADDRESS 0
#define WIFI_PORTAL_TIMEOUT     300   // 5 min max sur le portail
#define WIFI_CONNECT_TIMEOUT_MS 15000 // 15 s max par tentative (primaire ou alt)

// -----------------------------------------------------------------------------
// État global
// -----------------------------------------------------------------------------
DoubleResetDetector* drd = nullptr;
char gStationName[33] = "";
bool gWifiPortalOk = false;
// v2.3.3 — toggle runtime "push uniquement la nuit".
// Initialisé au défaut compile-time (Config.h #define NIGHT_ONLY_PUSH_ON).
// Surchargé par la valeur EEPROM si elle existe (cf. wifiPortal_setup).
#ifdef NIGHT_ONLY_PUSH_ON
bool gNightOnlyPush = true;
#else
bool gNightOnlyPush = false;
#endif

// -----------------------------------------------------------------------------
// Helpers internes
// -----------------------------------------------------------------------------
static void buildDefaultStationName(char* out, size_t outSize) {
  snprintf(out, outSize, "SQM-%06X", ESP.getChipId() & 0xFFFFFF);
}

// Bloque jusqu'à ce que WiFi.status() == WL_CONNECTED, ou que `timeoutMs`
// soit écoulé. Retourne true si connecté.
static bool waitForWifi(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

// Tente une connexion au WiFi de secours sans toucher aux credentials persistés
// en flash (WiFi.persistent(false)). Ainsi le primaire reste celui sauvé par
// WiFiManager même si on tombe en mode "alt" pour cette session.
static bool tryAltWifi(const char* altSsid, const char* altPass) {
  if (!altSsid || altSsid[0] == 0) return false;
  Serial.print(F("[WiFi] Trying backup SSID: "));
  Serial.println(altSsid);
  WiFi.persistent(false);
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(altSsid, altPass ? altPass : "");
  bool ok = waitForWifi(WIFI_CONNECT_TIMEOUT_MS);
  WiFi.persistent(true);  // restaure le comportement par défaut
  return ok;
}

// Tente une reconnexion au WiFi primaire (credentials persistés en flash).
static bool tryPrimaryWifi() {
  Serial.println(F("[WiFi] Trying primary saved credentials..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin();  // utilise les derniers SSID/password sauvés en flash
  return waitForWifi(WIFI_CONNECT_TIMEOUT_MS);
}

// Ouvre le portail captif WiFiManager avec les 3 champs custom et persiste
// en EEPROM ce que l'utilisateur a saisi. Bloquant.
static bool runConfigPortal(char* stationName, size_t stationNameSize,
                            char* altSsid, size_t altSsidSize,
                            char* altPass, size_t altPassSize) {
  char apName[32];
  snprintf(apName, sizeof(apName), "SQM-Setup-%06X",
           ESP.getChipId() & 0xFFFFFF);

  WiFiManager wm;
#ifdef WM_NODEBUG
  wm.setDebugOutput(false);
#endif

  WiFiManagerParameter customStationName(
    "station_name",
    "Nom de la station (ex: SQM-Maison)",
    stationName, 32
  );
  WiFiManagerParameter customAltSsid(
    "alt_ssid",
    "WiFi de secours - SSID (facultatif)",
    altSsid, 32
  );
  // Le mot de passe n'est jamais pré-rempli (sécurité : on ne l'expose pas)
  WiFiManagerParameter customAltPass(
    "alt_pass",
    "WiFi de secours - mot de passe",
    "", 64,
    " type=\"password\""
  );
  // v2.3.3 — toggle "Push uniquement la nuit"
  // Implémenté comme un input texte "0" / "1" (WiFiManager n'a pas de
  // widget checkbox natif). Label explicite pour ne pas perdre le user.
  char nightOnlyDefault[2];
  nightOnlyDefault[0] = gNightOnlyPush ? '1' : '0';
  nightOnlyDefault[1] = '\0';
  WiFiManagerParameter customNightOnly(
    "night_only",
    "Push nuit seulement ? (1=oui, 0=mode test)",
    nightOnlyDefault, 2
  );

  // v2.3.4 — Offsets calibration BME280 (Temp / Humidité / Pression).
  // Sub/sur-estimation due à la chauffe du boîtier (typique +0.5°C / -5% RH).
  // Saisie en float avec signe, ex: "-0.6", "+6.0", "0.0".
  char tempOffsetBuf[10], humOffsetBuf[10], presOffsetBuf[10];
  dtostrf(TempCalOffset, 0, 2, tempOffsetBuf);
  dtostrf(HumCalOffset,  0, 2, humOffsetBuf);
  dtostrf(PresCalOffset, 0, 2, presOffsetBuf);
  WiFiManagerParameter customTempOffset(
    "temp_offset",
    "BME280 Offset Temperature (degC, ex -0.6)",
    tempOffsetBuf, 8
  );
  WiFiManagerParameter customHumOffset(
    "hum_offset",
    "BME280 Offset Humidite (%, ex +6.0)",
    humOffsetBuf, 8
  );
  WiFiManagerParameter customPresOffset(
    "pres_offset",
    "BME280 Offset Pression (Pa, ex 0.0)",
    presOffsetBuf, 8
  );

  // v2.3.5 — Offset de calibration SQM (mpsas).
  // Ajusté en comparant avec un SQM-L Unihedron officiel (reference). La sonde
  // DIY (TSL2591 sans lentille) capture un FOV plus large (~70°) que le SQM-L
  // (~20°), ce qui sur-estime la pollution lumineuse latérale et donne une
  // magnitude artificiellement plus basse. L'offset compense ce biais.
  // Convention : offset POSITIF si DIY < officiel (= DIY mesure trop clair).
  // Plage acceptee : -10.0 a +10.0 mpsas (verifie dans WriteEESqmCalOffset).
  // Live-update : applique immediatement sans reboot apres save.
  char sqmOffsetBuf[10];
  dtostrf(SqmCalOffset, 0, 2, sqmOffsetBuf);
  WiFiManagerParameter customSqmOffset(
    "sqm_offset",
    "SQM Cal Offset (mpsas, ex +2.25)",
    sqmOffsetBuf, 8
  );

  wm.addParameter(&customStationName);
  wm.addParameter(&customAltSsid);
  wm.addParameter(&customAltPass);
  wm.addParameter(&customNightOnly);
  wm.addParameter(&customSqmOffset);
  wm.addParameter(&customTempOffset);
  wm.addParameter(&customHumOffset);
  wm.addParameter(&customPresOffset);

  wm.setTitle("SQM Pro - Configuration");
  wm.setClass("invert");
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
  wm.setBreakAfterConfig(true);

  // ---------------------------------------------------------------------------
  // v2.3.6 — setSaveParamsCallback : capture les valeurs AU MOMENT du submit
  // ---------------------------------------------------------------------------
  // Bug v2.3.5 : appeler getValue() APRÈS startConfigPortal() retourne parfois
  // l'ancienne valeur du buffer (cache WiFiManager). La méthode officielle est
  // d'utiliser un callback qui s'exécute pendant le handling du POST HTTP,
  // donc avant que le webserver embarqué ne soit arrêté.
  //
  // On capture les valeurs DANS DES STRINGS HEAP (pas pointeurs vers buffers
  // locaux) pour éviter tout problème de durée de vie.
  String capStationName, capAltSsid, capAltPass;
  String capNightOnly, capSqmOff, capTempOff, capHumOff, capPresOff;
  bool   callbackFired = false;

  wm.setSaveParamsCallback([&]() {
    callbackFired = true;
    // v2.3.6 : on tente 2 méthodes en parallèle :
    //  1. Via wm.server->arg(name) qui lit BRUT le POST HTTP (= source truth)
    //  2. Via customXxx.getValue() (= méthode WiFiManager standard)
    // Si les 2 diffèrent, on log les 2 et on PRIVILÉGIE server->arg() qui
    // est plus proche du fil HTTP donc immune au cache WiFiManager.
    auto pickArg = [&](const char* paramName, const char* fallback) -> String {
      String fromHttp;
      if (wm.server) {
        fromHttp = wm.server->arg(paramName);
        if (fromHttp.length() > 0) return fromHttp;
      }
      return String(fallback ? fallback : "");
    };
    capStationName = pickArg("station_name", customStationName.getValue());
    capAltSsid     = pickArg("alt_ssid",     customAltSsid.getValue());
    capAltPass     = pickArg("alt_pass",     customAltPass.getValue());
    capNightOnly   = pickArg("night_only",   customNightOnly.getValue());
    capSqmOff      = pickArg("sqm_offset",   customSqmOffset.getValue());
    capTempOff     = pickArg("temp_offset",  customTempOffset.getValue());
    capHumOff      = pickArg("hum_offset",   customHumOffset.getValue());
    capPresOff     = pickArg("pres_offset",  customPresOffset.getValue());

    Serial.println(F("\n[WiFi] setSaveParamsCallback() FIRED ----"));
    Serial.print(F("[WiFi]   station='"));   Serial.print(capStationName); Serial.println("'");
    Serial.print(F("[WiFi]   alt_ssid='"));  Serial.print(capAltSsid);     Serial.println("'");
    Serial.print(F("[WiFi]   alt_pass=[")); Serial.print(capAltPass.length()); Serial.println(F(" chars]"));
    Serial.print(F("[WiFi]   night_only='")); Serial.print(capNightOnly); Serial.println("'");
    Serial.print(F("[WiFi]   sqm_offset='")); Serial.print(capSqmOff);    Serial.println("'");
    Serial.print(F("[WiFi]   temp_offset='")); Serial.print(capTempOff); Serial.println("'");
    Serial.print(F("[WiFi]   hum_offset='")); Serial.print(capHumOff);   Serial.println("'");
    Serial.print(F("[WiFi]   pres_offset='")); Serial.print(capPresOff); Serial.println("'");
    // Comparaison getValue() vs HTTP raw (debug : diff = bug confirmé)
    Serial.print(F("[WiFi]   [debug] sqm_offset via getValue()='"));
    Serial.print(customSqmOffset.getValue());
    Serial.println("'");
    Serial.println(F("[WiFi] ---- END callback ----\n"));
  });

  // On utilise startConfigPortal qui ouvre toujours le portail (vs autoConnect
  // qui tente d'abord une connexion). Ici on a déjà fait nos propres tentatives
  // primaire + secours en amont, donc on veut juste l'UI.
  bool configured = wm.startConfigPortal(apName);

  Serial.print(F("[WiFi] Portal closed. configured="));
  Serial.print(configured ? F("true") : F("false"));
  Serial.print(F(" callbackFired="));
  Serial.println(callbackFired ? F("true") : F("false"));

  // ---------------------------------------------------------------------------
  // v2.3.8 — Persistance : on utilise EXCLUSIVEMENT les valeurs capturées par
  // le callback (pas les getValue() post-portal qui peuvent être incohérents).
  //
  // CORRECTION CRITIQUE v2.3.8 :
  // ❌ v2.3.5/6/7 testaient `if (configured)` = booleen retourne par
  //    startConfigPortal(). MAIS configured=false quand l'utilisateur a
  //    seulement modifie des parametres custom (offsets, station name)
  //    SANS toucher au SSID/password principal. Du coup le code de save
  //    EEPROM n'etait JAMAIS execute apres la 1ere ecriture !
  // ✅ On utilise maintenant `callbackFired` comme indicateur fiable : si
  //    le callback s'est declenche, c'est que l'utilisateur a clique Save
  //    dans le portail (vs timeout) → on persiste TOUT systematiquement.
  // ---------------------------------------------------------------------------
  if (callbackFired) {
    // Note : callbackFired=true garantit que les capXxx sont remplies par
    // le callback (qui utilise wm.server->arg + fallback getValue). Plus
    // besoin de fallback ici.

    // -------- Nom de station --------
    if (capStationName.length() > 0) {
      strncpy(stationName, capStationName.c_str(), stationNameSize - 1);
      stationName[stationNameSize - 1] = 0;
      // v2.3.2 : trim côté firmware
      {
        char* p = stationName;
        while (*p == ' ' || *p == '\t') p++;
        if (p != stationName) memmove(stationName, p, strlen(p) + 1);
        size_t n = strlen(stationName);
        while (n > 0 && (stationName[n-1] == ' ' || stationName[n-1] == '\t')) {
          stationName[--n] = '\0';
        }
      }
      WriteEEStationName(stationName);
    }

    // -------- WiFi de secours --------
    if (capAltSsid.length() > 0) {
      strncpy(altSsid, capAltSsid.c_str(), altSsidSize - 1);
      altSsid[altSsidSize - 1] = 0;
    } else {
      altSsid[0] = 0;
    }
    if (capAltPass.length() > 0) {
      strncpy(altPass, capAltPass.c_str(), altPassSize - 1);
      altPass[altPassSize - 1] = 0;
    }
    WriteEEAltWiFi(altSsid, altPass);

    // -------- Toggle night-only --------
    if (capNightOnly.length() > 0) {
      gNightOnlyPush = (capNightOnly[0] != '0');
    }
    WriteEENightOnly(gNightOnlyPush);

    // ------------------------------------------------------------------------
    // v2.3.6 — Persistance des 4 offsets (SQM + Temp + Hum + Pres)
    //
    // Pour CHAQUE offset, on :
    // 1. Log la valeur reçue ET la valeur actuelle EEPROM (avant écriture)
    // 2. INVALIDE le marker EEPROM (écrit 0xFF dessus) avant de réécrire,
    //    pour contourner toute optimisation cache potentielle de la lib
    //    EEPROM ESP8266 qui pourrait skipper des writes "redondants"
    // 3. Réécrit le marker correct + le float
    // 4. Log la valeur relue depuis EEPROM APRÈS commit pour confirmation
    // ------------------------------------------------------------------------
    extern float HumCalOffset, PresCalOffset;
    extern SQM_TSL2591 sqm;

    Serial.println(F("\n[WiFi] ---- Persisting offsets ----"));

    // === SQM Cal Offset (v2.3.5/6/7) ===
    if (capSqmOff.length() > 0) {
      float newSqm = capSqmOff.toFloat();
      Serial.print(F("[WiFi] SQM: received='"));
      Serial.print(capSqmOff);
      Serial.print(F("' parsed="));
      Serial.print(newSqm, 4);
      Serial.print(F(" current EEPROM="));
      Serial.println(SqmCalOffset, 4);

      // v2.3.7 : dump des bytes EEPROM AVANT toute modification
      // (pour comprendre pourquoi la 2e ecriture echoue silencieusement)
      Serial.print(F("[WiFi] SQM: EEPROM bytes BEFORE [1..6]: "));
      for (int i = EEPROM_SQM_CAL_INDEX_C;
           i <= EEPROM_SQM_CAL_INDEX_F + 3; i++) {
        Serial.print("0x"); Serial.print(EEPROM.read(i), HEX); Serial.print(' ');
      }
      Serial.println();

      if (newSqm >= -25.0f && newSqm <= 25.0f) {
        // v2.3.6: invalidate marker BEFORE rewriting (anti-cache)
        EEPROM.write(EEPROM_SQM_CAL_INDEX_C, 0xFF);
        EEPROM.commit();
        delay(50);

        // v2.3.7 : dump apres invalidation pour verifier que le 0xFF a pris
        Serial.print(F("[WiFi] SQM: EEPROM bytes after invalidate: "));
        for (int i = EEPROM_SQM_CAL_INDEX_C;
             i <= EEPROM_SQM_CAL_INDEX_F + 3; i++) {
          Serial.print("0x"); Serial.print(EEPROM.read(i), HEX); Serial.print(' ');
        }
        Serial.println();

        SqmCalOffset = newSqm;
        WriteEESqmCalOffset(SqmCalOffset);
        EEPROM.commit();
        delay(50);
        sqm.setCalibrationOffset(SqmCalOffset);

        // v2.3.7 : dump apres ecriture pour confirmation finale
        Serial.print(F("[WiFi] SQM: EEPROM bytes AFTER write: "));
        for (int i = EEPROM_SQM_CAL_INDEX_C;
             i <= EEPROM_SQM_CAL_INDEX_F + 3; i++) {
          Serial.print("0x"); Serial.print(EEPROM.read(i), HEX); Serial.print(' ');
        }
        Serial.println();

        // Readback verification
        float readback = ReadEESqmCalOffset();
        Serial.print(F("[WiFi] SQM: written, readback="));
        Serial.print(readback, 4);
        if (readback != SqmCalOffset) {
          Serial.println(F(" MISMATCH!"));
        } else {
          Serial.println(F(" OK"));
        }
      } else {
        Serial.println(F("[WiFi] SQM: value out of range, IGNORED"));
      }
    } else {
      // v2.3.7 : log explicite quand le champ est vide (= rien a faire)
      Serial.println(F("[WiFi] SQM: capSqmOff is EMPTY, no change"));
    }

    // === Temp Cal Offset ===
    if (capTempOff.length() > 0) {
      float v = capTempOff.toFloat();
      Serial.print(F("[WiFi] Temp: received='")); Serial.print(capTempOff);
      Serial.print(F("' parsed=")); Serial.print(v, 4);
      Serial.print(F(" current=")); Serial.println(TempCalOffset, 4);
      if (v >= -50.0f && v <= 50.0f) {
        EEPROM.write(EEPROM_TEMP_CAL_INDEX_C, 0xFF); EEPROM.commit(); delay(20);
        TempCalOffset = v;
        WriteEETempCalOffset(TempCalOffset);
        EEPROM.commit(); delay(20);
        Serial.print(F("[WiFi] Temp: written, readback="));
        Serial.println(ReadEETempCalOffset(), 4);
      }
    }

    // === Hum Cal Offset (v2.3.4) ===
    if (capHumOff.length() > 0) {
      float v = capHumOff.toFloat();
      Serial.print(F("[WiFi] Hum: received='")); Serial.print(capHumOff);
      Serial.print(F("' parsed=")); Serial.print(v, 4);
      Serial.print(F(" current=")); Serial.println(HumCalOffset, 4);
      if (v >= -50.0f && v <= 50.0f) {
        EEPROM.write(EEPROM_HUM_CAL_INDEX_C, 0xFF); EEPROM.commit(); delay(20);
        HumCalOffset = v;
        WriteEEHumCalOffset(HumCalOffset);
        EEPROM.commit(); delay(20);
        Serial.print(F("[WiFi] Hum: written, readback="));
        Serial.println(ReadEEHumCalOffset(), 4);
      }
    }

    // === Pres Cal Offset (v2.3.4) ===
    if (capPresOff.length() > 0) {
      float v = capPresOff.toFloat();
      Serial.print(F("[WiFi] Pres: received='")); Serial.print(capPresOff);
      Serial.print(F("' parsed=")); Serial.print(v, 4);
      Serial.print(F(" current=")); Serial.println(PresCalOffset, 4);
      if (v >= -500.0f && v <= 500.0f) {
        EEPROM.write(EEPROM_PRES_CAL_INDEX_C, 0xFF); EEPROM.commit(); delay(20);
        PresCalOffset = v;
        WriteEEPresCalOffset(PresCalOffset);
        EEPROM.commit(); delay(20);
        Serial.print(F("[WiFi] Pres: written, readback="));
        Serial.println(ReadEEPresCalOffset(), 4);
      }
    }

    Serial.println(F("[WiFi] ---- Offsets persistence done ----\n"));

    Serial.print(F("[WiFi] Station persisted: "));
    Serial.println(stationName);
    if (altSsid[0] != 0) {
      Serial.print(F("[WiFi] Backup SSID persisted: "));
      Serial.println(altSsid);
    } else {
      Serial.println(F("[WiFi] No backup SSID configured."));
    }
  }
  return configured;
}

// -----------------------------------------------------------------------------
// wifiPortal_setup()
// À appeler UNE FOIS au démarrage, AVANT toute autre opération WiFi.
// -----------------------------------------------------------------------------
void wifiPortal_setup() {
  char altSsid[33] = "";
  char altPass[65] = "";

  // 1. Charger depuis EEPROM
  ReadEEStationName(gStationName, sizeof(gStationName));
  if (gStationName[0] == 0) {
    buildDefaultStationName(gStationName, sizeof(gStationName));
  }
  // v2.3.2 : défense en profondeur — supprime les espaces leading/trailing
  // saisis par erreur dans le portail captif (ex: "SQM-Quentin "). Sans ce
  // trim, le firmware générerait une URL invalide à chaque push HTTPS.
  sqm_sanitize_station_name();
  // v2.3.3 : restaure le toggle night-only depuis l'EEPROM s'il a été
  // configuré au moins une fois via le portail captif. Sinon on garde le
  // défaut compile-time (cf. déclaration de gNightOnlyPush en haut).
  bool nightFromEE;
  if (ReadEENightOnly(&nightFromEE)) {
    gNightOnlyPush = nightFromEE;
    Serial.print(F("[WiFi] Night-only push (EEPROM): "));
    Serial.println(gNightOnlyPush ? F("ON") : F("OFF (mode test)"));
  } else {
    Serial.print(F("[WiFi] Night-only push (default): "));
    Serial.println(gNightOnlyPush ? F("ON") : F("OFF"));
  }
  ReadEEAltWiFi(altSsid, sizeof(altSsid), altPass, sizeof(altPass));

  // 2. Double reset → portail forcé (skip toutes les tentatives auto)
  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset()) {
    Serial.println(F("\n[WiFi] Double reset detected -> opening config portal"));
    runConfigPortal(gStationName, sizeof(gStationName),
                    altSsid, sizeof(altSsid),
                    altPass, sizeof(altPass));
    gWifiPortalOk = (WiFi.status() == WL_CONNECTED);
    return;
  }

  // 3. Tentative WiFi primaire (credentials persistés en flash)
  if (tryPrimaryWifi()) {
    gWifiPortalOk = true;
    Serial.print(F("[WiFi] Connected (primary), IP: "));
    Serial.println(WiFi.localIP());
    return;
  }

  // 4. Tentative WiFi de secours (si configuré)
  if (altSsid[0] != 0 && tryAltWifi(altSsid, altPass)) {
    gWifiPortalOk = true;
    Serial.print(F("[WiFi] Connected (backup), IP: "));
    Serial.println(WiFi.localIP());
    return;
  }

  // 5. Tout a échoué → portail captif
  Serial.println(F("[WiFi] Primary+backup failed -> opening config portal"));
  runConfigPortal(gStationName, sizeof(gStationName),
                  altSsid, sizeof(altSsid),
                  altPass, sizeof(altPass));
  gWifiPortalOk = (WiFi.status() == WL_CONNECTED);
  if (gWifiPortalOk) {
    Serial.print(F("[WiFi] Connected after portal, IP: "));
    Serial.println(WiFi.localIP());
  }
}

// -----------------------------------------------------------------------------
// wifiPortal_loop()
// -----------------------------------------------------------------------------
void wifiPortal_loop() {
  if (drd) drd->loop();
}

#endif // WIFI_ON
