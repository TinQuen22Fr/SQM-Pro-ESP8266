# Changelog

Tous les changements notables de ce projet sont documentés dans ce fichier.

Format basé sur [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) ;
le projet suit le [versionnage sémantique](https://semver.org/lang/fr/).

## [v2.1.2] — 2026-05-04

### Corrigé
- 🐛 **Oscillation OLED entre "Wait USB data" et la page mesures sur
  NodeMCU nu** (sans la PCB SQM-HR d'origine de Roman Hujer).
  GPIO2 = ModePin est aussi attaché à la LED bleue intégrée et à
  Serial1 TX du boot, donc la broche flappe en permanence et le
  firmware bascule sans cesse entre mode normal et mode USB.
- ➕ Nouveau flag `USB_MODE_OFF` (par défaut) qui ignore complètement
  ModePin et force le mode normal. Les utilisateurs de la PCB
  d'origine peuvent rétablir le comportement legacy avec
  `#define USB_MODE_ON`.

### Documentation
- 📄 `DEPLOY.md` §9 Dépannage : nouvelle entrée pour ce symptôme avec
  la solution.

---

## [v2.1.1] — 2026-05-04

### Modifié
- 🔇 **Debug Wi-Fi désactivé par défaut** dans `Config.h`
  (`DEBUG_WIFI_OFF` au lieu de `DEBUG_WIFI_ON`) pour libérer ~1-2 Ko
  d'IRAM. L'IRAM était à 96 % d'occupation à la compilation, cette
  modification ramène la marge à ~3-4 Ko sans rien perdre côté
  fonctionnel. À réactiver à la main si besoin pour debug.

### Documentation
- 📄 Précision dans `DEPLOY.md` §1 : NodeMCU v3 LoLin et NodeMCU 1.0
  partagent le même module ESP-12E, sélectionner *NodeMCU 1.0
  (ESP-12E Module)* dans Arduino IDE dans les deux cas.
- 📄 Nouvelle section `DEPLOY.md` §14 — Notes sur l'occupation mémoire
  ESP8266 (RAM/IRAM/Flash) avec leviers pour libérer de l'IRAM si
  besoin futur.
- 📄 Mention du bug connu `SyntaxWarning: invalid escape sequence '\s'`
  dans `elf2bin.py` du core ESP8266 3.1.2 (sans impact, fixé en 3.1.3+).

---

## [v2.1.0] — 2026-05-04

### Ajouté
- 🌃 **Mode « nuit uniquement »** : le firmware n'envoie plus de mesures à
  l'API quand le TSL2591 détecte qu'il fait jour (magnitude trop faible
  pour être physiquement valide). Activé par défaut via
  `#define NIGHT_ONLY_PUSH_ON` dans `Config.h`, seuil ajustable via
  `NIGHT_THRESHOLD_MPSAS` (défaut : 12.0 = crépuscule nautique).
  - Évite de polluer le dashboard avec des données de jour (TSL2591 saturé).
  - Économise la bande passante et l'API en mode continu.
  - Économise la batterie en mode deep-sleep + 18650.
  - Indicateur visuel sur l'OLED (`NGT` / `DAY` en haut à droite).

### Documentation
- 📄 Nouvelle section `DEPLOY.md` §12 — guide complet du mode nuit/jour.
- 📄 Nouveau fichier `FUTURE-IDEA-AMELIORATION.md` recensant les pistes
  d'amélioration non encore implémentées.

---

## [v2.0.0] — 2026-05-04

Première release publique du firmware **SQM Pro pour ESP8266**, adapté
pour pousser ses mesures vers le backend **SQM Nightwatch**
(`magnitude-tracker`) à <https://sqm.quentin-astro.fr>.

### Ajouté
- 🔭 **Pilote TSL2591** intégré (basé sur la lib gshau/SQM_TSL2591) avec
  auto-bump du gain et de l'intégration pour les ciels très sombres ou
  très lumineux.
- 🌡️ **Lecture BME280** (température, humidité, pression) avec
  oversampling x16 et compensation d'altitude pour la pression au niveau
  de la mer.
- 🛰️ **Module GPS NEO-6** optionnel via `SoftwareSerial` (latitude,
  longitude, altitude, satellites, date/heure UTC).
- 📺 **Afficheur OLED 128×64** I²C — SH1106 1,3″ ou SSD1306 0,96″
  (sélection à la compilation).
- ☁️ **Envoi HTTPS** (port 443) vers `/api/sqm_push?ID=...&KEY=...`
  toutes les ~10 s en mode continu :
  - magnitude (`S`), erreur sur la magnitude (`D`)
  - lux calculé depuis le TSL2591 (`L`)
  - température (`T`), humidité (`H`), pression (`P`)
  - tension batterie (`V`)
  - GPS (`Alt`, `Lat`, `Lon`) si verrouillé
- 🔄 **OTA (Over-The-Air)** — flash sans câble USB depuis Arduino IDE.
  Activable via `#define OTA_ON` dans `Config.h`. Hostname et mot de
  passe configurables. Affichage du % de progression sur l'OLED.
- 🌙 **Deep-sleep** — mode batterie longue durée optionnel via
  `#define DEEP_SLEEP_ON`. Cycle complet : Wi-Fi → mesure → push →
  `ESP.deepSleep(SLEEP_SEC * 1e6)`. Consommation moyenne ~20 µA en
  sommeil.
- 💾 **EEPROM** — persistance de l'offset de calibration SQM, de l'offset
  de température, du contraste OLED et des flags auto-contrast /
  auto-temp-cal.
- 🔌 **Mode USB / Unihedron** — protocole série compatible avec le
  standard Unihedron SQM-LE (`i`, `r`, `u`, `w` étendu météo, `g`,
  `z…`, `A50/51/d/e`).
- 🆔 **Multi-capteurs** — support natif de plusieurs `SensorID`
  (`SQM-001`, `SQM-002`, …) sur la même clé API. Chaque flux est stocké
  séparément côté backend sous `device_id`.
- 📚 **Documentation** : README.md (français, avec schéma de câblage
  ASCII), DEPLOY.md (guide d'installation pas à pas), LICENSE GPL-3.0.

### Sécurité
- TLS chiffré via `WiFiClientSecure::setInsecure()` (sans validation de
  certificat — l'ESP8266 manque de RAM pour valider une chaîne CA
  complète). La connexion est confidentielle mais non authentifiée
  côté serveur.
- L'auth applicatif s'appuie sur la `sensor_key` côté backend.

### Compatibilité
- Cible : **NodeMCU 1.0 (ESP-12E Module)** sous core `esp8266 ≥ 3.1.x`.
- Bibliothèques requises : Adafruit Unified Sensor ≥ 1.1.14,
  BMx280MI ≥ 1.2.3, U8g2 ≥ 2.34, TinyGPSPlus ≥ 1.0.3.

### Notes connues
- Si `DEEP_SLEEP_ON` ET `OTA_ON` sont définis simultanément, l'OTA est
  rarement joignable (chip endormi la plupart du temps) ; un
  `#warning` est émis à la compilation.
- Le mode deep-sleep nécessite de **relier physiquement GPIO16 (D0) à
  RST** sur la carte (résistance de 470 Ω en série recommandée).
