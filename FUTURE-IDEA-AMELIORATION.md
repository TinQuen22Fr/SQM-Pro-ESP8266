# Idées d'améliorations futures

Ce fichier recense les pistes d'amélioration **non encore implémentées**
dans le firmware SQM Pro. Aucun engagement de date — c'est un carnet
d'idées pour les prochaines versions.

---

## 🅲 Détection nuit/jour par calcul d'altitude du soleil (option C)

> **Idée discutée le 04/05/2026** — alternative plus « propre » à la
> détection par luminosité (option B implémentée en v2.1.0).

### Principe

Le firmware dispose déjà du module GPS NEO-6 qui fournit en temps réel :
- la latitude et la longitude (`g_lat`, `g_lng`)
- la date et l'heure UTC (`g_year`, `g_month`, `g_day`, `g_hour`,
  `g_minute`, `g_second`)

Avec ces 6 valeurs, on peut calculer l'**altitude du soleil** au-dessus
de l'horizon, sans aucun service externe. La nuit astronomique
correspond à une altitude solaire inférieure à un seuil :

| Seuil altitude soleil | Type de crépuscule         | Magnitude équivalente |
|-----------------------|----------------------------|------------------------|
| -6°                   | Crépuscule civil           | ~10 mag/arcsec²        |
| -12°                  | Crépuscule nautique        | ~12 mag/arcsec²        |
| -18°                  | Crépuscule astronomique    | ~13 mag/arcsec²        |

### Algorithme (≈ 50 lignes en C++)

```
1. Date + heure UTC → Julian Day (formule de Meeus)
2. Julian Day → temps sidéral apparent à Greenwich (GST)
3. GST + longitude → temps sidéral local (LST)
4. Date → longitude écliptique du soleil λ_s
5. λ_s → ascension droite α_s et déclinaison δ_s du soleil
6. (LST - α_s) → angle horaire H
7. (H, δ_s, latitude) → altitude h_s du soleil au-dessus de l'horizon
8. Si h_s < -12° → nuit, sinon jour
```

Voir <https://en.wikipedia.org/wiki/Position_of_the_Sun> pour les
formules exactes (à ±0,1° près, suffisant pour distinguer nuit/jour).

### Avantages par rapport à l'option B (luminosité)

- ✅ Robuste si le capteur est masqué (bâche, brume très dense, etc.) :
  on ne risque pas d'envoyer des mesures « nuit » par erreur en plein
  jour si le ciel est très couvert.
- ✅ Indépendant de la pollution lumineuse locale.
- ✅ Permettrait de calculer aussi l'altitude/azimut de la **lune** et
  de marquer les pushs concernés (info utile pour les analyses).

### Inconvénients

- ❌ Code astronomique non trivial (~50 lignes mathématiques + tests à
  ~±0,1° de précision sur l'altitude). Risque de bug subtil.
- ❌ Dépendance forte au verrouillage GPS — sans fix, on retombe sur le
  comportement legacy.
- ❌ Plus de RAM/flash consommée (calculs trigonométriques + stockage
  des constantes astronomiques).

### Pistes d'implémentation

Plutôt que de réinventer la roue, on peut s'inspirer de :
- <https://github.com/dmkishi/Dusk2Dawn> (lib Arduino, sun rise/set
  calculator, ~2 Ko flash)
- <https://github.com/buelowp/sunset> (algorithme de Meeus simplifié)

L'idée serait alors :

```cpp
#ifdef NIGHT_DETECTION_SUNALT_ON
  // Compute sun altitude using GPS-derived position and time
  double sun_alt = computeSunAltitude(g_year, g_month, g_day,
                                      g_hour, g_minute, g_second,
                                      g_lat, g_lng);
  if (sun_alt > SUN_ALT_THRESHOLD_DEG) {
    // Skip push - it's daytime
    return;
  }
#elif defined(NIGHT_ONLY_PUSH_ON)
  // Fallback: use TSL2591 reading as daylight detector
  if (mpsas < NIGHT_THRESHOLD_MPSAS) return;
#endif
```

### Décision actuelle

L'option C est **listée mais non implémentée** car l'option B (basée
sur la luminosité TSL2591) :
- Couvre 99 % des cas avec 5 lignes de code
- Ne dépend de rien d'autre que du capteur lui-même
- Est triviale à valider

Si un jour le besoin se présente (cas pathologique de capteur sous
bâche permanente, ou volonté d'ajouter le calcul de l'altitude
lunaire), on pourra implémenter C en s'appuyant sur Dusk2Dawn ou
équivalent.

---

## 🌙 Deep-sleep adaptatif jour/nuit

Idée bonus discutée mais non implémentée :

