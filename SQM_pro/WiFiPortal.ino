// WiFiPortal.ino
// Portail captif d'auto-configuration WiFi pour les sondes SQM Pro DIY.
//
// Copyright (c) 2026 Quentin Dumont
//
// Au premier boot (ou si l'EEPROM ne contient pas encore de credentials WiFi),
// le firmware ouvre un point d'accès `SQM-Setup-XXXXXX` (où XXXXXX = 6 derniers
// hex du chip ID ESP8266). L'utilisateur s'y connecte depuis son téléphone,
// un portail captif s'ouvre automatiquement, et il saisit :
//
//   - le SSID de son WiFi domestique
//   - le mot de passe associé
//   - un nom personnalisé pour sa station (ex: "SQM-Maison", "SQM-PicDuMidi")
//     → ce nom est envoyé au backend comme paramètre `ID=` et permet
//       d'identifier les mesures sur le dashboard multi-stations.
//
// Pour FORCER une reconfiguration sans accès physique au boîtier (boîtier
// scellé / installé en extérieur), l'utilisateur peut faire un DOUBLE RESET :
//   1. Couper le courant de la sonde
//   2. Rallumer  (le firmware démarre, attend 5 s)
//   3. Re-couper le courant DANS LES 5 SECONDES qui suivent
//   4. Rallumer → le firmware détecte le double reset et relance le portail
//
// Cette technique (Double Reset Detection / DRD) utilise la RTC memory de
// l'ESP8266 qui survit à un soft reset mais pas à une coupure d'alimentation
// > ~5 s, ce qui est exactement le comportement attendu.
//
// Bibliothèques utilisées (à ajouter dans la CI) :
//   - WiFiManager           (tzapu, v2.0.16+)
//   - ESP_DoubleResetDetector (khoih-prog, v1.3.2+)
//
#ifdef WIFI_ON

#include <ESP8266WiFi.h>

// WiFiManager + DRD
// Note: les deux libs sont header-only / .h+.cpp dans le dossier lib, donc
// arduino-cli les concatène automatiquement. Pas besoin de variables globales
// statiques séparées.
#define ESP_DRD_USE_LITTLEFS    false
#define ESP_DRD_USE_SPIFFS      false
#define ESP_DRD_USE_EEPROM      false  // on utilise la RTC memory, plus rapide
#define ESP8266_DRD_USE_RTC     true
#define DOUBLERESETDETECTOR_DEBUG false
#include <ESP_DoubleResetDetector.h>

#define WM_NODEBUG  // commente pour activer les logs WiFiManager sur Serial
#include <WiFiManager.h>

// -----------------------------------------------------------------------------
// Paramètres de configuration du portail
// -----------------------------------------------------------------------------
// Délai max pour faire le 2e reset (en secondes). 5 s laisse le temps de
// rebrancher le câble d'alim sans être trop laxiste.
#define DRD_TIMEOUT 5
// Adresse en RTC memory (0..511 pour ESP8266). 0 est le standard.
#define DRD_ADDRESS 0

// Timeout du portail captif quand il est ouvert automatiquement (sec).
// Si l'utilisateur ne configure rien dans ce délai, on tente quand même un
// boot normal (l'ESP retentera la dernière connexion connue). Évite que la
// sonde reste bloquée en mode AP si l'utilisateur s'est juste éloigné.
#define WIFI_PORTAL_TIMEOUT 300  // 5 min

// -----------------------------------------------------------------------------
// État global (initialisé dans wifiPortal_setup, utilisé par WiFi.ino)
// -----------------------------------------------------------------------------
DoubleResetDetector* drd = nullptr;

// Nom de la station envoyé au backend comme paramètre ID. Initialisé soit
// depuis l'EEPROM (si l'utilisateur l'a personnalisé via le portail), soit
// auto-généré au format `SQM-XXXXXX`. Cette variable remplace l'ancienne
// `SensorID` (qui venait de secrets.h en dur).
char gStationName[33] = "";

// Indique si la connexion WiFi initiale a réussi (true) ou échoué (false).
// WiFi.ino lit cette valeur pour activer/inhiber les push HTTPS.
bool gWifiPortalOk = false;

// -----------------------------------------------------------------------------
// Helper : auto-génère un nom par défaut SQM-XXXXXX d'après le chip ID
// -----------------------------------------------------------------------------
static void buildDefaultStationName(char* out, size_t outSize) {
  snprintf(out, outSize, "SQM-%06X", ESP.getChipId() & 0xFFFFFF);
}

