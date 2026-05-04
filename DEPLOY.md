# Déploiement du firmware SQM Pro sur ESP8266 (NodeMCU)

Ce document décrit, pas à pas, comment récupérer ce dépôt, installer les
dépendances Arduino, configurer le capteur pour votre compte **SQM
Nightwatch** (`magnitude-tracker`) et flasher un NodeMCU ESP8266.

> L'API cible est : `https://sqm.quentin-astro.fr/api/sqm_push` (HTTPS).

---

## 1. Prérequis

### Matériel
- ESP8266 **NodeMCU 1.0 (ESP-12E)** ou **NodeMCU v3 LoLin** (les deux variantes
  utilisent le même module ESP-12E ; dans Arduino IDE, sélectionnez dans tous
  les cas la carte *NodeMCU 1.0 (ESP-12E Module)*).
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

> 🔐 **Bonne pratique : sortir vos secrets dans `secrets.h`**
>
> Pour ne pas avoir à ressaisir vos identifiants à chaque `git pull` (et
> ne JAMAIS pousser vos vrais SSID/mots de passe sur GitHub), créez un
> fichier `SQM_pro/secrets.h` à partir du template :
>
> ```bash
> cd SQM-Pro-ESP8266
> cp SQM_pro/secrets.h.example SQM_pro/secrets.h
> nano SQM_pro/secrets.h     # éditez avec vos vrais identifiants
> ```
>
> `secrets.h` est listé dans `.gitignore`, donc :
> - Il ne sera **jamais** poussé sur le repo public
> - Les `git pull` futurs **ne l'écraseront pas**
>
> `Config.h` détecte automatiquement la présence de `secrets.h` (via
> `__has_include`) et utilise vos valeurs si le fichier existe ; sinon
> il utilise des placeholders qui permettent au moins la compilation.

Une fois `secrets.h` créé, ajustez (au choix) :
- les flags du firmware dans `Config.h` (debug, OLED, OTA, deep-sleep,
  mode nuit) - ceux-là peuvent rester versionnés sur Git
- les vrais identifiants dans `secrets.h` (Wi-Fi, clé API, mot de passe
  OTA, hostname) - jamais versionnés

### Contenu typique de `secrets.h`

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// Wi-Fi (principal et secours)
#define WIFI_SSID         "MonSSID"
#define WIFI_PASSWORD     "MonMotDePasse"
#define WIFI_SSID_ALT     "SSIDDeSecours"
#define WIFI_PASSWORD_ALT "MotDePasseDeSecours"

// Identité du capteur (chaque appareil = un ID unique)
#define SENSOR_ID    "SQM-001"
#define SENSOR_KEY   "votre-vraie-cle-API"

// OTA
#define OTA_HOSTNAME "sqm-pro-001"
#define OTA_PASSWORD "MonMotDePasseFort_42!"

#endif
```

### Flags non-secrets restant dans `Config.h`

Ouvrez **`SQM_pro/Config.h`** et ajustez uniquement les flags
fonctionnels (qui peuvent rester sur GitHub) :

```cpp
// --- Afficheur : un seul activé ----------------------------------------
#define SH1106_ON      // 1.3"  (par défaut)
#define SSD1306_OFF    // 0.96" (à activer à la place si c'est votre écran)

// --- GPS ---------------------------------------------------------------
#define GPS_ON         // commentez si pas de module GPS

// --- OTA / deep-sleep / mode nuit (cf. sections 10-12) ----------------
#define OTA_ON
#define DEEP_SLEEP_OFF
#define NIGHT_ONLY_PUSH_ON
#define NIGHT_THRESHOLD_MPSAS 12.0f

