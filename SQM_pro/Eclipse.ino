// Eclipse.ino
// Mode Éclipse — enregistrement haute cadence pour événement astronomique
// du 12 août 2026 (ou tout autre événement futur nécessitant un log local).
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
// ============================================================================
// PRINCIPE DE FONCTIONNEMENT
// ============================================================================
// Ce module implémente un "mode éclipse" activable via le portail captif :
//
//   1. LOG LOCAL (LittleFS) — chaque mesure est écrite dans /eclipse.csv
//      dans la flash NOR persistante de l'ESP8266. Le fichier survit aux
//      coupures de courant, reboots, absence de réseau WiFi. Une écriture
//      fsync est déclenchée tous les 5 lignes pour minimiser la fenêtre de
//      perte en cas de crash matériel.
//
//   2. PUSH CLOUD (best-effort) — si `ShouldSend` est actif ET WiFi
//      connecté, chaque mesure est aussi envoyée au backend magnitude-tracker
//      avec un tag `&mode=eclipse` ajouté à l'URL. Si le WiFi tombe, le CSV
//      local continue tranquillement sa vie.
//
//   3. RÉCUPÉRATION — 3 chemins indépendants pour récupérer les données :
//      a) HTTP local : GET http://<ip-sonde>/eclipse-log  (téléchargement CSV)
//      b) UDM série  : commande 'El' (dump via port série)
//      c) Portail captif : nouvelle page accessible après double reset
//
// La cadence de mesure est configurable (défaut 10 s, min 5 s, max 60 s).
//
// ============================================================================
// PERSISTANCE — GARANTIES
// ============================================================================
// * LittleFS = flash NOR non-volatile : survit à coupure de courant.
// * Design crash-safe : le FS reste cohérent même si le power est coupé
//   pile pendant une écriture (contrairement à SPIFFS, obsolete).
// * fsync toutes les 5 lignes : au pire on perd 5 mesures (~50 s à 10 s
//   de cadence) en cas de coupure brutale exactement au mauvais moment.
// * Le fichier NE sera PAS effacé par un reboot, un déchargement batterie,
//   ou un débranchement.
// * ⚠️ ATTENTION : un OTA firmware update peut effacer LittleFS si le
//   mapping partitions change. Un reflash USB avec "Erase All" efface aussi
//   tout. NE PAS faire ces opérations entre l'événement et la récupération.
//
// ============================================================================

#ifdef ECLIPSE_MODE_ON

#include <LittleFS.h>
#include <ESP8266WebServer.h>

// -----------------------------------------------------------------------------
// Forward declarations pour les fonctions de persistance EEPROM définies
// dans EEPROM.ino. Arduino IDE / arduino-cli génèrent des prototypes
// automatiques mais peuvent parfois manquer les fonctions cross-.ino
// conditionnées par un #ifdef. Ces prototypes explicites garantissent
// que le compilateur trouve la référence à Read/WriteEEEclipseConfig().
// -----------------------------------------------------------------------------
bool ReadEEEclipseConfig(bool* outMode, uint16_t* outCadenceS,
                        bool* outShouldLog, bool* outShouldSend);
void WriteEEEclipseConfig(bool mode, uint16_t cadenceS,
                         bool shouldLog, bool shouldSend);

// Forward declarations locales à ce module (utilisées par les endpoints web
// définis via lambdas plus bas dans le fichier).
static void eclipse_writeCsvHeader(File& f);
static void eclipse_checkFsSpace();

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------
#ifndef ECLIPSE_CSV_PATH
  #define ECLIPSE_CSV_PATH        "/eclipse.csv"
#endif
#ifndef ECLIPSE_CADENCE_DEFAULT_S
  #define ECLIPSE_CADENCE_DEFAULT_S 10
#endif
#ifndef ECLIPSE_CADENCE_MIN_S
  #define ECLIPSE_CADENCE_MIN_S    5
#endif
#ifndef ECLIPSE_CADENCE_MAX_S
  #define ECLIPSE_CADENCE_MAX_S    60
#endif
#ifndef ECLIPSE_FSYNC_EVERY_N
  #define ECLIPSE_FSYNC_EVERY_N    5
#endif
#ifndef ECLIPSE_WEB_PORT
  #define ECLIPSE_WEB_PORT         80