// -----------------------------------------------------------------------------
// wifiPortal_setup()
// À appeler UNE FOIS au démarrage, AVANT toute autre opération WiFi.
//   - Détecte le double reset (force le portail si oui)
//   - Sinon : tente l'auto-connexion avec les credentials EEPROM. En cas
//     d'échec ou de premier boot, ouvre le portail captif.
//   - Au retour, soit le WiFi est connecté (gWifiPortalOk = true), soit on
//     part en mode dégradé (la sonde continuera de mesurer en local sans
//     pousser, jusqu'au prochain reboot ou double reset).
// -----------------------------------------------------------------------------
void wifiPortal_setup() {
  // 1. Charger le nom de station depuis EEPROM (ou auto-générer si absent)
  ReadEEStationName(gStationName, sizeof(gStationName));
  if (gStationName[0] == 0) {
    buildDefaultStationName(gStationName, sizeof(gStationName));
  }

  // 2. Init du Double Reset Detector
  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);

  // 3. Construire le SSID du point d'accès (unique par chip ID)
  char apName[32];
  snprintf(apName, sizeof(apName), "SQM-Setup-%06X",
           ESP.getChipId() & 0xFFFFFF);

  // 4. Préparer le WiFiManager + champ personnalisé "nom de station"
  WiFiManager wm;

#ifdef WM_NODEBUG
  wm.setDebugOutput(false);
#endif

  WiFiManagerParameter customStationName(
    "station_name",
    "Nom de la station (ex: SQM-Maison, SQM-PicDuMidi)",
    gStationName,
    32
  );
  wm.addParameter(&customStationName);

  // Configuration esthétique du portail
  wm.setTitle("SQM Pro - Configuration");
  wm.setClass("invert");          // thème sombre (cohérent avec l'app web)
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
  wm.setBreakAfterConfig(true);   // sort dès que l'utilisateur a sauvé

  bool configured = false;

  // 5. Branche A : double reset détecté → portail forcé même si WiFi en EEPROM
  if (drd->detectDoubleReset()) {
    Serial.println(F("\n[WiFi] Double reset detected -> opening config portal"));
    configured = wm.startConfigPortal(apName);
  }
  // 6. Branche B : tentative d'auto-connexion (utilise les credentials
  //    sauvés en flash par WiFi.begin lors d'une session précédente).
  //    Si échec → portail captif ouvert automatiquement par WiFiManager.
  else {
    Serial.println(F("\n[WiFi] autoConnect..."));
    configured = wm.autoConnect(apName);
  }

  // 7. Si l'utilisateur a sauvé un nouveau nom, le persister en EEPROM
  if (configured) {
    const char* newName = customStationName.getValue();
    if (newName && newName[0] != 0
        && strncmp(newName, gStationName, sizeof(gStationName)) != 0) {
      strncpy(gStationName, newName, sizeof(gStationName) - 1);
      gStationName[sizeof(gStationName) - 1] = 0;
      WriteEEStationName(gStationName);
      EEPROM.commit();
      Serial.print(F("[WiFi] Station name persisted: "));
      Serial.println(gStationName);
    }
  }

  gWifiPortalOk = (WiFi.status() == WL_CONNECTED);

  if (gWifiPortalOk) {
    Serial.print(F("[WiFi] Connected, IP: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("[WiFi] Station: "));
    Serial.println(gStationName);
  } else {
    Serial.println(F("[WiFi] Not connected after portal/auto-connect."));
  }

  // 8. Le DRD a maintenant rempli son rôle initial. On déclenche un timer
  //    qui, au bout de DRD_TIMEOUT secondes, marquera "boot stable" pour
  //    qu'un prochain power-cycle ne soit pas vu comme un double reset.
  //    Le `drd->loop()` dans la boucle principale gère ça.
}

// -----------------------------------------------------------------------------
// wifiPortal_loop()
// À appeler à chaque tour de loop(). Sans cet appel, le DRD ne pourra
// jamais reset son flag → un reboot normal serait interprété comme un
// double reset au prochain boot.
// -----------------------------------------------------------------------------
void wifiPortal_loop() {
  if (drd) drd->loop();
}

#endif // WIFI_ON
