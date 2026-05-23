# Changelog

Tous les changements notables de ce projet sont documentés dans ce fichier.

Format basé sur [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) ;
le projet suit le [versionnage sémantique](https://semver.org/lang/fr/).

## [v2.3.13] — 2026-05-23 (branche `wifimanager`)

### ✨ Plancher d'intégration cumulative TSL2591 en ciel sombre

**Inspiration** : projet **FreeDSM** (Université de A Coruña, GPL 3.0,
Gaia4Sustainability — Ministère espagnol des Sciences + UE NextGen)
qui moyenne 6 lectures TSL2591 (~6 s d'intégration cumulée) via le
paramètre `FREEDSM_TSL2591_SAMPLES = 6` pour améliorer le rapport
signal/bruit (SNR) en ciel sombre.

**Constat** : l'algorithme existant de `takeReading()` (Adafruit/gshau)
arrêtait la boucle d'accumulation dès que `vis >= 128`. Pour les ciels
très sombres (Bortle 1-2, mpsas > 21), ce seuil bas pouvait laisser le
SNR un peu juste alors que la marge d'amélioration est facile à
récupérer en accumulant un peu plus longtemps (le TSL2591 est limité à
600 ms d'intégration native par lecture, donc l'astuce passe forcément
par l'accumulation logicielle).

**Implémentation v2.3.13** :

- Nouvelle constante `TSL_MIN_TOTAL_INTEGRATION_MS` dans `Config.h`
  (défaut **6000 ms**, configurable, **0 = désactivé** pour retomber
  sur le comportement legacy).
- La boucle d'accumulation de `takeReading()` (mode "gain MAX +
  intégration 600 ms") continue désormais d'accumuler tant que les
  **deux** conditions ne sont pas satisfaites :
  - `vis >= 128` (porte signal d'origine) **ET**
  - `niter * 600 ms >= TSL_MIN_TOTAL_INTEGRATION_MS` (plancher SNR).
- Cap dur conservé à `niter <= 32` (≈ 21 s max worst-case).

**Combinaison avec l'auto-tuning existant** :

| Condition ciel | Intégration effective | Stratégie |
|---|---|---|
| Lumineux (lever, lampadaires) | ~100-400 ms | Auto-bump gain LOW + temps court |
| Crépuscule | ~600 ms - 2 s | Auto-bump intermédiaire |
| Ciel rural moyen (mpsas ~20) | **≥ 6 s** (plancher) | Plancher + auto-tuning |
| Ciel très sombre (Bortle 1-2) | jusqu'à ~21 s | Accumulation complète |

**Avantages** :
- ✅ Conserve la réactivité en ciel lumineux (pas de gaspillage CPU)
- ✅ SNR garanti en ciel sombre (équivalent FreeDSM)
- ✅ Compatible avec la calibration `19.72 vs 19.77 mpsas` validée terrain
- ✅ Désactivable d'un seul `#define` si jamais ça pose problème

**Fichiers modifiés** :

```
SQM_pro/Config.h                      (+ TSL_MIN_TOTAL_INTEGRATION_MS)
SQM_pro/SQM_TSL2591.cpp               (logique while combinée)
SQM_pro/SQM_pro.ino                   (Version "2.3.13")
docs/freedsm-reference/               (archive source FreeDSM + README)
```

**Test recommandé** : laisser tourner une nuit complète, observer la
stabilité des valeurs mpsas en ciel sombre (Bortle 4 sur Lourdes selon
relevés précédents) et comparer avec une nuit en v2.3.12. La valeur
moyenne devrait rester très proche (≈ 19.72) mais l'écart-type
inter-mesures devrait diminuer.

---


## [v2.3.12] — 2026-05-16 (branche `wifimanager`)

### 🐛 Corrigé — Affichage batterie OLED instable

**Backport** du fix `v2.2.16` de la branche `main` vers `wifimanager`.

**Constat utilisateur** : la jauge batterie sautillait trop entre deux
mesures successives sur l'OLED. Cause matérielle (pont 100k+100k + bruit
HF MT3608) à corriger côté hardware, mais on peut déjà stabiliser
l'affichage côté firmware.

**Fix v2.3.12** : ajout d'un **cache temporel** sur la lecture batterie.
La fonction `batteryRefreshIfDue()` ne fait une nouvelle mesure que si
**`BATTERY_DISPLAY_INTERVAL_MS`** est écoulé depuis la précédente
(default = **5000 ms = 5 s**). Entre deux refreshs, l'OLED affiche la
même valeur stable.

### ✨ Nouvelle API publique (MyLib.ino)

```cpp
bool  batteryRefreshIfDue();        // true si refresh effectif
float getCachedBatteryVoltage();    // last cached V
byte  getCachedBatteryPercent();    // last cached %
float getCachedBatteryRawAvg();     // last cached raw ADC average
```

### 📊 Avant / Après

| Aspect | v2.3.11 | v2.3.12 |
|---|---|---|
| Lecture A0 / seconde | 5-15× | **1× toutes 5 s** |
| Trace `[BAT]` série | Chaque cycle | **Une fois par refresh** |
| Stabilité OLED V/% | Sautille | **Stable** ✅ |
| OLED ↔ push API | Décorrélés | **Identiques** ✅ |

### ⚙️ Configurable

```cpp
// Config.h
#define BATTERY_DISPLAY_INTERVAL_MS  5000UL  // 5 s par défaut
```

- **2000-3000** : réaction rapide (debug branchement/débranchement)
- **5000** ✅ default
- **10000-30000** : stabilité max, deep-sleep-friendly

### Note hardware (à traiter ultérieurement par l'utilisateur)

Le pont diviseur 100k+100k actuel sur A0 amène du bruit lié à l'impédance
source élevée. Le condo **100 nF céramique** entre A0 et GND, ou un
filtre RC, restent les vraies solutions. Le cache logiciel masque
visuellement le problème sans le résoudre fondamentalement.

### Fichiers modifiés
- `SQM_pro.ino` : bump version → `2.3.12`
- `SQM_pro/Config.h` : `BATTERY_DISPLAY_INTERVAL_MS`
- `SQM_pro/MyLib.ino` : cache + getters + DisplWait refactor
- `SQM_pro/WiFi.ino` : push utilise getters cachés

---

## [v2.2.14] — 2026-05-08

### 🆕 Calcul Hz "équivalent SQM-LU" dans `rx`/`ux`/`U1x`

**Avant v2.2.14** : champ Hz toujours `0000000000Hz` (raison : notre TSL2591
ne produit pas de fréquence native, contrairement au TSL237 du SQM-LU).

**Constat utilisateur** : UDM en mode USB n'affichait pas de fréquence,
contrairement à son SQM-LU officiel.

**Fix v2.2.14** : calcul d'une **fréquence équivalente** dérivée du mpsas
en inversant la formule SQM-LU standard :

```
Hz_equiv = 10 ^ ((22.0 - mpsas) / 2.5)
```

Cette formule est **indépendante du capteur** (TSL237 vs TSL2591) : c'est
juste la conversion d'unités qu'utilise UDM en interne. Garantit :

- ✅ UDM affiche une vraie valeur Hz (plus de zéro fixe)
- ✅ Si UDM recalcule mpsas depuis Hz, il retombe sur **notre valeur exacte**
  → cohérence end-to-end avec UDM
- ⚠️ C'est une **valeur dérivée**, pas une mesure indépendante. Les
  comparaisons brutes Hz vs un SQM-LU peuvent différer même pour le
  même ciel (différences capteur).

### 📐 Tableau de correspondance mpsas → Hz équivalent

| mpsas | Hz équivalent | Type de ciel |
|---|---|---|
| 22.0 | 1 Hz | Ciel parfait (Sahara, polaire) |
| 21.5 | ~1.6 Hz | Site exceptionnel |
| 20.0 | ~6 Hz | Très sombre |
| 18.0 | ~40 Hz | Sub-urbain sombre |
| 17.0 | 100 Hz | Banlieue |
| 15.0 | 631 Hz | Urbain |
| 13.0 | 3981 Hz | Centre-ville |
| 12.0 | 10 000 Hz | Très lumineux |
| 10.0 | ~63 000 Hz | Jour faible |
| 5.0 | ~10⁶ Hz | Plein jour (saturation) |

### ✨ Helper public `sqmHzEquivalent(double mpsas)`

Nouvelle fonction publique dans `MyLib.ino` :
```cpp
String sqmHzEquivalent(double mpsas);
```
Retourne directement une chaîne **10 caractères zero-padded** (format
SQM-LU natif). Gère les cas limites :
- mpsas >= 22.0 → "0000000001" (1 Hz floor)
- mpsas <= 0.0 → "9999999999" (saturation cap)
- mpsas NaN/Inf → "0000000000"

### Fichiers modifiés
- `SQM_pro/MyLib.ino` : nouvelle fonction `sqmHzEquivalent()`
- `SQM_pro/SQM_pro.ino` : utilisé dans handlers `r`, `u`, `U1`

### Notes pour la suite

- **`wx` (weather extended)** : n'utilise pas de champ Hz, format inchangé.
- Si tu veux la vraie **fréquence physique du TSL2591** (counts/intégration
  time), c'est faisable mais nécessite une calibration croisée pour matcher
  l'échelle SQM-LU. La méthode v2.2.14 (inverse mpsas) donne le résultat
  le plus directement utilisable par UDM.

---

## [v2.2.13] — 2026-05-08

### 🆕 3 nouvelles commandes UDM extraites du code source officiel

Analyse du code source UDM v1.0.0.383 (Pascal/Lazarus, ~50 fichiers `.pas`)
téléchargé depuis `unihedron.com`. Inventaire complet des ~80 commandes
envoyées par UDM. La plupart ne nous concernent pas (data logger interne,
LED RGB, locks hardware, snow detection — toutes spécifiques au SQM-LU-DL).

**3 commandes ont été identifiées comme pertinentes pour notre DIY :**

#### 1. 🌍 `g0x` — Position GPS (NMEA GGA)

UDM attend une réponse au format NMEA GGA délimité par virgules :
```
GGA,HHMMSS.000,DDMM.MMMM,N/S,DDDMM.MMMM,E/W,FixQual,SatCount,
```

Notre DIY a un module **NEO-6M GPS** qui parse déjà ces données dans
`GPS.ino` (`g_lat`, `g_lng`, `g_hour`, etc.). On les renvoie à UDM qui
**affichera notre position GPS dans son panel "GPS Response"**.

🎯 **Bonus inattendu** : le **SQM-LU officiel** n'a PAS de GPS (sauf
variant DLS très rare). Notre DIY devient ainsi **plus complet** que le
modèle commercial sur ce point précis.

#### 2. ⚡ `U1x` — Variant rapide de `ux`

UDM utilise cette commande pour son **stress test de temps de réponse**
(envoyée 50 fois en boucle avec 32 ms de sleep). Format de réponse
identique à `ux` (unaveraged reading). Implémentée comme alias.

#### 3. 🔄 `0x19` (EM byte) — Soft reset

UDM envoie l'octet de contrôle **EM (End-of-Medium, 0x19)** seul pour
soft-reset le device avant un stress test. Detecté en amont du parser
de commandes (car pas de terminateur `x`), déclenche `ESP.restart()`.
Sans ce handler, UDM voyait son test foirer car notre device ne
redémarrait pas comme prévu.

### Commandes UDM **inutiles** pour notre DIY (documentées pour mémoire)

| Catégorie | Pourquoi non implémentées |
|---|---|
| `L*x` (≈18 cmds) | Data logger interne flash + RTC (DL variant uniquement) |
| `K*x` (≈10 cmds) | Système de verrouillage hardware (lock/unlock) |
| `f*x` | LED RGB color cycling (V variant) |
| `rfx`, `rFx` | Snow detection LED ratio |
| `vtx` | Vector tilt (nécessite accéléromètre) |
| `Max`, `max` | Memory map access propriétaire |
| `A2A/F/R`, `A40/41` | Variants logging params |
| `g1x`-`gCx` | GPS sub-cmds (commentés dans UDM, jamais envoyés) |

### Tests recommandés post-flash

1. **Vérifier `g0x`** dans le moniteur série 115200 :
   ```
   g0x -> GGA,083522.000,4815.2374,N,00224.8915,E,1,07,
   ```
   (valeurs réelles si tu as un fix GPS, ou tout à 0 sinon)
2. **Vérifier `U1x`** :
   ```
   U1x -> u, 10.42m,0000000000Hz,0000000005c,0000000.000s, 022.8C
   ```
3. **Vérifier soft reset** : envoie d'un octet `0x19` brut (via Python
   par ex.) → le device doit reboot dans ~50 ms.

### Effet UDM attendu

Dans **UDM**, après reload :
- Onglet **GPS** (ou panel position) : ta position GPS du DIY s'affiche
- Stress test (caché dans dev menu) : fonctionne sans timeout
- Reset depuis UDM : effectif

### Fichiers modifiés
- `SQM_pro/SQM_pro.ino` : handlers `g0`, `U1`, et byte `0x19`

---

## [v2.2.12] — 2026-05-08

### 🐛 Corrigé — Courbe LiPo inadaptée aux cellules non-standard

**Découverte utilisateur** : sa LiPo Yunique 3000 mAh chargée à 100% via
**B6 V3 Smart Charger** atteint **3.99 V** au multimètre (pas 4.20 V).
Plusieurs explications possibles (profil B6 conservateur, BMS interne
avec cutoff bas, chimie spécifique du fabricant), mais l'important est
que **la réalité hardware n'est pas la spec LiPo standard**.

**Impact en v2.2.11** : la courbe piecewise avait des breakpoints
hardcodés à 4.20 V → la cellule de l'utilisateur ne pouvait **jamais**
afficher 100% (plafonnée à ~80% à 3.99V).

### ✨ Ajouté — Sélecteur de courbe `BATTERY_CURVE_*`

Nouvelle macro dans `Config.h` pour choisir la stratégie de calcul du % :

```cpp
#define BATTERY_CURVE_LINEAR       // simple proportionnel (par défaut)
//#define BATTERY_CURVE_LIPO_REAL  // courbe piecewise LiPo 1S "standard"
```

| Mode | Quand l'utiliser |
|---|---|
| **LINEAR** ✅ default | Cellule custom, VMAX ≠ 4.20V, chimie non-standard, 18650 avec BMS, LiFePO4, etc. |
| **LIPO_REAL** | Cellule LiPo 1S "pure" qui charge réellement à 4.20V |

### 🔧 Corrigé — `BATTERY_VMAX` à 3.99 V par défaut

Adapté au setup utilisateur réel (Yunique 3000 mAh + B6 V3). Documenté
dans `Config.h` que l'utilisateur doit mettre la valeur **réellement
mesurée au multimètre** quand son chargeur dit "100% / fully charged".

Exemples ajoutés en commentaire :
- LiPo 1S + B6 standard 4.20V cutoff → 4.20
- LiPo 1S + B6 conservatif 4.00V cutoff → 4.00
- Yunique 3000 mAh user setup → 3.99
- 18650 Li-Ion standard → 4.20
- LiFePO4 1S → 3.65

### 🔧 Corrigé — `BATTERY_VOLTS_PER_RAW = 0.78f` par défaut

Adapté à la calibration empirique réelle de l'utilisateur (au lieu de
l'exemple historique 0.715).

### 📐 Tableau comparatif des modes (sur cellule VMAX=3.99V)

| Tension | LINEAR (v2.2.12, défaut) | LIPO_REAL (ne marche pas ici) |
|---------|--------------------------|-------------------------------|
| 3.99 V  | **100 %** ✅             | 79 % ❌ jamais à 100%         |
| 3.80 V  | 81 %                     | 55 %                          |
| 3.70 V  | 71 %                     | 35 %                          |
| 3.50 V  | 51 %                     | 10 %                          |
| 3.20 V  | 20 %                     | 2 %                           |
| 3.00 V  | 0 % (cutoff)             | 0 %                           |

### 🛰️ Note hardware utilisateur

- **GPS NEO-6M** : repassé sur le 3.3V régulé du NodeMCU (pas plus direct LiPo)
- **MT3608** : capable **2A** (pas 1A — confirmation utilisateur)
- **TP4056 LEDs rouge + bleue simultanément** : comportement normal en
  fin de charge avec consommation aval (cellule pleine, faible courant
  résiduel). Pas un défaut.

### Fichiers modifiés
- `SQM_pro/Config.h` : sélecteur `BATTERY_CURVE_*`, VMAX/VOLTS_PER_RAW user
- `SQM_pro/MyLib.ino` : `getBatteryPercent()` supporte les deux modes

---

## [v2.2.11] — 2026-05-08

### 🔋 Nouveau setup hardware utilisateur (validé)

Chaîne d'alimentation portable refondue :
```
LiPo 1S 3000mAh ──→ TP4056 (USB-C) ──→ MT3608 boost @5V ──→ Switch ON/OFF ──→ Vin NodeMCU
                                                                                 │
                                                                            AMS1117 → 3.3V propre
GPS NEO-6M : alimenté directement par la LiPo (B+)
Pont diviseur 100kΩ+100kΩ : toujours sur B+, GND sur GND NodeMCU (pas sur Bat-)
```

**Avantages** :
- ✅ Le **MT3608 boost à 5V** résout le problème de dropout AMS1117 quand
  la LiPo descend sous 4.5V → l'ADC ne dérive plus avec la tension batterie.
- ✅ **TP4056 USB-C** = standard moderne, BMS intégré sur la LiPo.
- ✅ Plus de HT7333 nécessaire.

### 🐛 Corrigé — Pourcentage batterie peu intuitif

**Symptôme** : LiPo à 3.99 V affichait **82 %** au lieu des ~95-100 %
attendus instinctivement.

**Cause** : interpolation **linéaire** entre VMIN (3.00 V) et VMAX (4.20 V) :
```
(3.99 - 3.00) / (4.20 - 3.00) × 100 = 82.5%
```

Mathématiquement correct, mais **non représentatif de la décharge réelle
LiPo** (qui a un plateau plat entre 3.7-4.0 V puis chute rapidement sous 3.6 V).

**Fix v2.2.11** : remplacé par une **courbe piecewise** approximant la
décharge LiPo 1S sous charge légère (~50-200 mA) :

| Tension | % affiché (v2.2.10) | % affiché (v2.2.11) |
|---------|---------------------|---------------------|
| 4.20 V  | 100 %               | 100 %               |
| 4.10 V  | 92 %                | 90 %                |
| 4.00 V  | 83 %                | 80 %                |
| 3.90 V  | 75 %                | 70 %                |
| 3.80 V  | 67 %                | 55 %                |
| 3.70 V  | 58 %                | **35 %** (knee)     |
| 3.60 V  | 50 %                | 20 %                |
| 3.50 V  | 42 %                | 10 %                |
| 3.40 V  | 33 %                | 5 %                 |
| 3.30 V  | 25 %                | 2 %                 |

À noter : **une LiPo à 3.99 V est réellement à ~80 % de capacité**. Une cellule
"pleine" est à 4.20 V exact. Si le TP4056 affiche "charged" autour de 3.95-4.0 V,
c'est qu'il n'a pas complété sa phase CV (constant voltage).

### ✨ Ajouté — Lissage du pourcentage affiché

`getBatteryPercentSmoothed()` : hystérésis de ±2 %, le pourcentage affiché
ne change que si la nouvelle lecture diffère de plus de 2 % de la précédente.
Évite le sautillement dû au bruit ADC (notamment celui injecté par le
switching du MT3608 à ~1.2 MHz).

### 📄 Documentation `DEPLOY.md` enrichie

- **§12.7** : Schéma d'alimentation LiPo 1S + MT3608 (avec ASCII art)
- **§12.8** : Procédure de calibration via USB-Série externe (CP2102/FTDI)
- **§12.9** : Tableau de la courbe de décharge LiPo et explication du
  pourcentage non-linéaire

### 💡 Recommandation hardware optionnelle

Le MT3608 est un convertisseur switching ~1.2 MHz qui injecte du bruit HF
sur l'alim. Avec un pont diviseur haute impédance (100k+100k), l'ADC capte
ce bruit comme une antenne.

**Fix simple** : ajouter un condensateur **100 nF céramique** entre A0 et
GND, au plus près du pin A0. ~0.05 €, 1 minute de soudure, lecture ADC
significativement plus stable (élimine la jitter "une fois sur deux").

### 🐛 Note BME280 (hors firmware)

L'utilisateur a constaté que ses BME280 récents sont des **clones / fakes**
(certaines unités soudées à l'envers, d'autres non-détectées sur I²C avec
message "No humidity"). Le module **AZ-Delivery** (acheté sur Amazon) est
confirmé fonctionnel. À acquérir de nouveau plus tard depuis une source
fiable. Aucun changement firmware nécessaire.

### Fichiers modifiés
- `SQM_pro/MyLib.ino` : nouvelle courbe LiPo + `getBatteryPercentSmoothed()`
- `SQM_pro/WiFi.ino` : utilise la version lissée pour le push API
- `DEPLOY.md` : §12.7, §12.8, §12.9 ajoutées

---

## [v2.2.10] — 2026-05-08

### 🐛 Corrigé — Température `cx` à `000.0C` à froid

Bug confirmé dans le moniteur série :
```
cx -> c,00000001.00m,0000000.000s, 000.0C,00000000.00m, 000.0C
                                  ^^^^^^^                ^^^^^^^
                                  temperature = 0 si cx envoye en premier
```

**Cause** : la commande `cx` réutilisait la variable globale `temp` qui
n'est jamais initialisée tant qu'aucun `rx`/`ux`/`wx` n'a été envoyé.

**Fix** : `cx` déclenche maintenant une lecture BME280 (via `ReadWeather()`,
~100ms) avant de construire la réponse. Plus rapide que `rx` (qui lit
aussi le TSL2591 ~1s) mais suffisant pour avoir une vraie température.

### ✨ Ajouté — Macros calibration personnalisables (cosmétique)

Nouvelles macros dans `Config.h` pour personnaliser la réponse `cx`
façon "certificat de calibration Unihedron" :

```cpp
#define DIY_LIGHT_CAL_OFFSET      0.00f    // 0 = utilise SqmCalOffset live
#define DIY_DARK_CAL_TIME_PERIOD  0.0f     // secondes
#define DIY_LIGHT_CAL_TEMP_FROM_BME280     // sinon DIY_LIGHT_CAL_TEMP
#define DIY_DARK_CAL_OFFSET       0.00f
#define DIY_DARK_CAL_TEMP_FROM_BME280
```

**IMPORTANT** : ces macros n'affectent **PAS** les mesures réelles.
Elles ne modifient que ce que `cx` retourne à UDM (champs cosmétiques).
Les mesures live (`rx`, `ux`, `wx`) utilisent toujours :
- `SqmCalOffset` (EEPROM, défaut `SQM_CAL_OFFSET`) pour la magnitude
- `TempCalOffset` (EEPROM, défaut `TEMP_CAL_OFFSET`) pour la temp
- Les valeurs live BME280 / TSL2591

Les mesures continuent de fonctionner normalement quoi qu'il arrive.

### Comportement UDM observé en v2.2.9 (utilisateur)
- ✅ `Settings` puis `Header` puis `Continuous` → log continu fonctionnel
- ✅ Valeurs mpsas, counter, time, Tint correctes dans le tableau
- ⚠️ Boutons `One record`, `Continuous` (1er écran), `Reading` grisés
  → contournement : passer par menu `Logging` → `Settings` puis revenir
  → comportement probablement lié à Protocol=2 (vs Protocol=4 du SQM-LU)
  → conséquence acceptable, on garde Protocol=2 pour rester safe

### Fichiers modifiés
- `SQM_pro/Config.h` : macros DIY_* certificat calibration
- `SQM_pro/SQM_pro.ino` : `cx` lit BME280 + utilise macros DIY_*

---

## [v2.2.9] — 2026-05-08

### 🐛 Corrigé — BUG CRITIQUE introduit en v2.2.8 (pollution Serial)

La trace `[BAT] raw_avg=... V=... pct=...` ajoutée en v2.2.8 dans
`DisplWait()` était envoyée sur `Serial` **même en mode USB**. Or en
mode USB le port série est réservé au protocole Unihedron : tout print
parasite corrompt les réponses UDM.

**Symptôme observé dans le log UDM** :
```
Sent: ix   Received: [BAT] raw_avg=13.63  V=7.395  pct=100  ❌
```

UDM a envoyé `ix` mais a reçu **notre trace debug** au lieu de la
réponse `i,...`. Le canal devenait désynchronisé en cascade :
```
Sent: rx   Received: 0000000000s,...    ← réponse au Ix précédent
Sent: cx   Received: r, 11.21m,...      ← réponse au rx précédent
```

**Fix v2.2.9** : tous les `Serial.print` de debug dans `DisplWait()`
(trace batterie + bip buzzer BAT LOW) sont désormais conditionnés à
`if (digitalRead(ModePin))` → ils ne se déclenchent que **en mode
WiFi/normal**. En mode USB/Unihedron, plus aucune pollution série.

### 🐛 Corrigé — Format `cx` 5e champ (depuis v2.2.5)

Comparé au log d'un vrai SQM-LU :
```
SQM-LU : c,00000019.92m,0000300.000s, 019.9C,00000008.71m, 020.9C
v2.2.8 : c,00000001.00m,0000000.000s, 022.8C,00000000.00m,0000000.000s
                                                          ^^^^^^^^^^^^
                                                  5e champ FAUX (period)
```

Le 5e champ est en réalité une **TEMPÉRATURE** (calibration dark), pas
un period. Corrigé en `temp_string + "C"`.

### ✨ Ajouté — Handlers UDM extraits du log SQM-LU officiel

Nouvelles commandes observées lors du dialogue UDM ↔ SQM-LU v1962 :

| Commande | Réponse | Rôle |
|---|---|---|
| `A1x` | `A,1,D,7,0,00128,08224` | Logging param 1 |
| `A2x` / `A2Px` | `A,2,D,3,F,7,P` | Logging param 2 |
| `A3x` / `A31x` | `A,3,D,0,1` | Logging mode |
| `A4x` | `A,4,0,7,31,-049,224,002` | Logging config |
| `P...x` | `0000000000s,0000000000s,...` | Set Period (echo) |
| `T...x` | `0000000000s,0000000000s,...` | Set Threshold (echo) |

Ces réponses correspondent à un SQM-LU fraîchement réinitialisé. Le
DIY n'implémente pas de logging on-device (pas de stockage flash
dédié), mais ces réponses statiques permettent à UDM de continuer son
dialogue sans timeout et d'activer ses boutons.