- **La nuit** (mpsas ≥ seuil) : `SLEEP_SEC = 300` (5 min) — cadence fine.
- **Le jour** (mpsas < seuil) : `SLEEP_SEC_DAY = 1800` (30 min) — on
  vérifie juste périodiquement si la nuit n'est pas tombée.

Avantage : économise la batterie en cas d'oubli d'éteindre le capteur
mobile au lever du soleil. Avec 12 réveils/heure de jour vs 2 réveils/
heure, autonomie multipliée par ~3 sur les périodes diurnes.

Implémentation : ~10 lignes dans `SQM_pro.ino` autour de l'appel
`ESP.deepSleep()`.

---

## 📊 Export Grafana / InfluxDB

Permettre au firmware de pousser **en parallèle** vers une instance
Grafana/InfluxDB locale (LAN) en plus du dashboard SQM Nightwatch.
Utile pour visualiser des historiques très longs ou cross-corréler
avec d'autres capteurs météo.

Implémentation : ajouter dans `WiFi.ino` une fonction
`send_grafana(...)` qui appelle l'endpoint InfluxDB `/api/v2/write`
via HTTPS avec un token Bearer.

---

## 🔔 Alerte ciel exceptionnel

Quand `mpsas > 21.5` (ciel SQM-21 ou mieux), envoyer une notification
push (Pushover / ntfy.sh / Telegram) à l'observateur pour qu'il sorte
son matériel.

---

## 🛰️ Annonce mDNS étendue

En plus de l'OTA, ajouter un service mDNS `_sqm._tcp` qui exposerait
en JSON les dernières mesures sur `http://sqm-pro-001.local/data`.
Permettrait à un autre logiciel local (PHD2, NINA, KStars) de récupérer
la mesure en temps réel sans passer par le cloud.

---

## 🧪 Calibration assistée par IA

Ajouter une page web servie par l'ESP8266 (mode AP) où un assistant
guide l'utilisateur pour calibrer son capteur en pointant un SQM
référence — calcul automatique de l'offset à stocker en EEPROM.

---

## 🪫 Mesure batterie plus précise

Le calcul actuel `analogRead(A0) / 1023.0 * 11` est imprécis (ADC
ESP8266 à 10 bits, non linéaire en bord de plage). Idées :
- Ajouter un MAX17048 (fuel gauge dédié pour LiPo) en I²C
- Ou calibrer manuellement la courbe ADC vs tension réelle dans EEPROM

---

---

## 🔧 Fix `SERIAL_BAUD` 74880 → 115200 pour compatibilité UDM (Unihedron Device Manager)

> **Identifié le 04/05/2026** — à valider/appliquer plus tard.

### Problème

Le firmware utilise actuellement `#define SERIAL_BAUD 74880` (rate de debug ESP8266
pour capturer les messages de boot natifs). Conséquence : le **logiciel UDM
(Unihedron Device Manager) ne peut pas dialoguer** avec le capteur en mode USB.

UDM (et les autres logiciels SQM) attendent **115200 8N1**, conformément aux
specs des modules Unihedron SQM-LU (USB) et SQM-LR (RS232).

### Fix (1 ligne)

Dans `Config.h` :

```cpp
// AVANT
#define SERIAL_BAUD 74880

// APRÈS (Unihedron-compatible)
#define SERIAL_BAUD 115200
```

### Conséquences

- ✅ UDM devrait fonctionner immédiatement (commandes `i`, `r`, `u`, `w`,
  `g`, `z…`, `A5…` reconnues par le firmware en mode USB Unihedron).
- ⚠️ Les messages de boot ESP8266 restent émis à 74880 par le hardware.
  Vous ne pourrez plus les lire dans le moniteur série après le passage à
  115200 (sauf à rouvrir temporairement le moniteur à 74880 pour debug).
- Aucun impact sur le mode normal Wi-Fi (le push HTTPS n'utilise pas le
  port série).

### Validation

Après modification :
1. Reflasher (USB ou OTA).
2. Lancer UDM, sélectionner le port USB du NodeMCU à **115200 8N1**.
3. Mettre l'interrupteur de façade en position **côté D5** (mode USB
   Unihedron).
4. UDM devrait répondre à la commande "Detect" et afficher
   les mesures TSL2591 + BME280 (version de protocole = SQM-LU).

---

## 📡 LoRa / Sigfox au lieu de Wi-Fi

Pour des capteurs en site **vraiment** isolé (montagne, désert), un
backend LoRa (RFM95W) ou Sigfox serait plus pertinent que le Wi-Fi.
Cadence d'envoi limitée mais portée >> 10 km.

Refonte plus lourde : module radio + downlink différent, donc plutôt
un projet sœur que ce firmware.

---

*Dernière mise à jour : 04/05/2026*