// --- Bouton mode USB (cf. section 9) ----------------------------------
#define USB_MODE_ON    // PCB SQM-HR avec interrupteur de façade
// #define USB_MODE_OFF  // NodeMCU nu sans interrupteur
```

> 💡 Les identifiants Wi-Fi, l'API key et le mot de passe OTA sont
> désormais dans `secrets.h` (cf. encadré ci-dessus). Vous n'avez plus
> jamais à les remettre dans `Config.h`.

### Plusieurs capteurs sur le même compte
L'API accepte plusieurs `SENSOR_ID` distincts avec la **même**
`SENSOR_KEY`. Pour déployer un 2e appareil :

1. Cloner le repo sur le 2e PC (ou copier les sources)
2. Créer un `secrets.h` local en changeant juste `SENSOR_ID` :
   ```cpp
   #define SENSOR_ID    "SQM-002"
   #define OTA_HOSTNAME "sqm-pro-002"
   // ... reste identique
   ```
3. Compiler et flasher.

Les deux flux apparaîtront comme des `device_id` séparés dans le
dashboard.

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

L'**interrupteur de façade** du SQM Pro est un **interrupteur SPDT
3 positions center-off** câblé ainsi :
- Plot central : **GND**
- Plot 1 : **D3 (GPIO0)**
- Plot 2 : **D5 (GPIO14)**

Selon sa position, le firmware (et le bootloader ESP) bascule entre
trois comportements :

| Position interrupteur | État GPIO0 (D3) | État GPIO14 (D5) | Mode actif |
|---|---|---|---|
| **Centre** (par défaut) | HIGH (pull-up) | HIGH (pull-up) | 🌌 **Mode normal** : OLED + push HTTPS toutes les ~10 s au dashboard SQM Nightwatch |
| Côté **D5** | HIGH | LOW | 🔌 **Mode USB Unihedron** : protocole série compatible SQM-LE (commandes `i`, `r`, `u`, `w`, `g`, `z…`, `A5…`) |
| Côté **D3** *(au démarrage)* | LOW | HIGH | ⚡ **Mode flash** : l'ESP8266 entre en mode téléversement firmware (USB) — pour reflasher sans toucher Arduino IDE |

> 💡 Le mode flash est géré par le **bootloader ESP8266 lui-même**
> (lecture de GPIO0 au reset). Le firmware ne lit donc **que GPIO14**
> pour distinguer mode normal ↔ mode USB Unihedron.

> ⚠️ **Au démarrage, mettez l'interrupteur en CENTRE ou côté D5.**
> Si vous démarrez en position D3, l'ESP entre en mode flash et
> l'OLED reste éteint — ce qui peut surprendre. Il suffit alors de
> repositionner l'interrupteur (centre ou D5) et d'appuyer sur le
> bouton **RESET**.

Commandes USB Unihedron supportées : `i` (info unité), `r` (reading),
`u` (unaveraged), `w` (extended weather, étendu non standard),
`g` (config), `z…` (calibration), `A5…` (contraste/dimmer OLED).
Détails dans `SQM_pro.ino`.

---

## 9. Dépannage

| Symptôme                                        | Cause / solution                                                                |
|-------------------------------------------------|----------------------------------------------------------------------------------|
| Boot loop + buzzer rapide au démarrage          | Erreur init BME280 ou TSL2591 → câblage I²C, vérifier adresse dans `Setup.h`    |
| `OLED Err` sur le moniteur série                | Mauvais modèle d'afficheur → bascule entre `SH1106_ON` et `SSD1306_ON`           |
| Wi-Fi connecté mais 0 donnée sur le dashboard   | `sensor_key` incorrecte, ou TLS cassé (voir ligne suivante)                     |
| `connection failed` ou `HTTPS Timeout !`        | DNS lent / firewall port 443 / désactivez `EXTENDET_PROTOCOL_ON` pour gagner de la RAM |
| Compile error: `WiFiClientSecure.h: No such file` | Mettre à jour le core ESP8266 (≥ 2.5.0)                                         |
| GPS jamais synchronisé                          | Voir les 3 lignes ci-dessous selon le message OLED                              |
| **OLED affiche `GPS no wire!`**                 | Le firmware ne reçoit AUCUNE trame série du GPS. Vérifier : GPS **TXD** sur **D4 (GPIO2)**, alim **3,3 V** (pas 5 V), GND commun, et LED rouge du module qui doit s'allumer dès qu'il est alimenté. |
| **OLED affiche `GPS wait Sat:0`**              | Le GPS envoie bien des NMEA mais n'a pas encore vu de satellite. **Approcher d'une fenêtre** ou sortir, attendre 30-90 s pour un cold start. La LED rouge du module **clignote** dès qu'il y a un fix. |
| **OLED affiche `GPS wait Sat:1-3`**             | Pas assez de satellites (il en faut ≥ 4 pour fixer la position). Patience + meilleure vue du ciel. |
| **L'OLED oscille entre "Wait USB data" et la page mesures** | Vous êtes sur un NodeMCU "nu" (sans la PCB SQM-HR avec son interrupteur de façade) : GPIO2 (ModePin) flotte entre HIGH et LOW. **Solution** : dans `Config.h`, commentez `#define USB_MODE_ON` et décommentez `#define USB_MODE_OFF`. Le mode USB/Unihedron sera alors désactivé et le firmware restera toujours en mode normal. |
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

