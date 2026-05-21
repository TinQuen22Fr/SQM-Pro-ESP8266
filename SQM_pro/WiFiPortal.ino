// WiFiPortal.ino
// Portail captif d'auto-configuration WiFi pour les sondes SQM Pro DIY.
//
// Copyright (c) 2026 Quentin Dumont
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

  wm.addParameter(&customStationName);
  wm.addParameter(&customAltSsid);
  wm.addParameter(&customAltPass);

  wm.setTitle("SQM Pro - Configuration");
  wm.setClass("invert");
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
  wm.setBreakAfterConfig(true);

  // On utilise startConfigPortal qui ouvre toujours le portail (vs autoConnect
  // qui tente d'abord une connexion). Ici on a déjà fait nos propres tentatives
  // primaire + secours en amont, donc on veut juste l'UI.
  bool configured = wm.startConfigPortal(apName);

  // Persistance du nom de station
  if (configured) {
    const char* newName = customStationName.getValue();
    if (newName && newName[0] != 0) {
      strncpy(stationName, newName, stationNameSize - 1);
      stationName[stationNameSize - 1] = 0;
      // v2.3.2 : trim côté firmware avant persistance EEPROM, pour éviter
      // qu'un espace saisi par mégarde dans le portail captif ne se
      // propage en URL malformée à chaque push HTTPS.
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
    // Persistance du WiFi de secours (peut être vide pour le supprimer)
    const char* newAltSsid = customAltSsid.getValue();
    const char* newAltPass = customAltPass.getValue();
    if (newAltSsid && newAltSsid[0] != 0) {
      strncpy(altSsid, newAltSsid, altSsidSize - 1);
      altSsid[altSsidSize - 1] = 0;
    } else {
      altSsid[0] = 0;
    }
    // Si l'utilisateur a saisi un nouveau password, on l'utilise ; sinon, on
    // conserve celui déjà en EEPROM (newAltPass == "" car non pré-rempli).
    // Donc on ne réécrit le password que si l'utilisateur a tapé quelque chose.
    if (newAltPass && newAltPass[0] != 0) {
      strncpy(altPass, newAltPass, altPassSize - 1);
      altPass[altPassSize - 1] = 0;
    }
    WriteEEAltWiFi(altSsid, altPass);
    EEPROM.commit();

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