### ✨ Corrigé — Réponse `zcalDx`

Factory reset : `zxdL` → `zxdU` pour matcher le SQM-LU officiel.

### 🚫 Limitation hardware découverte

**`Find` USB d'UDM ne trouvera JAMAIS le DIY** :
```
FindUSB: Searching here : /dev/serial/by-id/usb-FTDI_*
```

UDM cherche **uniquement** des chips FTDI sur Linux. Le NodeMCU
LoLin V3 utilise un **CH340G** → invisible pour `Find` USB **par
design d'UDM**. Workaround : utiliser l'onglet **RS232** avec port et
baud sélectionnés manuellement (déjà fonctionnel en v2.2.8).

Pour rendre `Find` USB compatible : remplacer la puce CH340 par un
FT232RL sur la carte d'extension (modification hardware optionnelle,
non couverte par cette release).

### Fichiers modifiés
- `SQM_pro/MyLib.ino` : guard `Serial.print` + `buzzer()` sur ModePin
- `SQM_pro/SQM_pro.ino` :
  - format `cx` corrigé (5e champ = temp)
  - handlers `A1`, `A2`, `A2P`, `A3`, `A31`, `A4`, `P...`, `T...`
  - `zcalDx` → `zxdU`

---

## [v2.2.8] — 2026-05-XX