### 10.1. Avec un câble USB

```bash
cd SQM-Pro-ESP8266
git pull
# puis re-upload depuis Arduino IDE
```

### 10.2. Sans câble (OTA — Over-The-Air) ✨

Le firmware embarque le support OTA (`ArduinoOTA`). Une fois le NodeMCU
flashé une **première fois en USB** avec `OTA_ON` défini dans
`Config.h`, vous pouvez le re-flasher **par le réseau Wi-Fi**, sans
câble, depuis Arduino IDE.

#### 10.2.1. Pré-requis

| Pré-requis                                  | Pourquoi                                                 |
|---------------------------------------------|----------------------------------------------------------|
| Un premier flash USB avec `OTA_ON`           | Le firmware OTA doit être en place sur la puce            |
| Le PC sur le **même réseau Wi-Fi** que le capteur | OTA passe en mDNS/UDP local (pas routé sur Internet)  |
| Sur Windows : **Bonjour Print Services** installé (souvent fourni avec iTunes) | Découverte mDNS du nom `sqm-pro-001.local` |
| Sur Linux : `avahi-daemon` actif (`sudo systemctl status avahi-daemon`) | Idem |
| Sur macOS : rien à installer, Bonjour est natif | Idem |
| Sketch + variables qui prennent **moins de ~50 % de la flash** | OTA stocke le nouveau firmware avant de basculer dessus |

> 💡 La règle des 50 % : ESP8266 doit pouvoir tenir l'**ancien** ET le
> **nouveau** firmware en flash pendant le transfert. Vérifiez à la
> compilation : *Le croquis utilise XXXXX octets (YY %) de l'espace de
> stockage du programme*. Si YY ≥ 50, désactivez `EXTENDET_PROTOCOL_ON`
> ou `DEBUG_*_ON` pour gagner de la place.

#### 10.2.2. Configuration dans `Config.h`

```cpp
// Activez OTA
#define OTA_ON

// Nom unique du capteur sur le LAN (visible dans Arduino IDE)
const char* ota_hostname = "sqm-pro-001";   // sqm-pro-002, sqm-pro-003, ...

// Mot de passe pour autoriser un upload OTA (CHANGEZ-LE !)
const char* ota_password = "monMotDePasseFort_42!";
```

> 🔐 **Important** : changez `ota_password` avant tout déploiement
> réel. Sur le LAN, n'importe quel appareil peut tenter un upload
> OTA — seul ce mot de passe l'empêche.

Re-flashez **une fois en USB** pour que la nouvelle config (hostname +
mot de passe) soit prise en compte.

#### 10.2.3. Première mise à jour OTA — pas à pas

1. **Vérifiez que le NodeMCU est connecté au Wi-Fi.**
   Ouvrez le moniteur série à 74880 bauds, vous devez voir :
   ```
   WiFi connected
   IP address: 192.168.1.42
   OTA ready, hostname: sqm-pro-001
   ```

2. **Ouvrez Arduino IDE** sur le même PC, sur le même Wi-Fi.

3. **Menu *Outils → Port***. Au bout de 5 à 10 secondes, vous devez
   voir apparaître un nouveau bloc :
   ```
   Ports série
     /dev/ttyUSB0
     /dev/ttyACM0
   Ports réseau
     sqm-pro-001 at 192.168.1.42 (Generic ESP8266 module)   ← cliquer ici
   ```

