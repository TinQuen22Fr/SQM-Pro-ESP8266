# FreeDSM (Free Dark Sky Meter) — Référence Archive

> Source archivée pour comparaison et analyse uniquement.
> Projet original : https://gitlab.citic.udc.es/lia2-publico/g4s/-/wikis/home

## Pourquoi cette archive ?

Le projet **FreeDSM** (Université de A Coruña / CITIC, Espagne) est un SQM DIY
basé sur **TSL2591 + AHT2x + Tasmota + Berry script**, financé par le projet
européen *Gaia4Sustainability* (Ministère espagnol des Sciences + UE NextGen).

Il partage la même philosophie open source que **SQM Pro ESP8266** mais avec
une approche **firmware Tasmota** au lieu d'Arduino C++ natif.

## Points techniques retenus pour SQM Pro

### Approche d'intégration "6 secondes"

FreeDSM n'utilise **PAS** une intégration native du TSL2591 de 6 secondes
(impossible : le TSL2591 a un maximum natif de 600 ms d'après son datasheet).

Il s'agit en réalité d'une **moyenne externe de 6 lectures** (chacune ~1s
incluant les délais Tasmota) :

```cpp
// Extrait de freedsmfw/tasmota/user_config_override.h
#define FREEDSM_TSL2591_SAMPLES 6
```

Puis dans `xsns_111_freedsm.ino` :
- Buffer circulaire `ir_readings[6]` et `full_readings[6]`
- Moyenne arithmétique calculée toutes les 6 mesures
- Gain : `TSL2591_GAIN_HIGH` puis `TSL2591_GAIN_MAX` selon le signal
- Intégration native : `TSL2591_INTEGRATIONTIME_500MS` ou `600MS`

### Comparaison avec l'approche SQM Pro

| Critère | FreeDSM | SQM Pro ESP8266 |
|---|---|---|
| Stratégie | Moyenne fixe (6 samples) | Adaptative (gain+time+accumulation) |
| Intégration cible | ~6 s (fixe) | 200 ms - 19 s (dynamique selon ciel) |
| En ciel lumineux | 6 s (gaspillage CPU) | ~200 ms (réactif) |
| En ciel sombre | 6 s (bon SNR) | Jusqu'à 19 s (SNR maximal) |
| Plancher SNR | Garanti par moyenne 6× | **Améliorable** : ajout d'un plancher 6s minimum |

### Implémentation choisie pour SQM Pro v2.3.13+

Ajout d'une constante `TSL_MIN_TOTAL_INTEGRATION_MS` (défaut 6000 ms) dans
`Config.h`. La fonction `takeReading()` continue d'accumuler les lectures en
condition ciel sombre (gain MAX + 600 ms d'intégration) jusqu'à ce que le
temps cumulé `niter * 600 ms` atteigne ce plancher, **même si** le seuil
`vis >= 128` est déjà satisfait.

Cela combine le meilleur des deux mondes :
- ✅ Auto-tuning gain/time intelligent (avantage SQM Pro)
- ✅ Plancher SNR garanti en ciel sombre (avantage FreeDSM)

## Boîtier 3D

Les fichiers STL du boîtier FreeDSM sont disponibles dans le wiki original
et sont une excellente base pour un futur boîtier extérieur SQM Pro :
https://gitlab.citic.udc.es/lia2-publico/g4s/-/wikis/FreeDSM/Enclosure

## Licence FreeDSM

GPL 3.0 (firmware) — Compatible avec la migration GPL 3.0 de SQM Pro.

## Contenu de cette archive

- `freedsm.v24.source.zip` — Code source original FreeDSM v2.4
- `freedsm.v24.source/` — Code décompressé pour navigation
- Référence pour l'idée du Mode Eclipse (12 août 2026)