### Hardware (utilisateur)
Le NodeMCU est désormais sur un **shield d'extension** avec rails 3,3V/5V
distincts. Nouvelle configuration alimentation :
- Accu 18650 → TP4056 (chargeur dédié) → OUT+/- → interrupteur ON/OFF →
  **Vin** du shield → AMS1117 du NodeMCU → 3,3V pour ESP8266
- Module GPS NEO-6M : alimenté **directement par l'accu 18650** (plus en 3,3V régulé)
- Pont diviseur **100kΩ + 100kΩ** sur **B+** du TP4056 → A0 NodeMCU

### Ajouté — Calcul tension batterie calibré
- 🆕 `BATTERY_VOLTS_PER_RAW` dans `Config.h` : facteur de calibration
  empirique (V par unité ADC) — pas de formule théorique car le
  combiné « pont externe + diviseur interne NodeMCU + impédance
  source ADC ESP8266 » a un comportement non-linéaire dépendant
  du shield utilisé.
- 🆕 Constantes 18650 calibrables : `BATTERY_VMAX`, `BATTERY_VMIN`,
  `BATTERY_LOW_THRESHOLD`, `BATTERY_OVERSAMPLES`.
- 🆕 Fonctions `readBatteryVoltage()`, `getBatteryPercent()`,
  `readBatteryRawAvg()` dans `MyLib.ino` (oversampling 8x → bruit ADC réduit).

