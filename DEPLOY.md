# Déploiement du firmware SQM Pro sur ESP8266 (NodeMCU)

Ce document décrit, pas à pas, comment récupérer ce dépôt, installer les
dépendances Arduino, configurer le capteur pour votre compte **SQM
Nightwatch** (`magnitude-tracker`) et flasher un NodeMCU ESP8266.

> L'API cible est : `https://sqm.quentin-astro.fr/api/sqm_push` (HTTPS).

---

## 1. Prérequis

### Matériel
- ESP8266 **NodeMCU 1.0 (ESP-12E)**
- Capteur **TSL2591** (I²C 0x29)
- Capteur météo **BME280** (I²C 0x76 par défaut, 0x77 possible)
- Afficheur **OLED 128×64** — SH1106 1.3" **ou** SSD1306 0.96" (I²C, HW)
- Module **GPS NEO-6M** (optionnel) — RX=GPIO13, TX=GPIO15 via `SoftwareSerial`
- Bouton mode sur `GPIO2`, buzzer sur `GPIO12`, batterie lue sur `A0` (÷11)

### Logiciel
- **Arduino IDE ≥ 2.0** — <https://www.arduino.cc/en/software>
- **Git** pour cloner le dépôt
- Core **esp8266 by ESP8266 Community ≥ 3.1.x**

---

## 2. Cloner le dépôt

```bash
git clone https://github.com/TinQuen22Fr/SQM-Pro-ESP8266.git
cd SQM-Pro-ESP8266
```

