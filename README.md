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

---

## 6. Structure du dépôt

```
.
├── LICENSE
├── README.md
├── DEPLOY.md            ← guide de déploiement détaillé
├── .gitignore
└── SQM_pro/
    ├── SQM_pro.ino       ← croquis principal (setup + loop + protocole USB)
    ├── Config.h          ← configuration utilisateur (Wi-Fi, SensorID, clé, ...)
    ├── Setup.h           ← brochage matériel / adresse I²C BME / police OLED
    ├── Validate.h        ← contrôles à la compilation
    ├── EEPROM.ino        ← persistance des calibrations et réglages d'affichage
    ├── GPS.ino           ← helpers NEO-6
    ├── MyLib.ino         ← pages OLED + lecture BME280 + buzzer
    ├── WiFi.ino          ← Wi-Fi STA + envoi HTTPS vers /api/sqm_push
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