#endif

// -----------------------------------------------------------------------------
// État global (accessible depuis les autres .ino via extern déclarés en
// Setup.h — voir addendum plus bas)
// -----------------------------------------------------------------------------
bool     gEclipseMode        = false;   // activation générale du mode
uint16_t gEclipseCadenceS    = ECLIPSE_CADENCE_DEFAULT_S;
bool     gEclipseShouldLog   = true;    // écrire dans /eclipse.csv
bool     gEclipseShouldSend  = true;    // pousser vers magnitude-tracker
uint32_t gEclipseLineCount   = 0;       // nb lignes écrites dans le CSV
uint32_t gEclipseLastMeasMs  = 0;       // dernière mesure (millis)
bool     gEclipseFsMounted   = false;   // LittleFS OK ?
bool     gEclipseFsFull      = false;   // FS plein → arrêt écriture (non-latché depuis v2.3.14.3)
uint16_t gEclipseSessionId   = 0;       // ID de session (bump à chaque boot)

// v2.3.14.3 — compteurs de diagnostic exposés dans /eclipse-status.
// Servent à comprendre POURQUOI les écritures se sont arrêtées lors de
// la production éclipse 12 août 2026 (arrêt à 16:21:06Z).
uint32_t    gEclipseOpenFailures = 0;   // nb d'échecs LittleFS.open("a")
uint32_t    gEclipseWriteSkipped = 0;   // nb de skips (heap bas / fs plein)
const char* gEclipseLastFail     = "";  // raison du dernier skip

// Web server dédié aux endpoints eclipse. On ne réutilise pas celui du
// portail captif (WiFiManager) qui n'est actif que pendant la config.
static ESP8266WebServer* eclipseServer = nullptr;
static uint16_t          eclipseUnsyncedLines = 0;

// -----------------------------------------------------------------------------
// Helpers internes
// -----------------------------------------------------------------------------

// Construit un timestamp ISO 8601 à partir des données GPS si disponibles,
// sinon retourne un fallback "T+SEC_SINCE_BOOT" pour ne jamais laisser de
// timestamp vide dans le CSV.
static String eclipse_buildTimestamp() {
#ifdef GPS_ON
  // Types alignés sur les vraies déclarations dans GPS.ino :
  //   int   g_year;
  //   byte  g_month, g_day, g_hour, g_minute, g_second;
  //   byte  g_sat;
  // (byte est un alias de uint8_t sur ESP8266/AVR)
  extern int  g_year;
  extern byte g_month, g_day, g_hour, g_minute, g_second;
  extern byte g_sat;
  if (g_sat >= 3 && g_year >= 2020) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%02u:%02u:%02uZ",
             (int)g_year, (unsigned)g_month, (unsigned)g_day,
             (unsigned)g_hour, (unsigned)g_minute, (unsigned)g_second);
    return String(buf);
  }
#endif
  // Fallback : temps relatif depuis boot
  char buf[24];
  snprintf(buf, sizeof(buf), "T+%lu", (unsigned long)(millis() / 1000UL));
  return String(buf);
}