### Calibration initiale
- Mesure utilisateur : 3,99 V au multimètre, ancien code affichait 0,06 V
- Reverse-engineered raw ADC ≈ 5,58
- Facteur calculé : `BATTERY_VOLTS_PER_RAW = 3.99 / 5.58 ≈ 0.715 V/raw`

### Affichage OLED amélioré (page Wait)
- 🆕 Format normal : `Bat: X.XXV (YY%)`
- 🆕 Si tension < `BATTERY_LOW_THRESHOLD` (3,20 V par défaut) :
  - Texte `BAT LOW! X.XXV !` clignotant
  - Bip court périodique (50ms) sur le buzzer
- 🆕 Trace série `[BAT] raw_avg=N.NN V=V.VVV pct=PP` à chaque cycle
  pour faciliter la re-calibration future.

### API push enrichie
- 🆕 Nouveau champ `&Vpct=` (pourcentage batterie 0-100) en plus du
  `&V=` (tension en V, désormais 3 décimales).

### Procédure de re-calibration
1. Charger l'accu, mesurer **B+** au multimètre → noter `Vmesure`.
2. Ouvrir le Serial Monitor à 115200 → noter la valeur `raw_avg` affichée.
3. Calculer : `nouveau BATTERY_VOLTS_PER_RAW = Vmesure / raw_avg`.
4. Mettre à jour `Config.h`, reflasher.