4. **Sélectionnez `sqm-pro-001 at 192.168.1.42`** (le port réseau).

5. **Croquis → Téléverser** (ou raccourci `Ctrl + U` / `Cmd + U`).

6. Arduino IDE compile, puis ouvre une **petite fenêtre demandant le
   mot de passe** :
   ```
   ┌──────────────────────────────────────┐
   │  Type board password to upload a     │
   │  new sketch:                         │
   │  [ ************************    ]     │
   │                  [ Annuler ] [ OK ]  │
   └──────────────────────────────────────┘
   ```
   Entrez `ota_password` et validez.

7. **L'upload commence**. L'OLED du capteur affiche :
   ```
   OTA Update
   Type: sketch
   Do NOT unplug!
                            32 %
   ```
   Le PC affiche la même progression dans la console Arduino.

8. **À 100 %**, le capteur émet un bip court de 100 ms, l'OLED affiche
   *Done. Reboot.*, le NodeMCU redémarre seul et reprend ses mesures.
   Côté Arduino IDE, vous voyez :
   ```
   Uploading...
   100% [=========================================] 372 KB
   Téléversement terminé
   ```

#### 10.2.4. Dépannage OTA

| Symptôme                                         | Cause / solution                                                  |
|--------------------------------------------------|--------------------------------------------------------------------|
| Le port réseau `sqm-pro-001` n'apparaît jamais   | mDNS/Bonjour pas installé (Windows : installer **Bonjour Print Services**). Sinon, ajouter manuellement le port avec son IP : *Outils → Port → Saisir un port réseau personnalisé* (Arduino IDE 2.x) |
| Le port apparaît, l'upload démarre mais bloque à 0 % | Pare-feu Windows / antivirus bloque le port UDP 3232. Autorisez Arduino IDE dans le pare-feu |
| `Authentication Failed`                          | Mauvais `ota_password`. Vérifiez la valeur exacte dans `Config.h` |
| `No response from device` / `[ERROR]: No Answer` | Le capteur n'est pas sur le même sous-réseau que le PC, ou `OTA_ON` n'est pas défini, ou Wi-Fi pas encore connecté |
| OTA démarre mais bascule en `Receive Failed`     | Coupure Wi-Fi pendant le transfert : recommencez                  |
| OTA réussit mais le capteur reboucle au boot     | Le nouveau sketch est trop gros (≥ 50 % de la flash) → reflash USB d'urgence |
| Tout marche en USB mais pas en OTA               | Ouvrez le moniteur série pendant que vous lancez l'upload OTA. Si vous ne voyez RIEN, le firmware n'est pas en train de tourner — reflash USB |

#### 10.2.5. OTA avec plusieurs capteurs

Si vous avez plusieurs capteurs (`SQM-001`, `SQM-002`, …), donnez à
chacun un `ota_hostname` distinct dans son `Config.h` :

```cpp
// Capteur 1
const char* SensorID    = "SQM-001";
const char* ota_hostname = "sqm-pro-001";

// Capteur 2
const char* SensorID    = "SQM-002";
const char* ota_hostname = "sqm-pro-002";
```

Tous apparaîtront en parallèle dans *Outils → Port → Ports réseau* et
vous pouvez choisir lequel mettre à jour.

#### 10.2.6. Désactiver l'OTA (production sans accès distant)

Si votre capteur est dans un endroit physiquement sécurisé et que vous
ne voulez pas exposer la moindre surface d'attaque sur le LAN,
commentez simplement la ligne dans `Config.h` :

```cpp
// #define OTA_ON
```

et reflashez une dernière fois en USB. Le code OTA n'est alors plus
compilé du tout.

#### 10.2.7. Résumé visuel du flux