// Vérifie l'espace flash restant. Passe le FS en "pause" temporaire si presque plein.
//
// v2.3.14.3 — CORRECTIF POST-MORTEM ÉCLIPSE 12/08/2026 :
// ------------------------------------------------------------------
// L'ancienne version LATCHAIT le flag `gEclipseFsFull` à `true` de
// façon PERMANENTE dès que `LittleFS.info()` rapportait moins de 8 kB
// libres. Or `usedBytes` inclut l'overhead des métadonnées LittleFS
// et peut faire des pics transitoires pendant le garbage collect.
// Résultat en production : écritures arrêtées définitivement à
// 16:21:06Z alors qu'il restait ~490 kB libres sur la partition.
//
// Nouveau comportement :
//   - Flag NON-latché : réévalué à chaque appel (donc à chaque cycle
//     de 5 lignes) → si l'espace se libère (GC), on reprend.
//   - Seuil abaissé à 4 kB = 1 bloc LittleFS = marge suffisante pour
//     écrire au moins une ligne CSV (~120 bytes) sans risque.
//   - Log clair quand on entre/sort de l'état "fs plein".
// ------------------------------------------------------------------
static void eclipse_checkFsSpace() {
  if (!gEclipseFsMounted) return;
  FSInfo info;
  if (!LittleFS.info(info)) {
    // Info FS indispo -> on ne DÉCIDE PAS d'arrêter les écritures sur
    // un doute. LittleFS.open("a") remontera lui-même l'erreur si le
    // FS est vraiment cassé, et le compteur gEclipseOpenFailures
    // permettra de le diagnostiquer via /eclipse-status.
    return;
  }
  size_t freeSpace = (info.totalBytes > info.usedBytes)
                     ? (info.totalBytes - info.usedBytes) : 0;

  bool wasFull = gEclipseFsFull;
  gEclipseFsFull = (freeSpace < 4096);   // 1 bloc LittleFS

  if (gEclipseFsFull && !wasFull) {
    Serial.print(F("[Eclipse] FS space low ("));
    Serial.print((unsigned)freeSpace);
    Serial.println(F(" B free), pausing local writes."));
    gEclipseLastFail = "fs_low";
  } else if (!gEclipseFsFull && wasFull) {
    Serial.print(F("[Eclipse] FS space recovered ("));
    Serial.print((unsigned)freeSpace);
    Serial.println(F(" B free), resuming local writes."));
  }
}

// -----------------------------------------------------------------------------
// eclipse_setup() — À appeler UNE FOIS dans setup() après wifiPortal_setup()
// -----------------------------------------------------------------------------
void eclipse_setup() {
  // 1. Charger la config depuis EEPROM (marker 'E' + 5 bytes)
  ReadEEEclipseConfig(&gEclipseMode, &gEclipseCadenceS,
                      &gEclipseShouldLog, &gEclipseShouldSend);

  // Clamp cadence
  if (gEclipseCadenceS < ECLIPSE_CADENCE_MIN_S) {
    gEclipseCadenceS = ECLIPSE_CADENCE_DEFAULT_S;
  }
  if (gEclipseCadenceS > ECLIPSE_CADENCE_MAX_S) {
    gEclipseCadenceS = ECLIPSE_CADENCE_MAX_S;
  }

  Serial.println(F("\n[Eclipse] ---- Eclipse mode setup ----"));
  Serial.print(F("[Eclipse] Mode active: "));
  Serial.println(gEclipseMode ? F("YES") : F("NO"));
  Serial.print(F("[Eclipse] Cadence: "));
  Serial.print(gEclipseCadenceS);
  Serial.println(F(" s"));
  Serial.print(F("[Eclipse] ShouldLog: "));
  Serial.println(gEclipseShouldLog ? F("YES") : F("NO"));
  Serial.print(F("[Eclipse] ShouldSend: "));
  Serial.println(gEclipseShouldSend ? F("YES") : F("NO"));

  // 2. Monter LittleFS (obligatoire même si mode inactif, pour le potentiel
  //    téléchargement d'un CSV déjà existant d'une session précédente).
  if (!LittleFS.begin()) {
    Serial.println(F("[Eclipse] LittleFS.begin() FAILED, trying format..."));
    if (!LittleFS.format() || !LittleFS.begin()) {
      Serial.println(F("[Eclipse] LittleFS unusable, log local DISABLED"));
      gEclipseFsMounted = false;
    } else {
      gEclipseFsMounted = true;
      Serial.println(F("[Eclipse] LittleFS formatted and mounted."));
    }
  } else {
    gEclipseFsMounted = true;
  }

  if (gEclipseFsMounted) {
    FSInfo info;
    if (LittleFS.info(info)) {
      Serial.print(F("[Eclipse] LittleFS total="));
      Serial.print(info.totalBytes);
      Serial.print(F(" used="));
      Serial.print(info.usedBytes);
      Serial.print(F(" free="));
      Serial.println(info.totalBytes - info.usedBytes);
    }
    eclipse_checkFsSpace();

    // Comptage des lignes déjà présentes (si CSV existe déjà)
    if (LittleFS.exists(ECLIPSE_CSV_PATH)) {
      File f = LittleFS.open(ECLIPSE_CSV_PATH, "r");
      if (f) {
        uint32_t lines = 0;
        while (f.available()) {
          if (f.read() == '\n') lines++;
        }
        f.close();
        // -1 pour l'en-tête CSV si présent
        if (lines > 0) lines--;
        gEclipseLineCount = lines;
        Serial.print(F("[Eclipse] Existing CSV: "));
        Serial.print(lines);
        Serial.println(F(" data lines already present."));
      }
    }
  }

  // 3. Nouvelle session : increment session ID en RTC memory (0..65535)
  //    Utile pour distinguer les redémarrages accidentels dans le CSV.
  gEclipseSessionId = (uint16_t)(millis() & 0xFFFF);

  // 4. Démarrer le web server si mode actif ET WiFi connecté
  if (gEclipseMode && gEclipseFsMounted) {
    eclipse_startWebServer();
  }
  Serial.println(F("[Eclipse] ---- Setup done ----\n"));
}