### Fichiers modifiés
- `SQM_pro/Config.h` : bloc batterie complet ajouté (calibration + seuils)
- `SQM_pro/MyLib.ino` : fonctions batterie + nouveau bloc d'affichage OLED
- `SQM_pro/WiFi.ino` : utilise les nouvelles fonctions, ajoute `Vpct` au payload

---

## [v2.2.7] — 2026-05-06

### Ajouté — Séquence complète d'auto-détection UDM

Grâce à des logs UDM capturés sur un **vrai SQM-LU officiel**, on a
découvert qu'UDM envoie 6 commandes dans son handshake d'identification,
et que notre firmware n'en gérait que 1 (`ix`). Les 5 autres timeout
faisaient échouer la détection.

### Nouveaux handlers
- 🆕 **`Ix` (I majuscule)** — Report Interval Settings
  - ⚠️ **Différent** de `ix` (i minuscule) = Version Info !
  - Format : `0000000000s,0000000000s,00000000.00m,00000000.00m`
  - Représente : period_s, threshold_period_s, threshold_mpsas_1, threshold_mpsas_2
  - On retourne des zéros (feature non implémentée sur DIY, comme un SQM-LU fraîchement reset).

- 🆕 **`m0x`, `m1x`, `m2x`** — Manual parameter reads
  - Réponses : `m0,000`, `m1,000`, `m2,000`
  - Sur un vrai SQM-LU ce sont des paramètres de registre ; sur DIY
    on renvoie le format minimal qu'UDM attend.