```
   ┌────────┐    Wi-Fi LAN    ┌────────────────────┐
   │   PC   │ ◄──────────────►│  NodeMCU SQM-001   │
   │Arduino │   mDNS + TCP    │  IP: 192.168.1.42  │
   │  IDE   │   port 8266     │  hostname:         │
   └────┬───┘                 │   sqm-pro-001      │
        │                     └─────────┬──────────┘
        │   1. Compile sketch            │
        │   2. Découvre par mDNS        │
        │   3. Demande password ────►    │  Vérifie ota_password
        │   4. Envoie firmware ─────►    │  Stocke en flash secondaire
        │   5. Attend ACK ◄──────────    │  Bascule + redémarre
        │                                │
        │                                └──► Bip court + OLED OK
```

> ⚠️ **Si vous activez `DEEP_SLEEP_ON`**, l'OTA devient quasiment
> inutilisable car la radio Wi-Fi est éteinte la plupart du temps. Pour
> mettre à jour un capteur en deep-sleep, il y a deux options :
>
> 1. Re-flasher temporairement avec `DEEP_SLEEP_OFF` (USB), pousser la
>    nouvelle version par OTA puis remettre `DEEP_SLEEP_ON`.
> 2. Garder l'USB branché : pendant la fenêtre où le chip est éveillé
>    (~10 s par cycle de `SLEEP_SEC`), Arduino IDE peut joindre l'OTA
>    si le délai d'attente est augmenté — peu pratique.

---

## 11. Mode batterie longue durée (deep-sleep) 🌙

Le firmware peut faire passer le NodeMCU en deep-sleep entre deux
mesures pour fonctionner sur batterie pendant plusieurs jours.

### 11.1. Modification matérielle requise

⚠️ **Sans cette modification, le NodeMCU ne se réveillera jamais.**

Reliez **GPIO16 (broche D0)** au signal **RST** du module ESP8266 avec
une **résistance de 470 Ω en série** (ou une diode Schottky en
inverse) :

```
   D0 (GPIO16) ──[ 470 Ω ]── RST
```

La résistance/diode permet au programmeur USB de continuer à tirer RST
à la masse pour reflasher le module.

### 11.2. Activer le mode deep-sleep

Dans `Config.h` :

```cpp
// #define DEEP_SLEEP_OFF       // (commentez cette ligne)
#define DEEP_SLEEP_ON           // décommentez celle-ci

#define SLEEP_SEC 300           // 5 minutes entre deux pushs (300 s)
```

Et **commentez** `OTA_ON` (la radio sera éteinte la plupart du temps,
l'OTA est inutile) :

```cpp
// #define OTA_ON
```

Reflashez le NodeMCU une dernière fois en USB.

### 11.3. Comportement

À chaque réveil :

1. Wi-Fi se connecte (~3-10 s)
2. Le firmware tente d'avoir un fix GPS (5 s, best-effort)
3. Mesure TSL2591 + BME280
4. Push HTTPS vers `/api/sqm_push`
5. `ESP.deepSleep(SLEEP_SEC * 1e6)` — chip à ~20 µA pendant `SLEEP_SEC`

Pendant le sommeil :
- L'OLED est éteint
- Le bouton mode est ignoré
- L'OTA est inactive

### 11.4. Autonomie indicative

Avec une batterie LiPo 3,7 V / 2000 mAh et `SLEEP_SEC = 300` :

| Phase                    | Durée    | Courant moyen |
|--------------------------|----------|---------------|
| Réveil + Wi-Fi + push    | ~10 s    | ~80 mA        |
| Deep-sleep               | ~290 s   | ~20 µA        |

**Consommation moyenne ≈ 2,7 mA** → autonomie théorique ≈ **30 jours**
(à dériver selon T° ambiante, vieillissement de la batterie, etc.).

Si vous voulez plus d'autonomie, augmentez `SLEEP_SEC` (par ex. 900 =
15 minutes ⇒ ~3 mois).

---

## 12. Mode « nuit uniquement » (push intelligent) 🌃

Les mesures SQM ne sont **utiles que la nuit** : en plein jour, le
TSL2591 est saturé et la magnitude lue est dépourvue de sens. Polluer
le dashboard avec ces valeurs est inutile.

Le firmware embarque un détecteur **nuit/jour** qui filtre les pushs
vers l'API en fonction de la luminosité réellement mesurée.