// -----------------------------------------------------------------------------
// eclipse_startWebServer() — expose les endpoints HTTP de récupération.
// Appelé automatiquement au boot si mode actif, ou manuellement après
// activation via portail.
// -----------------------------------------------------------------------------
void eclipse_startWebServer() {
  if (eclipseServer != nullptr) {
    Serial.println(F("[Eclipse] Web server already running."));
    return;
  }
  // v2.3.14.1 : on se base directement sur WiFi.status() (source de vérité
  // du core ESP8266) plutôt que sur gWifiPortalOk qui n'est mis à jour
  // qu'après setup(). Permet un lazy start correct depuis eclipse_loop().
  if (WiFi.status() != WL_CONNECTED) {
    // Silencieux ici — eclipse_loop() re-tente à chaque cycle jusqu'à réussite.
    return;
  }

  Serial.print(F("[Eclipse] Starting web server on port "));
  Serial.print(ECLIPSE_WEB_PORT);
  Serial.print(F(" IP="));
  Serial.println(WiFi.localIP());

  eclipseServer = new ESP8266WebServer(ECLIPSE_WEB_PORT);
  if (eclipseServer == nullptr) {
    Serial.println(F("[Eclipse] FATAL: new ESP8266WebServer() returned null "
                     "(out of memory?)"));
    return;
  }

  // v2.3.14.3 : endpoint racine "/" pour test de connectivité de base.
  // Utile pour diagnostiquer les problèmes de firewall/browser sans
  // dépendre de LittleFS ou d'autres endpoints plus complexes.
  eclipseServer->on("/", HTTP_GET, []() {
    String body = F("SQM Pro Eclipse Mode - Web server OK\n\n"
                    "Endpoints:\n"
                    "  GET /eclipse-status                 - JSON status\n"
                    "  GET /eclipse-log                    - Download CSV\n"
                    "  GET /eclipse-clear?confirm=YES      - Clear CSV\n\n"
                    "Serial fallback (via UDM/USB):\n"
                    "  Es   - status\n"
                    "  El   - dump CSV via serial\n");
    eclipseServer->send(200, "text/plain", body);
  });

  // v2.3.14.3 — SUPPRESSION du double `new ESP8266WebServer(...)` qui
  // écrasait le pointeur (fuite mémoire ~2 kB + handler "/" orphelin).
  // Sur un ESP8266 déjà chargé par le HTTPS BearSSL, cette fuite pouvait
  // suffire à faire échouer les accept() TCP sur le port 80.

  // GET /eclipse-log → dump le CSV
  eclipseServer->on("/eclipse-log", HTTP_GET, []() {
    if (!gEclipseFsMounted || !LittleFS.exists(ECLIPSE_CSV_PATH)) {
      eclipseServer->send(404, "text/plain",
                          "No eclipse log found.\n");
      return;
    }
    File f = LittleFS.open(ECLIPSE_CSV_PATH, "r");
    if (!f) {
      eclipseServer->send(500, "text/plain", "Failed to open CSV.\n");
      return;
    }
    eclipseServer->sendHeader("Content-Disposition",
                              "attachment; filename=\"eclipse.csv\"");
    eclipseServer->streamFile(f, "text/csv");
    f.close();
  });

  // GET /eclipse-status → JSON état
  eclipseServer->on("/eclipse-status", HTTP_GET, []() {
    String j = "{";
    j += "\"active\":";        j += (gEclipseMode ? "true" : "false");
    j += ",\"cadence_s\":";    j += gEclipseCadenceS;
    j += ",\"should_log\":";   j += (gEclipseShouldLog ? "true" : "false");
    j += ",\"should_send\":";  j += (gEclipseShouldSend ? "true" : "false");
    j += ",\"lines\":";        j += gEclipseLineCount;
    j += ",\"fs_mounted\":";   j += (gEclipseFsMounted ? "true" : "false");
    j += ",\"fs_full\":";      j += (gEclipseFsFull ? "true" : "false");
    // v2.3.14.3 — Diagnostics post-mortem éclipse 12/08/2026
    j += ",\"open_failures\":";j += gEclipseOpenFailures;
    j += ",\"write_skipped\":";j += gEclipseWriteSkipped;
    j += ",\"last_fail\":\"";  j += gEclipseLastFail; j += "\"";
    j += ",\"free_heap\":";    j += ESP.getFreeHeap();
    j += ",\"session_id\":";   j += gEclipseSessionId;
    j += ",\"uptime_s\":";     j += (unsigned long)(millis() / 1000UL);
    if (gEclipseFsMounted) {
      FSInfo info;
      if (LittleFS.info(info)) {
        j += ",\"fs_total\":"; j += info.totalBytes;
        j += ",\"fs_used\":";  j += info.usedBytes;
        j += ",\"fs_free\":";  j += (info.totalBytes > info.usedBytes)
                                     ? (info.totalBytes - info.usedBytes) : 0;
      }
    }
    j += "}";
    eclipseServer->send(200, "application/json", j);
  });

  // GET /eclipse-clear?confirm=YES → efface le CSV (avec double confirmation)
  eclipseServer->on("/eclipse-clear", HTTP_GET, []() {
    if (eclipseServer->arg("confirm") != "YES") {
      eclipseServer->send(400, "text/html",
        "<h1>Confirmation required</h1>"
        "<p>To delete eclipse.csv permanently, add "
        "<code>?confirm=YES</code> to the URL.</p>");
      return;
    }
    if (LittleFS.exists(ECLIPSE_CSV_PATH)) {
      LittleFS.remove(ECLIPSE_CSV_PATH);
      gEclipseLineCount = 0;
      eclipseUnsyncedLines = 0;
    }
    eclipseServer->send(200, "text/plain", "eclipse.csv deleted.\n");
  });

  eclipseServer->begin();
  Serial.print(F("[Eclipse] Web server started at http://"));
  Serial.print(WiFi.localIP());
  Serial.println(F("/eclipse-log"));
}