- 🆕 **`Yx`** — ContCheck (advertises capabilities)
  - Réponse : `Yrcpu` (reading + calibration + period + unaveraged)

### Séquence UDM complète (maintenant supportée)
```
UDM → ix    → i,00000002,00000003,00000001,20200604
UDM → m0x   → m0,000
UDM → m1x   → m1,000
UDM → m2x   → m2,000
UDM → Yx    → Yrcpu
UDM → Ix    → 0000000000s,0000000000s,00000000.00m,00000000.00m
UDM → cx    → c,00000001.00m,0000000.000s, 022.8C,00000000.00m,0000000.000s
```

### Référence
Capture effectuée sur un SQM-LU v1962 sur `/dev/ttyUSB1` :
```
Sent: ix   Received: i,00000004,00000003,00000057,00001962
Sent: m0x  Received: m0,000
Sent: m1x  Received: m1,000
Sent: m2x  Received: m2,000
Sent: Yx   Received: Yrcpu
Sent: Ix   Received: 0000000000s,0000000000s,00000000.00m,00000000.00m
```

### Fichiers modifiés
- `SQM_pro/SQM_pro.ino` : handlers `Ix`, `m0x`, `m1x`, `m2x`, `Yx` ajoutés.

---

## [v2.2.6] — 2026-05-06

