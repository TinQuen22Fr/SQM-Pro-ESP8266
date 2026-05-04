# SQM Pro — Sky Quality Meter DIY pour ESP8266

Firmware pour un **Sky Quality Meter (SQM) DIY** construit autour d'un
ESP8266 (NodeMCU), d'un capteur de lumière TSL2591, d'un capteur météo
BME280, d'un afficheur OLED 128×64 (SH1106 ou SSD1306) et d'un module
GPS NEO-6 optionnel.

Ce firmware envoie automatiquement les mesures via HTTPS vers le tableau
de bord **[SQM Nightwatch](https://sqm.quentin-astro.fr)**
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

## 2. Configuration de l'IDE Arduino

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

Le pilote TSL2591 est inclus dans ce dépôt (`SQM_TSL2591.h/.cpp`),
rien à installer pour lui.

6. `ESP8266WiFi` et `WiFiClientSecure` sont fournis avec le core
   ESP8266 — pas d'installation supplémentaire.

---

## 3. Configurer votre capteur

Ouvrez **`Config.h`** et adaptez :

```cpp
// Wi-Fi (principal et secours optionnel)
const char* ssid     = "VotreSSIDWiFi";
const char* password = "VotreMotDePasseWiFi";
#define ALT_SSID_ON
const char* ssid2     = "SSIDDeSecours";
const char* password2 = "MotDePasseDeSecours";

// Identité du capteur (chaque appareil physique doit avoir un ID unique)
const char* SensorID    = "SQM-001";   // changez en SQM-002, SQM-003, ...
const char* sensor_key  = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

// Afficheur : choisissez exactement un seul
#define SH1106_ON      // dalle 1,3"
#define SSD1306_OFF    // dalle 0,96"
```

La `sensor_key` est générée depuis l'onglet **Configuration** du
[tableau de bord SQM Nightwatch](https://sqm.quentin-astro.fr).
**Une même clé peut être utilisée pour plusieurs capteurs physiques** :
il suffit de donner à chacun un `SensorID` distinct (par exemple
`SQM-001`, `SQM-002`, …). Le backend stocke chaque flux séparément
sous `device_id`.

---

## 4. Flasher

1. Brancher le NodeMCU en USB.
2. *Outils → Port* → sélectionner le bon COM / tty.
3. *Croquis → Téléverser*.

Tailles de compilation/flash typiques sur NodeMCU (ESP-12E, 4 Mo flash) :

```
Le croquis utilise ~370 Ko (35 %) de l'espace de stockage du programme.
Les variables globales utilisent ~33 Ko (40 %) de la mémoire dynamique.
```

---

## 5. Comment ça fonctionne

### Deux modes, sélectionnés par le bouton mode (`ModePin`)

**Mode normal (bouton NON pressé) :**
* Lit le TSL2591 (`mpsas`, `dmpsas`), le BME280 (T/H/P), le GPS
  (lat/lng/alt/sat).
* Affiche sur l'OLED.
* Toutes les ~10 s (5 cycles de 2 s), si le Wi-Fi est connecté, envoie
  une requête HTTPS GET à :
  ```
  https://sqm.quentin-astro.fr/api/sqm_push?
      ID=<SensorID>&KEY=<sensor_key>
    & T=<temp °C>&H=<hum %>&P=<pression hPa>
    & S=<mpsas>&D=<erreur>&V=<batterie V>
    & L=<lux>
    & Alt=<alt GPS m>&Lat=<lat GPS>&Lon=<lng GPS>     (si GPS verrouillé)
  ```
  Le TLS est utilisé mais sans validation de certificat
  (`setInsecure()`) car l'ESP8266 n'a pas assez de RAM pour valider une
  chaîne CA complète. Les données sont quand même chiffrées sur le
  réseau.

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

---

## 6. Structure du dépôt

```
.
├── LICENSE
├── README.md
├── DEPLOY.md            ← guide de déploiement détaillé
├── CHANGELOG.md         ← historique des versions
├── FUTURE-IDEA-AMELIORATION.md  ← pistes d'évolution
├── .gitignore
└── SQM_pro/
    ├── SQM_pro.ino       ← croquis principal (setup + loop + protocole USB)
    ├── Config.h          ← configuration utilisateur (flags : OLED, OTA, deep-sleep, mode nuit)
    ├── secrets.h.example ← template de secrets.h (à copier en secrets.h, gitignoré)
    ├── Setup.h           ← brochage matériel / adresse I²C BME / police OLED
    ├── Validate.h        ← contrôles à la compilation
    ├── EEPROM.ino        ← persistance des calibrations et réglages d'affichage
    ├── GPS.ino           ← helpers NEO-6
    ├── MyLib.ino         ← pages OLED + lecture BME280 + buzzer
    ├── WiFi.ino          ← Wi-Fi STA + envoi HTTPS vers /api/sqm_push
    ├── OTA.ino           ← support OTA (Over-The-Air firmware update)
    ├── SQM_TSL2591.h     ← pilote TSL2591 (en-tête)
    └── SQM_TSL2591.cpp   ← pilote TSL2591 (implémentation)
```

---

## 7. Dépannage rapide

| Symptôme                                        | Cause probable / solution                                            |
|-------------------------------------------------|----------------------------------------------------------------------|
| Boot loop + buzzer rapide (erreur init)         | Câblage BME280 ou TSL2591 — vérifier l'adresse I²C dans `Setup.h`     |
| `OLED Err` sur le port série                    | Mauvais modèle d'afficheur — basculer `SH1106_ON` / `SSD1306_ON` dans Config.h |
| Wi-Fi connecté mais pas de données sur le dashboard | `sensor_key` incorrecte ou `SensorID` pas encore déclaré côté serveur |
| `HTTPS Timeout !` dans le debug série           | Pare-feu/NAT bloque le port 443, ou DNS lent — réessaye 2-3 boucles  |
| Échec du handshake TLS                          | Pas assez de heap libre — désactiver `DEBUG_WIFI_ON` / `EXTENDET_PROTOCOL_ON` |
| Erreur de compilation : `WiFiClientSecure.h: No such file` | Mettre à jour le core ESP8266 (≥ 2.5.0)                       |
| Pas de verrouillage GPS                         | Commenter `#define GPS_ON` pour désactiver le GPS, ou améliorer la vue du ciel |

Activez le debug série en réglant `DEBUG_WIFI_ON` (et/ou `DEBUG_ON`,
`DEBUG_GPS_ON`) dans `Config.h`, puis ouvrez le moniteur série à
**74880 bauds** (ou 115200 si vous changez `SERIAL_BAUD`).

Pour un guide de dépannage complet, voir **[DEPLOY.md](./DEPLOY.md)**.

---

## 8. Licence

GPL-3.0 — voir `LICENSE`. Le pilote TSL2591 est distribué sous la
licence BSD originale d'Adafruit (préservée dans les en-têtes).

Copyright © 2025 Quentin Dumont.