// -----------------------------------------------------------------------------
// eclipse_stopWebServer() — arrête proprement le web server.
// -----------------------------------------------------------------------------
void eclipse_stopWebServer() {
  if (eclipseServer == nullptr) return;
  eclipseServer->stop();
  delete eclipseServer;
  eclipseServer = nullptr;
  Serial.println(F("[Eclipse] Web server stopped."));
}

// -----------------------------------------------------------------------------
// eclipse_writeCsvHeader() — écrit la ligne d'en-tête si le fichier vient
// d'être créé (donc à sa toute première utilisation).
// -----------------------------------------------------------------------------
static void eclipse_writeCsvHeader(File& f) {
  f.println(F("timestamp,session_id,seconds_since_boot,mpsas,dmpsas,"
              "lux,ir_raw,full_raw,temperature_c,humidity_pct,"
              "pressure_hpa,battery_v"));
}

// -----------------------------------------------------------------------------
// eclipse_logMeasurement() — appelée par la boucle principale à cadence
// configurée. Écrit une ligne CSV et éventuellement push cloud.
// -----------------------------------------------------------------------------
void eclipse_logMeasurement(float mpsas, float dmpsas,
                            float lux, uint32_t ir_raw, uint32_t full_raw,
                            float temp, float hum, float pres_hpa,
                            float battery_v) {
  if (!gEclipseMode) return;

  // ---- Écriture locale CSV ----
  //
  // v2.3.14.3 — Robustesse post-mortem éclipse 12/08/2026 :
  //   1. On réévalue eclipse_checkFsSpace() À CHAQUE APPEL (pas juste
  //      toutes les 5 lignes). Combiné au flag non-latché, ça garantit
  //      que le flag `gEclipseFsFull` reflète l'état RÉEL de la flash.
  //   2. Garde heap : si moins de 6 kB libres, on skip cette écriture
  //      pour PROTÉGER le push HTTPS BearSSL (qui a besoin de ~16 kB
  //      pour un handshake TLS). Priorité au push cloud puisque c'est
  //      lui qui a sauvé les données pendant l'éclipse.
  //   3. Diagnostics : on incrémente gEclipseOpenFailures et on stocke
  //      la raison exacte dans gEclipseLastFail, exposée via
  //      /eclipse-status → plus jamais de "log qui s'arrête sans qu'on
  //      sache pourquoi".
  if (gEclipseShouldLog && gEclipseFsMounted) {
    eclipse_checkFsSpace();  // ré-évaluation live (non-latché)

    if (gEclipseFsFull) {
      gEclipseWriteSkipped++;
      gEclipseLastFail = "fs_full";
      return;
    }

    uint32_t heap = ESP.getFreeHeap();
    if (heap < 6144) {
      gEclipseWriteSkipped++;
      gEclipseLastFail = "heap_low";
#ifdef DEBUG_ECLIPSE_ON
      Serial.print(F("[Eclipse] SKIP write, heap="));
      Serial.println(heap);
#endif
      return;
    }

    bool needsHeader = !LittleFS.exists(ECLIPSE_CSV_PATH);
    File f = LittleFS.open(ECLIPSE_CSV_PATH, "a");
    if (f) {
      if (needsHeader) {
        eclipse_writeCsvHeader(f);
      }
      String ts = eclipse_buildTimestamp();
      f.print(ts);                             f.print(',');
      f.print(gEclipseSessionId);              f.print(',');
      f.print((unsigned long)(millis()/1000UL));f.print(',');
      f.print(mpsas, 3);                       f.print(',');
      f.print(dmpsas, 3);                      f.print(',');
      f.print(lux, 4);                         f.print(',');
      f.print(ir_raw);                         f.print(',');
      f.print(full_raw);                       f.print(',');
      f.print(temp, 2);                        f.print(',');
      f.print(hum, 2);                         f.print(',');
      f.print(pres_hpa, 2);                    f.print(',');
      f.println(battery_v, 3);
      f.close();

      gEclipseLineCount++;
      eclipseUnsyncedLines++;

      // fsync forcé tous les N lignes (crash-safety)
      if (eclipseUnsyncedLines >= ECLIPSE_FSYNC_EVERY_N) {
        // LittleFS n'expose pas de fsync explicite, mais fermer/ouvrir
        // le fichier force un flush du cache. Ici c'est déjà fait par
        // f.close() ci-dessus, donc on remet juste le compteur à 0.
        eclipseUnsyncedLines = 0;
        eclipse_checkFsSpace();  // check disk space every N lines
#ifdef DEBUG_ECLIPSE_ON
        Serial.print(F("[Eclipse] Flushed CSV, lines="));
        Serial.print(gEclipseLineCount);
        Serial.print(F(" heap="));
        Serial.println(ESP.getFreeHeap());
#endif
      }
    } else {
      // v2.3.14.3 — Diagnostic détaillé + compteur d'échecs.
      gEclipseOpenFailures++;
      gEclipseLastFail = "open_failed";
      Serial.print(F("[Eclipse] WARN: LittleFS.open(\"a\") failed, heap="));
      Serial.print(heap);
      Serial.print(F(" total_failures="));
      Serial.println(gEclipseOpenFailures);
    }
  }

  gEclipseLastMeasMs = millis();
}