### Performance / Corrigé
- ⚡ **Réponse `ix`/`cx` quasi-instantanée (<10ms)** au lieu de potentiellement
  ~1s. UDM `Find` a un timeout court (~500ms) : si on prend trop de temps à
  répondre à `ix`, UDM nous considère comme « pas de SQM » et passe au
  port suivant.
- 🐛 **Cause** : dans la boucle USB, le firmware faisait `ReadWeather()` +
  `sqm.takeReading()` (TSL2591 en gain élevé peut bloquer jusqu'à 600-1000ms
  par lecture) **avant** même de lire la commande série. Donc même un simple
  `ix` (qui n'a aucun besoin des capteurs) attendait que tout le pipeline
  capteur termine.
- 🔧 **Fix** : la commande série est lue en premier ; les lectures capteurs
  ne sont effectuées **que** pour les commandes qui en ont besoin (`r`, `u`,
  `w`). Les commandes purement EEPROM (`i`, `c`, `g`, `z…`, `A5…`) répondent
  désormais immédiatement.

### Conséquence pratique
- Avec `v2.2.5` : `cx` répondait correctement mais lentement → UDM `Find`
  pouvait timeout malgré tout selon la config TSL2591.
- Avec `v2.2.6` : UDM `Find` doit aboutir dès le premier essai (sous
  réserve que les permissions `/dev/ttyUSB0` soient OK et que le moniteur
  série Arduino IDE ne tienne pas le port).

### Fichiers modifiés
- `SQM_pro/SQM_pro.ino` : restructuration de la boucle USB mode
  (lecture commande avant lectures capteurs).

---

## [v2.2.5] — 2026-05-04

### Corrigé
- 🐛 **Handler `cx` (Calibration Information Request) manquant** dans le
  protocole Unihedron. C'est **la** raison pour laquelle UDM (Unihedron
  Device Manager) ne trouvait pas le device même avec le bon port et le
  bon baud rate : lors de l'auto-détection, UDM envoie la séquence
  `ix` + `cx`. `ix` répondait bien, mais `cx` restait muet → UDM
  concluait « ce n'est pas un SQM valide » et rejetait le device.