### 12.1. Comment ça fonctionne

À chaque cycle de mesure :

```
   1. Mesure TSL2591 → magnitude (mpsas)
   2. Si mpsas < NIGHT_THRESHOLD_MPSAS  (jour)
        → on SAUTE le push HTTPS
        → l'OLED continue d'afficher la valeur live + indicateur "DAY"
        → en deep-sleep : on dort quand même SLEEP_SEC secondes
   3. Si mpsas ≥ NIGHT_THRESHOLD_MPSAS  (nuit)
        → push HTTPS normal
        → indicateur "NGT" sur l'OLED
```

Le détecteur s'appuie sur la mesure du capteur lui-même, donc :
- ✅ Pas besoin du GPS
- ✅ Pas besoin de calculer l'heure du coucher/lever du soleil
- ✅ Auto-adaptatif (un capteur sous bâche temporaire reste en mode nuit)

### 12.2. Configuration

Dans `Config.h` :

```cpp
// Activer le mode nuit-uniquement (par défaut)
#define NIGHT_ONLY_PUSH_ON

// Seuil de magnitude au-dessus duquel on considère qu'il fait nuit.
//   10.0  = crépuscule civil (-6°)   — début de soirée, ciel encore clair
//   12.0  = crépuscule nautique (-12°) — DÉFAUT, début de la nuit utile
//   13.0  = crépuscule astronomique (-18°) — nuit noire stricte
#define NIGHT_THRESHOLD_MPSAS 12.0f
```

> 💡 La valeur est **ajustable directement dans le code** sans librairie
> externe. Modifiez-la, recompilez (USB ou OTA), c'est tout.

### 12.3. Pour désactiver le filtre (toujours pousser)