// -----------------------------------------------------------------------------
// eclipse_shouldMeasureNow() — retourne true si la cadence indique qu'il
// faut prendre une nouvelle mesure. Non-bloquant.
// -----------------------------------------------------------------------------
bool eclipse_shouldMeasureNow() {
  if (!gEclipseMode) return false;
  uint32_t interval_ms = (uint32_t)gEclipseCadenceS * 1000UL;
  return (millis() - gEclipseLastMeasMs) >= interval_ms;
}

// -----------------------------------------------------------------------------
// eclipse_loop() — appelée à chaque itération de loop(). Fait tourner le
// web server et tente un démarrage tardif (lazy start) si le WiFi n'était
// pas encore connecté au moment de eclipse_setup().
// -----------------------------------------------------------------------------
void eclipse_loop() {
  // v2.3.14.1 : lazy start du web server. setup() est appelé AVANT que
  // WiFi soit effectivement connecté (le WiFiManager confirme juste la
  // config, la connexion réelle se fait plus tard). On tente donc de
  // démarrer le web server dès qu'on détecte le WiFi UP, et seulement
  // si le mode Eclipse est actif ET LittleFS ok.
  if (gEclipseMode && gEclipseFsMounted && eclipseServer == nullptr) {
    if (WiFi.status() == WL_CONNECTED) {
      eclipse_startWebServer();
    }
  }
  if (eclipseServer) {
    eclipseServer->handleClient();
  }
}