### Ajouté
- ✨ Réponse à la commande `cx` au format officiel Unihedron :
  ```
  c,LLLLLLLL.LLm,SSSSSSS.SSSs, TTT.TC,DDDDDDDD.DDm,SSSSSSS.SSSs
  ```
  - `LLLLLLLL.LL` : light calibration offset (valeur EEPROM `SqmCalOffset`)
  - `SSSSSSS.SSS` : dark period (toujours 0 sur DIY, pas de dark sensor séparé)
  - ` TTT.T`      : température au moment de la calibration (temp courante)
  - `DDDDDDDD.DD` : dark calibration offset (0 sur DIY)
- 💡 Cette réponse permet à UDM de valider le device et d'afficher les
  offsets de calibration dans l'onglet **Information** / **Calibration**.

### Notes
- Le champ « dark calibration » reste à 0 car le SQM DIY n'a pas de
  capteur séparé pour le dark frame (contrairement au SQM-LU-DL
  commercial qui a un capteur obturé). Les `zcal1Axxx.xx` et
  `zcal2Axxx.xx` continuent de fonctionner pour ajuster light cal et
  temp cal respectivement.

---

## [v2.2.4] — 2026-05-04

### Corrigé
- 🐛 **`SERIAL_BAUD` 74880 → 115200** : indispensable pour qu'UDM
  (Unihedron Device Manager) puisse détecter le SQM DIY sur le port USB
  (la spec Unihedron SQM-LU/LR est 115200 8N1). Avec l'ancienne valeur
  (74880, qui correspond au rate des messages de boot ESP8266), UDM
  recevait du bruit binaire et concluait à l'absence de capteur.
- ➡️ Conséquence : pour relire les messages de boot ESP, ouvrir
  désormais le moniteur série à **74880** spécifiquement (le hardware
  ESP utilise toujours ce rate pour le boot, indépendamment du
  `SERIAL_BAUD`). Pour tout le reste (debug firmware, mode USB
  Unihedron), c'est **115200**.

### Documentation
- 📄 `Config.h` : commentaire détaillé sur le pourquoi du 115200 et la
  particularité des messages de boot à 74880.
- 📄 `FUTURE-IDEA-AMELIORATION.md` : entrée « Fix SERIAL_BAUD » marquée
  comme appliquée.

---

## [v2.2.3] — 2026-05-04

### Documentation
- 📄 `DEPLOY.md` §9 : la ligne de dépannage « GPS no wire! » mentionne
  maintenant en **première intention** la vérification du croisement
  TX↔RX (cause #1 des pannes GPS UART). Inutile d'aller chercher des
  problèmes électriques exotiques avant d'avoir validé que TX d'un
  côté est bien sur RX de l'autre.
- 📄 `README.md` schéma de câblage : note explicite sur la convention
  UART (toujours croiser TX↔RX) ajoutée sous le bloc GPS.

---

## [v2.2.2] — 2026-05-04

### Ajouté
- 🛰️ **OLED : diagnostic GPS amélioré**. La page mesure distingue
  désormais 3 cas quand le GPS n'a pas de fix :
  - `GPS no wire!` → aucune donnée série reçue (problème câblage / alim)
  - `GPS wait Sat:N` → trames NMEA reçues mais N satellites visibles
    (insuffisant pour un fix, il en faut ≥ 4)
  - Affichage normal lat/lng → fix GPS valide
- 📄 `DEPLOY.md` §9 dépannage : 3 nouvelles lignes pour chaque message
  GPS, avec actions concrètes (approcher d'une fenêtre, vérifier la
  LED rouge du module, etc.).

---

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
