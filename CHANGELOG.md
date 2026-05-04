# Changelog

Tous les changements notables de ce projet sont documentés dans ce fichier.

Format basé sur [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) ;
le projet suit le [versionnage sémantique](https://semver.org/lang/fr/).

## [v2.2.1] — 2026-05-04

### Corrigé
- 🔧 **Brochage NodeMCU corrigé** pour matcher le câblage réel du SQM Pro
  PCB (Quentin Dumont / Roman Hujer) :
  - `ModePin` : **GPIO2 (D4) → GPIO14 (D5)**. Lit l'interrupteur de
    façade côté Unihedron USB. GPIO2 (D4) reste libre pour le GPS.
  - `gpsSerial` : **(13, 15) → (2, 13)**. NodeMCU reçoit le NMEA du GPS
    sur GPIO2 (D4 = GPS TXD), envoie sur GPIO13 (D7 = GPS RXD).
  - `BuzzerPin` : inchangé, GPIO12 (D6).

### Documentation
- 📄 `Setup.h` réécrit avec un **tableau ASCII complet du brochage**
  NodeMCU (broche / GPIO / usage / notes) et explication des pièges
  GPIO0 (boot strap), GPIO2 (LED + Serial1 boot TX), GPIO15
  (boot strap pull-down).
- 📄 `DEPLOY.md` §8 « Modes de fonctionnement » entièrement réécrite
  pour expliquer l'interrupteur SPDT 3 positions center-off :
  centre = mode normal, côté D5 = mode Unihedron USB, côté D3 au
  boot = mode flash USB.
- 📄 `README.md` schéma de câblage ASCII mis à jour avec les bonnes
  broches et la nouvelle représentation de l'interrupteur 3 positions.

---

## [v2.2.0] — 2026-05-04

### Ajouté
- 🔐 **Fichier `secrets.h` séparé** pour stocker localement les
  identifiants sensibles (Wi-Fi SSID/password, API key, OTA password,
  hostname mDNS). Le fichier est listé dans `.gitignore` donc :
  - Il ne sera **jamais** poussé sur GitHub par accident.
  - Les `git pull` futurs **ne l'écraseront pas**.
- 📋 Nouveau template `SQM_pro/secrets.h.example` (versionné sur Git) à
  copier en `secrets.h` au premier checkout.
- 🔧 `Config.h` détecte automatiquement la présence de `secrets.h` via
  `__has_include("secrets.h")` et utilise les `#define` qu'il contient.
  Si le fichier est absent, des placeholders permettent au moins la
  compilation (fallback `#ifndef` defaults).

### Modifié
- 🔄 `Config.h` ne contient plus aucun secret en dur. Les vraies
  valeurs Wi-Fi / clé API / OTA password sont maintenant lues depuis
  `secrets.h` à la compilation.
- 📄 `DEPLOY.md` §5 réécrite : nouvelle section sur la création de
  `secrets.h` à partir du template, avec snippet d'exemple et
  procédure pour déployer plusieurs capteurs.

### Migration depuis v2.1.x
Au premier `git pull` :
1. `cp SQM_pro/secrets.h.example SQM_pro/secrets.h`
2. Éditez `secrets.h` avec vos vrais identifiants
3. Recompilez (USB ou OTA)

Vous ne perdrez plus jamais vos identifiants au prochain `git pull`. 🎉

---

## [v2.1.3] — 2026-05-04

### Modifié
- 🔄 **Le défaut de `USB_MODE_*` repasse à `USB_MODE_ON`** dans `Config.h`,
  pour respecter la spec d'origine du SQM Pro qui inclut un
  **interrupteur de façade** câblé sur ModePin (GPIO2) permettant de
  basculer entre mode normal (push dashboard) et mode USB/Unihedron.
- ➡️ `USB_MODE_OFF` reste disponible comme opt-in pour les utilisateurs
  qui assemblent un prototype sur breadboard / NodeMCU nu sans
  l'interrupteur de façade.

### Documentation
- 📄 Section `USB / Unihedron mode` de `Config.h` réécrite pour refléter
  le rôle de l'interrupteur de façade et le cas d'usage de chaque flag.
- 📄 `DEPLOY.md` §9 dépannage : entrée mise à jour (oscillation OLED →
  basculer en `USB_MODE_OFF` plutôt que l'inverse).

### Note
La régression introduite en v2.1.2 (qui mettait `USB_MODE_OFF` en
défaut) cassait silencieusement le bouton de façade chez les
utilisateurs de la PCB d'origine. Si vous étiez en v2.1.2 avec une
PCB d'origine, mettez à jour : votre interrupteur retrouve son rôle.

---

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