Ouvrez ensuite `SQM_pro/SQM_pro.ino` avec Arduino IDE : tous les fichiers
du sous-dossier `SQM_pro/` s'affichent automatiquement dans des onglets
(c'est la structure attendue par Arduino IDE : dossier = sketch).

---

## 3. Installer le core ESP8266

1. *Fichier → Préférences* — dans « URL de gestionnaire de cartes
   supplémentaires » ajouter :
   ```
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
2. *Outils → Type de carte → Gestionnaire de cartes…* → installer
   **esp8266 by ESP8266 Community** (≥ 3.1.2).
3. *Outils → Type de carte → ESP8266 Boards → **NodeMCU 1.0 (ESP-12E Module)***
4. Paramètres recommandés :
   - *Flash Size* : 4MB (FS:2MB OTA:~1019KB)
   - *CPU Frequency* : 80 MHz
   - *Upload Speed* : 115200

---

## 4. Installer les bibliothèques Arduino

*Outils → Gérer les bibliothèques…*, puis rechercher et installer :

| Bibliothèque                               | Version testée |
|--------------------------------------------|----------------|
| **Adafruit Unified Sensor** (Adafruit)     | ≥ 1.1.14       |
| **BMx280MI** (Gregor Christandl)           | ≥ 1.2.3        |
| **U8g2** (olikraus) — fournit `U8x8lib`    | ≥ 2.34.x       |
| **TinyGPSPlus** (Mikal Hart)               | ≥ 1.0.3        |

Le driver TSL2591 est déjà inclus dans le dépôt (`SQM_TSL2591.h/.cpp`),
rien à installer pour lui.

`ESP8266WiFi` et `WiFiClientSecure` font partie du core ESP8266 — pas
d'installation séparée.

---

## 5. Configurer votre capteur

Ouvrez **`SQM_pro/Config.h`** et ajustez :

```cpp
// --- Wi-Fi -------------------------------------------------------------
const char* ssid     = "VotreSSIDWiFi";
const char* password = "VotreMotDePasseWiFi";

// (optionnel) Wi-Fi secondaire si le premier échoue
#define ALT_SSID_ON
const char* ssid2     = "SSIDSecondaire";
const char* password2 = "MotDePasseSecondaire";

// --- Identité du capteur -----------------------------------------------
// Un ID UNIQUE par appareil physique. Même clé pour tous vos capteurs.
const char* SensorID   = "SQM-001";       // "SQM-002", "SQM-003", ...
const char* sensor_key = "votre-cle-API"; // depuis l'onglet Configuration
                                          // de sqm.quentin-astro.fr

// --- Afficheur : un seul activé ----------------------------------------
#define SH1106_ON      // 1.3"  (par défaut)
#define SSD1306_OFF    // 0.96" (à activer à la place si c'est votre écran)
```

### Plusieurs capteurs sur le même compte
L'API accepte plusieurs `SensorID` distincts avec la **même**
`sensor_key`. Pour déployer un 2e appareil, **il suffit de changer**
`SensorID = "SQM-002"` et de reflasher. Les deux flux apparaîtront
comme des `device_id` séparés dans le dashboard.

---

## 6. Compiler et flasher

1. Branchez le NodeMCU en USB.
2. *Outils → Port* → sélectionnez le port COM/tty correspondant.
   (Sous Windows, il faut parfois le driver CH340 ou CP2102 selon la carte.)
3. Cliquez sur **Upload** (flèche →).
4. La compilation prend ~30–60 s. La taille typique :
   ```
   Sketch  : ~370 KB  (35 % de la flash)
   Globals : ~33 KB   (40 % du RAM)
   ```
5. Quand Arduino IDE affiche *Hard resetting via RTS pin…*, c'est bon —
   ouvrez le **Moniteur série à 74880 bauds**.

---

## 7. Première mise en route

Au démarrage, le NodeMCU affiche sur l'OLED :

1. **Page boot** : version, numéro de série, état TSL2591/BME280.
2. **Page calibration** : offsets SQM / température chargés de l'EEPROM.
3. Quelques secondes plus tard, connexion Wi-Fi puis **page mesures**
   avec magnitude, température, humidité, pression, GPS.

Sur le moniteur série (si `DEBUG_WIFI_ON` est défini dans `Config.h`) :

```
Wait for WiFi.....
WiFi connected
IP address: 192.168.1.42

SQM: 20.54+-0.02 mas^2
Temp: 15 C
Humidity: 72 %
Pressure: 1013 hPa
Lux: 0.0012
Battery: 4.15
HTTP/1.1 200 OK
{"status":"ok","stored":{...,"device_id":"SQM-001","source":"http_get"},"total":N}
closing connection
```

Allez sur <https://sqm.quentin-astro.fr> : votre mesure devrait
apparaître dans le tableau de bord dans les 30 s (cadence de
rafraîchissement du dashboard).

---

## 8. Modes de fonctionnement

Le bouton **Mode** (GPIO2) bascule entre deux modes :

| Bouton   | Mode               | Comportement                                            |
|----------|--------------------|---------------------------------------------------------|
| Relâché  | **Normal**         | OLED actif + envoi HTTPS toutes les ~10 s au dashboard  |
| Pressé   | **USB / Unihedron**| Réponses au protocole série Unihedron-compatible        |

Commandes USB supportées : `i` (info), `r` (reading), `u` (unaveraged),
`w` (extended + weather), `g` (config), `z…` (calibration), `A5…`
(contraste/dimmer). Détails dans `SQM_pro.ino`.

---

## 9. Dépannage

| Symptôme                                        | Cause / solution                                                                |
|-------------------------------------------------|----------------------------------------------------------------------------------|
| Boot loop + buzzer rapide au démarrage          | Erreur init BME280 ou TSL2591 → câblage I²C, vérifier adresse dans `Setup.h`    |
| `OLED Err` sur le moniteur série                | Mauvais modèle d'afficheur → bascule entre `SH1106_ON` et `SSD1306_ON`           |
| Wi-Fi connecté mais 0 donnée sur le dashboard   | `sensor_key` incorrecte, ou TLS cassé (voir ligne suivante)                     |
| `connection failed` ou `HTTPS Timeout !`        | DNS lent / firewall port 443 / désactivez `EXTENDET_PROTOCOL_ON` pour gagner de la RAM |
| Compile error: `WiFiClientSecure.h: No such file` | Mettre à jour le core ESP8266 (≥ 2.5.0)                                         |
| GPS jamais synchronisé                          | Vue du ciel insuffisante ou câblage RX/TX inversé — ou commentez `#define GPS_ON` |
| `undefined reference to BMx280I2C`              | Lib `BMx280MI` non installée (voir §4)                                          |
| Capteur visible mais `mag` négatif              | Réajuster `SQM_CAL_OFFSET` dans `Config.h` ou via commande série `zcal1<val>`   |

Activez le debug complet en mettant dans `Config.h` :
```cpp
#define DEBUG_ON
#define DEBUG_GPS_ON
#define DEBUG_WIFI_ON
```

---

## 10. Mise à jour du firmware

```bash
cd SQM-Pro-ESP8266
git pull
# puis re-upload depuis Arduino IDE
```

Pour une future OTA (Over-The-Air) sans câble USB, ajoutez
`ArduinoOTA.begin()` dans `setup()` et lancez `ArduinoOTA.handle()` dans
`loop()`. L'ESP8266 annoncera alors un port réseau dans *Outils → Port*.

---

## 11. Sécurité

- **Ne commitez jamais** votre vraie `sensor_key` sur un dépôt public.
  Pour cela, créez un fichier `SQM_pro/secrets.h` local :
  ```cpp
  #ifndef SECRETS_H
  #define SECRETS_H
  #undef  WIFI_SSID
  #undef  WIFI_PWD
  #define WIFI_SSID  "MyRealSSID"
  #define WIFI_PWD   "MyRealPassword"
  #define SQM_API_KEY "..."
  #endif
  ```
  puis ajoutez `secrets.h` au `.gitignore` (déjà prévu en commentaire)
  et référencez ces macros dans `Config.h`.
- Le TLS utilisé par le firmware appelle `setInsecure()` (pas de
  validation de certificat), car l'ESP8266 manque de RAM pour valider
  la chaîne complète. La connexion reste **chiffrée**, mais pas
  authentifiée côté serveur.
- Le token GitHub que vous m'avez fourni pour le premier push doit
  être **révoqué** :
  <https://github.com/settings/tokens>

---

## 12. Références

- API magnitude-tracker : <https://sqm.quentin-astro.fr>
- Endpoint ingestion : `POST|GET /api/sqm_push`
- Lib TSL2591 origine : <https://github.com/gshau/SQM_TSL2591>
- PCB / wiring : <https://easyeda.com/hujer.roman/sqm-hr>
- TinyGPSPlus : <http://arduiniana.org/libraries/tinygpsplus/>
- Unihedron SQM-LE serial protocol :
  <http://unihedron.com/projects/sqm-le/commands.html>

---

Copyright © 2025 Quentin Dumont — GPL-3.0.