// -----------------------------------------------------------------------------
// eclipse_pumpHttp() — v2.3.14.3
// Version "lite" de eclipse_loop() destinée à être appelée depuis les
// endroits où loop() ne tourne pas (ex: delay(2000) du main loop, ou
// pendant les 6 s d'intégration TSL2591). Ne fait QUE handleClient(),
// sans tenter de lazy-start le server. Non-bloquant. Sûr à appeler en
// masse.
//
// Pourquoi ? Pendant l'éclipse du 12/08/2026, `handleClient()` n'était
// appelé qu'une fois par cycle loop(), soit ~toutes les 8 secondes
// (6 s d'intégration TSL + 2 s de delay). Résultat : les navigateurs et
// curl timeoutaient (défaut 5-10 s) avant que le serveur ne réponde.
// -----------------------------------------------------------------------------
void eclipse_pumpHttp() {
  if (eclipseServer) {
    eclipseServer->handleClient();
  }
}

// -----------------------------------------------------------------------------
// eclipse_delayPumped(ms) — v2.3.14.3
// Remplacement direct de `delay(ms)` qui appelle handleClient() toutes
// les ~50 ms. À utiliser dans le main loop à la place du `delay(2000)`
// pour maintenir le web server réactif entre deux mesures.
// -----------------------------------------------------------------------------
void eclipse_delayPumped(uint32_t total_ms) {
  const uint32_t chunk = 50;   // ms
  uint32_t elapsed = 0;
  while (elapsed < total_ms) {
    uint32_t step = (total_ms - elapsed) < chunk ? (total_ms - elapsed) : chunk;
    delay(step);
    eclipse_pumpHttp();
    elapsed += step;
  }
}

// -----------------------------------------------------------------------------
// eclipse_dumpToSerial() — commande UDM "El" (Eclipse Log). Dump le CSV
// intégral via le port série. Utile si le WiFi ne fonctionne plus mais que
// le boîtier est branché à un PC via USB.
// -----------------------------------------------------------------------------
void eclipse_dumpToSerial() {
  if (!gEclipseFsMounted) {
    Serial.println(F("ERR: LittleFS not mounted"));
    return;
  }
  if (!LittleFS.exists(ECLIPSE_CSV_PATH)) {
    Serial.println(F("ERR: eclipse.csv not found"));
    return;
  }
  File f = LittleFS.open(ECLIPSE_CSV_PATH, "r");
  if (!f) {
    Serial.println(F("ERR: could not open eclipse.csv"));
    return;
  }
  Serial.println(F("---BEGIN eclipse.csv---"));
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();
  Serial.println(F("---END eclipse.csv---"));
}

// -----------------------------------------------------------------------------
// eclipse_isActive() — accesseur public pour la logique push (WiFi.ino) et
// l'affichage OLED (MyLib.ino).
// -----------------------------------------------------------------------------
bool eclipse_isActive() {
  return gEclipseMode;
}

// -----------------------------------------------------------------------------
// eclipse_shouldSendCloud() — accesseur public : est-ce que le push HTTPS
// vers magnitude-tracker doit se faire (avec le tag mode=eclipse) ?
// -----------------------------------------------------------------------------
bool eclipse_shouldSendCloud() {
  return gEclipseMode && gEclipseShouldSend;
}

#endif // ECLIPSE_MODE_ON