Si vous voulez le comportement legacy (push même en plein jour, par
exemple pour tester la chaîne d'ingestion en pleine journée), commentez
la ligne :

```cpp
// #define NIGHT_ONLY_PUSH_ON
```

### 12.4. Indication visuelle sur l'OLED

L'angle supérieur droit de la page mesure affiche en permanence :

| Affichage | Signification                                        |
|-----------|------------------------------------------------------|
| `NGT`     | Nuit détectée → push HTTPS actif                     |
| `DAY`     | Jour détecté → push HTTPS désactivé (économie)       |

### 12.5. Cas d'usage typiques

| Contexte                                                   | Mode recommandé                              |
|------------------------------------------------------------|----------------------------------------------|
| Capteur **mobile** sur batterie 18650, sortie d'observation | Interrupteur physique on/off + `NIGHT_ONLY_PUSH_ON` en filet de sécurité |
| Capteur **fixe** sur secteur (observatoire perso)          | `DEEP_SLEEP_OFF` + `OTA_ON` + `NIGHT_ONLY_PUSH_ON` |
| Capteur **fixe** sur batterie/solaire en site distant      | `DEEP_SLEEP_ON` + `NIGHT_ONLY_PUSH_ON`       |

### 12.6. Conséquence pour les batteries 18650 mobiles 🔋

Pour les capteurs **portables sur batterie 18650**, la meilleure
solution reste un **interrupteur d'alimentation physique** entre la
batterie et le NodeMCU :

- 0 µA quand éteint (vs ~20 µA même en deep-sleep)
- Aucune décharge parasite quand le capteur dort dans son sac entre
  deux sorties d'observation
- Robuste, prévisible, sans surprise

Le mode `NIGHT_ONLY_PUSH_ON` agit alors comme **filet de sécurité** :
si vous oubliez d'éteindre le capteur le matin, il continuera à
fonctionner mais ne polluera pas votre dashboard avec des données de
journée.

---

## 13. Sécurité

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

## 14. Notes sur l'occupation mémoire (ESP8266) 🧠

L'ESP8266 répartit le code et les données dans plusieurs zones séparées.
Lors de la compilation, Arduino IDE affiche un rapport d'utilisation :

```
. Variables and constants in RAM (global, static), used 32280 / 80192 bytes (40%)
. Instruction RAM (IRAM_ATTR),                     used 63439 / 65536 bytes (96%)
. Code in flash (default, ICACHE_FLASH_ATTR),      used 417500 / 1048576 bytes (39%)
```

| Zone | Quoi | Limite | Marge typique | Critique ? |
|---|---|---|---|---|
| RAM | variables globales + statiques + heap | 80 Ko | 40 % utilisé | ✅ confortable |
| **IRAM** | code qui doit s'exécuter pendant que la flash est verrouillée (ISR Wi-Fi, TLS, SoftwareSerial GPS) | **64 Ko** | **96 % utilisé** | ⚠️ **serré** |
| Flash | code applicatif standard | 1 Mo | 39 % utilisé | ✅ OTA-compatible (< 50 %) |

### Pourquoi l'IRAM est si chargée

Trois fonctionnalités du firmware tirent fortement sur l'IRAM :
- **`WiFiClientSecure`** (TLS pour HTTPS) — gros consommateur incompressible
- **`ArduinoOTA`** — code OTA en partie en IRAM
- **`SoftwareSerial`** (GPS NEO-6) — bit-banging sous interruption → ~3 Ko IRAM à lui seul

### Comment libérer de l'IRAM si on dépasse

Si un jour vous ajoutez une feature et tombez sur l'erreur de link
`region 'iram1_0_seg' overflowed by N bytes`, voici les leviers, du
moins invasif au plus invasif :

| Levier | Gain typique | Inconvénient |
|---|---|---|
| Mettre tous les `DEBUG_*_OFF` dans `Config.h` (par défaut depuis v2.1.0) | ~1-2 Ko IRAM | Plus de logs sur le port série |
| Commenter `#define EXTENDET_PROTOCOL_ON` | ~1 Ko flash + un peu d'IRAM | Mode USB/Unihedron sans la commande `w` (météo étendue) |
| Brancher le GPS sur RX/TX matériels (GPIO3/GPIO1) au lieu de SoftwareSerial | **~3 Ko IRAM** | Conflit avec le port série de debug et le mode USB/Unihedron |
| Désactiver `OTA_ON` si vous ne flashez qu'en USB | ~1-2 Ko | Plus d'OTA |
| Désactiver `GPS_ON` si vous n'utilisez pas le GPS | ~3 Ko | Plus de localisation |

### Rapport d'occupation typique avec la config par défaut v2.1.x

```
RAM       : ~32 Ko / 80 Ko    (40 %)
IRAM      : ~63 Ko / 64 Ko    (96 %)   ⚠  marge ~2 Ko
Flash     : ~417 Ko / 1024 Ko (39 %)   ✓  OTA OK
```

Ce niveau d'IRAM est **fonctionnel mais sans réserve**. Si vous prévoyez
d'ajouter beaucoup de fonctionnalités (option C, mDNS étendu, Grafana,
LoRa, etc.), envisagez l'un des leviers ci-dessus.

> 💡 **Warnings Python à la compilation** : avec Python ≥ 3.12, le core
> ESP8266 3.1.2 affiche deux `SyntaxWarning: invalid escape sequence '\s'`
> dans `elf2bin.py`. Bug connu, corrigé dans le core 3.1.3+ ; sans aucun
> impact sur le firmware. Vous pouvez soit mettre à jour le core, soit
> patcher manuellement les deux occurrences `re.split('\s+', line)` en
> `re.split(r'\s+', line)` (préfixe `r` pour *raw string*), soit
> simplement ignorer.

---

## 15. Références

- API magnitude-tracker : <https://sqm.quentin-astro.fr>
- Endpoint ingestion : `POST|GET /api/sqm_push`
- Lib TSL2591 origine : <https://github.com/gshau/SQM_TSL2591>
- PCB / wiring : <https://easyeda.com/hujer.roman/sqm-hr>
- TinyGPSPlus : <http://arduiniana.org/libraries/tinygpsplus/>
- Unihedron SQM-LE serial protocol :
  <http://unihedron.com/projects/sqm-le/commands.html>

---

Copyright © 2025 Quentin Dumont — GPL-3.0.
