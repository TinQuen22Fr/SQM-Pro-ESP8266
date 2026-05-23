# SQM Pro — Sky Quality Meter DIY pour ESP8266

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform: ESP8266](https://img.shields.io/badge/Platform-ESP8266-orange.svg)](https://github.com/esp8266/Arduino)
[![CI: GitHub Actions](https://img.shields.io/badge/CI-GitHub_Actions-2088FF.svg)](.github/workflows/build-firmware.yml)

Firmware pour un **Sky Quality Meter (SQM) DIY** construit autour d'un
ESP8266 (NodeMCU), d'un capteur de lumière TSL2591, d'un capteur météo
BME280, d'un afficheur OLED 128×64 (SH1106 ou SSD1306) et d'un module
GPS NEO-6 optionnel.

Ce firmware envoie automatiquement les mesures via HTTPS vers le tableau
de bord coopératif **[SQM Nightwatch](https://sqm.quentin-astro.fr)**
(le backend `magnitude-tracker`).

```
              ┌────────────────────────┐
              │  ESP8266 (NodeMCU)     │
   TSL2591 ──▶│  + OLED + BME280 + GPS │── Wi-Fi ──▶ https://sqm.quentin-astro.fr/api/sqm_push
              └────────────────────────┘
```

> Pour la procédure de déploiement complète (installation, flash,
> dépannage), consultez **[DEPLOY.md](./DEPLOY.md)**.

---

## 🆕 Nouveautés v2.3.x : portail captif + flash en ligne

À partir de la **v2.3.0**, le firmware devient **distribuable** : plus
besoin d'éditer le code et de compiler localement pour chaque contributeur.

| Avant (v2.2.x et antérieures) | Maintenant (v2.3.x) |
|---|---|
| WiFi `SSID` / `password` codés en dur dans `secrets.h` | **Portail captif** au premier boot (SSID `SQM-Setup-XXXXXX`) |
| `SensorID` codé en dur | **Nom de station personnalisable** via le portail (persisté en EEPROM) |
| WiFi de secours codé en dur (`#define ALT_SSID_ON`) | **WiFi de secours configurable** via le portail (deux champs facultatifs) |
| Compilation locale obligatoire (Arduino IDE) | **Flash 100% web** via [https://sqm.quentin-astro.fr/flasher](https://sqm.quentin-astro.fr/flasher) (Chromium / Edge avec Web Serial) |
| Reconfiguration WiFi → reflash USB | **Double Reset** (couper/rallumer 2× en < 5 s) → ré-ouvre le portail captif sans toucher au boîtier |

La `SENSOR_KEY` (clé API du backend) reste compilée dans le binaire car
elle est **partagée par tous les capteurs** du projet coopératif (=
passe-partout vers le serveur central de Quentin). Elle est injectée
automatiquement par la CI GitHub Actions depuis le secret de dépôt
`SENSOR_KEY` au moment du build. Pas de manipulation côté contributeur.

---

## 1. Matériel

| Composant                 | Détails                                                     |
|---------------------------|-------------------------------------------------------------|
| Microcontrôleur           | ESP8266 NodeMCU v1.0                                        |
| Capteur de lumière        | Adafruit TSL2591 (I²C, 0x29)                                |
| Capteur météo             | BME280 (I²C, 0x76 ou 0x77, configurable)                    |
| Afficheur                 | SH1106 1,3" **ou** SSD1306 0,96" (I²C matériel)             |
| GPS (optionnel)           | u-blox NEO-6M sur `D7` (RX=GPIO13) / `D8` (TX=GPIO15) via `SoftwareSerial` |
| Bouton mode               | Sur `GPIO2` (`ModePin`), pull-up interne                    |
| Buzzer                    | Sur `GPIO12` (`BuzzerPin`)                                  |
| Mesure batterie           | `A0` (pont diviseur donnant un rapport ×11)                 |

Schéma de câblage / PCB : <https://easyeda.com/hujer.roman/sqm-hr>.

### Schéma de câblage simplifié

```
                       ┌───────────────────────────────────────┐
                       │           NodeMCU ESP-12E             │
                       │                                       │
   +5V ────────────────┤ Vin                              3V3 ├──┬──── 3,3 V (capteurs)
   GND ────────────────┤ GND                              GND ├──┤
                       │                                       │  │
   Bus I²C  ╔══════════╡ D2 (GPIO4 = SDA)            (GPIO16) D0╞══╗
            ║          │ D1 (GPIO5 = SCL)                     │  ║   ┌──── 470 Ω ──┐
            ║          │                                      │  ║   │             │
            ║          │ D3 (GPIO0)  ◄──┐  Inter. 3 positions │  ║   │             ▼
            ║          │ D5 (GPIO14) ◄──┤  centre-off (façade)│  ║   │            RST
            ║          │                ├── centre = GND      │  ║   │             ▲
            ║          │                                      │  ║   │  (deep-sleep wake-up:
            ║          │ D4 (GPIO2 = RX SoftSerial) ──> GPS TXD│  ║   │   GPIO16 → RST,
            ║          │ D7 (GPIO13 = TX SoftSerial) ──> GPS RXD│ ║   │   470 Ω optionnel)
            ║          │ D6 (GPIO12 = BuzzerPin) ──> buzzer +  │  ║   │
            ║          │                                      │  ╚═══╛
            ║          │ A0 ◄── pont diviseur ÷11 ── batterie + │
            ║          └────────────────────────────────────────┘
            ║
            ║          ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
            ║          │   TSL2591   │    │   BME280    │    │  OLED 0,96/ │
            ║          │   (0x29)    │    │ (0x76/0x77) │    │  1,3" I²C    │
            ╠══SDA═════╡ SDA         │    │ SDA         │    │ SDA         │
            ╠══SCL═════╡ SCL         │    │ SCL         │    │ SCL         │
            ║          │ VIN ── 3V3  │    │ VIN ── 3V3  │    │ VCC ── 3V3  │
            ║          │ GND ── GND  │    │ GND ── GND  │    │ GND ── GND  │
            ║          │             │    │ CSB ── 3V3  │    │             │
            ║          │             │    │ SDO ── GND  │    │             │
            ║          │             │    │   (=> 0x76) │    │             │
                       └─────────────┘    └─────────────┘    └─────────────┘

   Bus I²C : 4 fils communs SDA + SCL + 3,3 V + GND (résistances pull-up
             ~4,7 kΩ en général déjà présentes sur les modules breakout).

   GPS NEO-6M (alimenté en 3,3 V) :
       GPS TXD ─── NodeMCU D4 (GPIO2)  → NodeMCU reçoit les trames NMEA
       GPS RXD ─── NodeMCU D7 (GPIO13) → NodeMCU envoie commandes au GPS
       3V3 / GND
       ⚠️  Convention UART : toujours croiser TX↔RX. Si vous obtenez
           "GPS no wire!" sur l'OLED, vérifiez d'abord que vous n'avez
           pas TX↔TX ou RX↔RX par mégarde.

   Interrupteur de façade (SPDT 3 positions center-off) :
       Plot central       ── GND
       Plot D3 (GPIO0)    ── NodeMCU D3   (mode flash USB au boot)
       Plot D5 (GPIO14)   ── NodeMCU D5   (mode Unihedron USB en runtime)
       Centre             ── pas de connexion → mode normal Wi-Fi (par défaut)

   ⚠️  Pour activer le mode deep-sleep (cf. DEEP_SLEEP_ON dans Config.h),
       il FAUT relier physiquement GPIO16 (D0) à RST avec une résistance
       de 470 Ω en série (ou un Schottky), sinon le NodeMCU ne se
       réveillera jamais.
```

> 💡 SDA = D2/GPIO4, SCL = D1/GPIO5 sont les broches I²C **matérielles**
> par défaut sur NodeMCU (ne pas modifier).

---

## 2. Installation : 2 méthodes au choix

### 🌐 Méthode A — Flash en ligne (recommandée pour les contributeurs)

**Aucune installation logicielle requise.** Compatible Windows, Linux, macOS.

1. Ouvrir **Chromium**, **Edge** ou **Brave** (Firefox ne supporte pas
   Web Serial). Sur Android, **Chrome Mobile** fonctionne aussi.
2. Aller sur **<https://sqm.quentin-astro.fr/flasher>**.
3. Brancher la NodeMCU en USB (câble *data*, pas un câble charge seule).
4. Cliquer sur **"Connecter & Installer le firmware"** → sélectionner
   le port USB-Serial (`USB-Serial CH340` ou `/dev/ttyUSB0` sur Linux).
5. ESP Web Tools efface la flash puis y écrit le `.bin` (≈ 1 min).
6. Au reboot, la sonde diffuse un point d'accès WiFi
   **`SQM-Setup-XXXXXX`** (où `XXXXXX` = 6 derniers hex du chip ID de
   votre carte). Voir **[§ 3](#3-premier-boot--portail-captif)** pour la
   suite.

> 💡 Le badge en haut à droite de la page Flasher indique la version du
> firmware proposé. Vous pouvez basculer entre la release **Stable** et
> une **Beta** via le toggle juste au-dessus du bouton.

### 🔧 Méthode B — Compilation locale (Arduino IDE)

Réservé aux développeurs / cas d'usage spécifiques (déboguer en série,
changer une calibration en dur, etc.).

1. Installer **Arduino IDE** ≥ 2.0.
2. Dans *Fichier → Préférences → URL de gestionnaire de cartes
   supplémentaires* ajouter :
   ```
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
3. Dans *Outils → Type de carte → Gestionnaire de cartes…*, installer
   **esp8266 by ESP8266 Community** ≥ 3.1.x.
4. Sélectionner la carte : *Outils → Type de carte → ESP8266 Boards →
   **NodeMCU 1.0 (ESP-12E Module)***.
5. Installer les bibliothèques suivantes via *Outils → Gérer les
   bibliothèques…* :

| Bibliothèque                            | Version testée |
|-----------------------------------------|----------------|
| Adafruit Unified Sensor                 | ≥ 1.1.14       |
| BMx280MI (par Gregor Christandl)        | ≥ 1.2.3        |
| U8g2 *(fournit `U8x8lib`)*              | ≥ 2.34.x       |
| TinyGPSPlus                             | ≥ 1.0.3        |
| **WiFiManager** *(tzapu)*               | **≥ 2.0.16**   |
| **ESP_DoubleResetDetector** *(khoih-prog)* | **≥ 1.3.2**  |

Le pilote TSL2591 est inclus dans ce dépôt (`SQM_TSL2591.h/.cpp`),
rien à installer pour lui.

6. `ESP8266WiFi` et `WiFiClientSecure` sont fournis avec le core
   ESP8266 — pas d'installation supplémentaire.

7. **Créer `SQM_pro/secrets.h`** depuis le template :
   ```bash
   cp SQM_pro/secrets.h.example SQM_pro/secrets.h
   ```
   Et y renseigner uniquement votre `SENSOR_KEY` (récupérée sur
   <https://sqm.quentin-astro.fr/setup>) et optionnellement vos
   `OTA_HOSTNAME` / `OTA_PASSWORD` pour les futurs flashs réseau.

   > ⚠️ Depuis la v2.3.0, **`secrets.h` ne contient PLUS** les
   > credentials WiFi (qui sont saisis via le portail captif au premier
   > boot). Cf. `secrets.h.example` pour la liste exacte.

8. *Croquis → Téléverser*.

---

## 3. Premier boot — portail captif

Au tout premier boot après un flash (ou si l'EEPROM ne contient pas de
credentials WiFi enregistrés), le firmware ouvre un point d'accès :

```
SSID : SQM-Setup-XXXXXX     (pas de mot de passe)
Adresse du portail : http://192.168.4.1
```

Où `XXXXXX` = 6 derniers chiffres hexadécimaux du chip ID de votre
NodeMCU (visible aussi sur l'OLED durant ce mode).

### Procédure de configuration

1. **Depuis un téléphone** (le plus pratique), aller dans les
   paramètres WiFi → se connecter à `SQM-Setup-XXXXXX`.
2. **Le portail captif s'ouvre automatiquement** sur Android/iOS (une
   notification *"Se connecter à ce réseau"* apparaît). Sinon, ouvrir
   un navigateur et aller sur **<http://192.168.4.1>**.
3. Cliquer sur **"Configure WiFi"**. Quatre champs à remplir :

   | Champ | Description | Obligatoire |
   |---|---|---|
   | SSID (principal) | Votre WiFi domestique (sélectionnable depuis le scan automatique) | ✅ |
   | Mot de passe (principal) | WPA/WPA2 de votre WiFi | ✅ |
   | **Nom de la station** | Identifiant unique de votre sonde (ex: `SQM-Maison`, `SQM-PicDuMidi`). Par défaut auto-généré au format `SQM-XXXXXX`. | ✅ |
   | **WiFi de secours - SSID** | Réseau de fallback en cas d'échec du principal (ex: hotspot téléphone, résidence secondaire) | ❌ (facultatif) |
   | **WiFi de secours - mot de passe** | WPA/WPA2 du réseau de secours | ❌ (facultatif) |

4. **Sauvegarder** → la sonde redémarre, se connecte à votre WiFi
   principal, et commence immédiatement à pousser des mesures vers le
   backend.

### Reconfiguration sans accès physique : Double Reset

Pour les sondes installées dans un boîtier scellé en extérieur, plus
besoin d'ouvrir pour reflasher si vous voulez changer le WiFi :

1. Couper le courant de la sonde (interrupteur ou débrancher l'USB).
2. Rallumer → laisser démarrer **2 secondes**.
3. **Re-couper le courant DANS LES 5 SECONDES** suivantes (avant que le
   firmware n'ait fini son boot complet).
4. Rallumer → le firmware détecte le double power-cycle et **ouvre à
   nouveau le portail captif** `SQM-Setup-XXXXXX`.
5. Reconfigurer comme à l'étape précédente.

> Comment ça marche : le firmware utilise la **RTC memory** de l'ESP8266
> (qui survit à un soft reset mais pas à une coupure d'alimentation
> > ~5 s) pour stocker un flag "first boot". Si ce flag est encore
> présent au boot suivant, on en déduit qu'un double reset s'est produit
> dans la fenêtre de temps imposée. Implémenté via la lib
> [`ESP_DoubleResetDetector`](https://github.com/khoih-prog/ESP_DoubleResetDetector).

### Comportement au boot

```
1. WiFi principal (depuis EEPROM) ? ─── Connexion réussie ?
                                                   ├── oui → push HTTPS OK
                                                   └── non → étape 2
2. WiFi de secours (depuis EEPROM, si configuré) ? Connexion réussie ?
                                                   ├── oui → push HTTPS OK
                                                   └── non → étape 3
3. Portail captif SQM-Setup-XXXXXX (5 min timeout) → étape 1
```

Si les trois étapes échouent (par exemple : aucun WiFi à portée, et
personne ne configure le portail), la sonde continue de mesurer en
local (OLED reste actif) mais ne push pas. Au prochain reboot ou DRD,
elle retente.

---

## 4. Comment ça fonctionne

### Deux modes, sélectionnés par le bouton mode (`ModePin`)

**Mode normal (bouton NON pressé) :**
* Lit le TSL2591 (`mpsas`, `dmpsas`), le BME280 (T/H/P), le GPS
  (lat/lng/alt/sat).
* Affiche sur l'OLED.
* Toutes les ~10 s (5 cycles de 2 s), si le Wi-Fi est connecté, envoie
  une requête HTTPS GET à :
  ```
  https://sqm.quentin-astro.fr/api/sqm_push?
      ID=<NomDeLaStation>&KEY=<sensor_key>
    & T=<temp °C>&H=<hum %>&P=<pression hPa>
    & S=<mpsas>&D=<erreur>&V=<batterie V>&Vpct=<batterie %>
    & L=<lux>
    & Alt=<alt GPS m>&Lat=<lat GPS>&Lon=<lng GPS>     (si GPS verrouillé)
  ```
  Le TLS est utilisé mais sans validation de certificat
  (`setInsecure()`) car l'ESP8266 n'a pas assez de RAM pour valider une
  chaîne CA complète. Les données sont quand même chiffrées sur le
  réseau.

  `<NomDeLaStation>` = la valeur saisie dans le portail captif (par
  défaut `SQM-XXXXXX`).

**Mode USB (bouton pressé) :**
Implémente le protocole série compatible Unihedron (`i`, `r`, `u`,
`w` *(étendu avec la météo)*, `g`, `z…` commandes de calibration,
`A50/A51/A5d/A5e/A5`). Liste complète des commandes dans `SQM_pro.ino`.

### Pages OLED

1. Première page : bannière de boot (version, S/N, état TSL/BME)
2. Page calibration : offsets stockés (SQM, température, contraste auto)
3. Page mesures : date/heure UT, magnitude, température, humidité,
   pression, altitude, nombre de satellites, latitude/longitude
4. Page d'attente (GPS pas encore verrouillé / mode USB)
5. Page OTA : barre de progression pendant un téléversement réseau

### Modes de mise à jour et d'alimentation

| Réglage `Config.h`               | Comportement                                                                       |
|----------------------------------|------------------------------------------------------------------------------------|
| `OTA_ON` (par défaut)            | Le NodeMCU est joignable par Arduino IDE en réseau pour reflash sans câble USB.   |
| `DEEP_SLEEP_ON` (optionnel)      | Mode batterie longue durée : ~20 µA en sommeil, autonomie typique ~30 j sur 2000 mAh. |
| `NIGHT_ONLY_PUSH_ON` (par défaut) | Le push HTTPS est désactivé en plein jour (TSL2591 saturé). Évite de polluer le dashboard. Seuil ajustable via `NIGHT_THRESHOLD_MPSAS` (défaut 12.0 = crépuscule nautique). |

> 🔌 **OTA** et **deep-sleep** sont incompatibles : voyez
> **[DEPLOY.md §10.2 & §11](./DEPLOY.md)** pour les détails et le
> câblage GPIO16-RST nécessaire au réveil.
> 🌃 Mode **nuit/jour** : voir **[DEPLOY.md §12](./DEPLOY.md)**.

### Stockage en EEPROM

Le firmware utilise les 192 premiers octets de l'EEPROM virtuelle ESP8266
(zone protégée en flash) pour persister :

| Offset | Taille | Contenu                                         |
|--------|--------|-------------------------------------------------|
| 0–29   | 30 B   | Calibrations historiques (offsets SQM, T°, contraste OLED) |
| 30     | 1 B    | Marker `'N'` indiquant la présence d'un nom de station |
| 31–62  | 32 B   | Nom de station personnalisé (chaîne ASCII)      |
| 63     | 1 B    | Marker `'W'` indiquant la présence d'un WiFi de secours |
| 64–95  | 32 B   | SSID du WiFi de secours                         |
| 96–159 | 64 B   | Mot de passe du WiFi de secours (WPA/WPA2)      |

Le SSID/mot de passe **principaux** sont gérés directement par
`ESP8266WiFi` dans sa propre zone flash (pas par notre EEPROM virtuelle).

---

## 5. Structure du dépôt

```
.
├── LICENSE
├── README.md
├── DEPLOY.md            ← guide de déploiement détaillé
├── CHANGELOG.md         ← historique des versions
├── FUTURE-IDEA-AMELIORATION.md  ← pistes d'évolution
├── .gitignore
├── .github/workflows/
│   └── build-firmware.yml ← CI : compile & publie le .bin (avec SENSOR_KEY injectée)
└── SQM_pro/
    ├── SQM_pro.ino       ← croquis principal (setup + loop + protocole USB)
    ├── Config.h          ← configuration utilisateur (flags : OLED, OTA, deep-sleep, mode nuit, EEPROM_SIZE)
    ├── secrets.h.example ← template de secrets.h (à copier en secrets.h, gitignoré)
    ├── Setup.h           ← brochage matériel / adresse I²C BME / police OLED
    ├── Validate.h        ← contrôles à la compilation
    ├── EEPROM.ino        ← persistance des calibrations + nom de station + WiFi secours
    ├── GPS.ino           ← helpers NEO-6
    ├── MyLib.ino         ← pages OLED + lecture BME280 + buzzer
    ├── WiFi.ino          ← maintenance WiFi + envoi HTTPS vers /api/sqm_push
    ├── WiFiPortal.ino    ← (v2.3.0+) portail captif + double reset detection
    ├── OTA.ino           ← support OTA (Over-The-Air firmware update)
    ├── SQM_TSL2591.h     ← pilote TSL2591 (en-tête)
    └── SQM_TSL2591.cpp   ← pilote TSL2591 (implémentation)
```

---

## 6. Dépannage rapide

| Symptôme                                            | Cause probable / solution                                            |
|-----------------------------------------------------|----------------------------------------------------------------------|
| Boot loop + buzzer rapide (erreur init)             | Câblage BME280 ou TSL2591 — vérifier l'adresse I²C dans `Setup.h`     |
| `OLED Err` sur le port série                        | Mauvais modèle d'afficheur — basculer `SH1106_ON` / `SSD1306_ON` dans Config.h |
| **Pas de réseau `SQM-Setup-XXXXXX` après flash**    | (v2.3+) Vérifier l'OLED : si une connexion WiFi est déjà persistée en flash, le firmware tente cette connexion d'abord. Faire un **double reset** pour forcer le portail. |
| **Le portail captif refuse de sauvegarder**         | (v2.3+) SSID > 32 caractères ou mot de passe > 64 caractères : non supporté par WPA2 / l'EEPROM. Vérifier la longueur. |
| Wi-Fi connecté mais pas de données sur le dashboard | `sensor_key` incorrecte (recompiler avec le bon secret), nom de station déjà utilisé par une autre sonde, ou push de jour bloqué par `NIGHT_ONLY_PUSH_ON` |
| `HTTPS Timeout !` dans le debug série               | Pare-feu/NAT bloque le port 443, ou DNS lent — réessaye 2-3 boucles  |
| Échec du handshake TLS                              | Pas assez de heap libre — désactiver `DEBUG_WIFI_ON` / `EXTENDET_PROTOCOL_ON` |
| Erreur de compilation : `WiFiClientSecure.h: No such file` | Mettre à jour le core ESP8266 (≥ 2.5.0)                       |
| **Erreur de compilation : `WiFiManager.h: No such file`**  | (v2.3+) Installer les libs `WiFiManager` (tzapu) et `ESP_DoubleResetDetector` (khoih-prog) via Arduino IDE Library Manager |
| Pas de verrouillage GPS                             | Commenter `#define GPS_ON` pour désactiver le GPS, ou améliorer la vue du ciel |

Activez le debug série en réglant `DEBUG_WIFI_ON` (et/ou `DEBUG_ON`,
`DEBUG_GPS_ON`) dans `Config.h`, puis ouvrez le moniteur série à
**74880 bauds** (ou 115200 si vous changez `SERIAL_BAUD`).

Sur ESP Web Tools (page `/flasher`), un bouton **"Logs & Console"** est
disponible après le flash pour afficher la sortie série directement
dans le navigateur (pratique pour diagnostiquer en mobilité).

Pour un guide de dépannage complet, voir **[DEPLOY.md](./DEPLOY.md)**.

---

## 7. Contribuer au projet coopératif

SQM Nightwatch est un projet de **science citoyenne** : chaque sonde
construite par un contributeur enrichit la base mondiale de mesures de
pollution lumineuse.

**Pour devenir contributeur :**

1. Construire la sonde matérielle (cf. [§1](#1-matériel)).
2. La flasher via [la page Flasher](https://sqm.quentin-astro.fr/flasher).
3. Configurer son WiFi via le portail captif (cf. [§3](#3-premier-boot--portail-captif)).
4. Choisir un nom de station unique et descriptif (ex: `SQM-Annecy`,
   `SQM-MontBlanc-Refuge-Goûter`, `SQM-Toulouse-Centre`).
5. Les mesures arrivent automatiquement sur le dashboard partagé.

**Suggestions de contribution code** : voir
[`FUTURE-IDEA-AMELIORATION.md`](./FUTURE-IDEA-AMELIORATION.md).

---

## 8. Licence

GPL-3.0 — voir `LICENSE`. Le pilote TSL2591 est distribué sous la
licence BSD originale d'Adafruit (préservée dans les en-têtes).

Copyright © 2025–2026 Quentin Dumont.
