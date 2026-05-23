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
| Plancher SNR | Garanti par moyenne 6× | **Désormais aussi** : plancher 6s min v2.2.17+ |

### Comparaison avec le SQM-LU officiel (Unihedron)

Le **SQM-LU officiel** utilise un capteur **TSL237** (sortie fréquence,
comptage d'impulsions sur une fenêtre temporelle) avec une durée typique
de **~300 ms** par lecture. Le TSL237 produit un signal numérique
(fréquence proportionnelle à l'éclairement) tandis que le TSL2591 produit
un signal analogique 16-bit accumulé sur sa fenêtre d'intégration.

| Critère | SQM-LU officiel | SQM Pro ESP8266 v2.2.17+ |
|---|---|---|
| Capteur | TSL237 (fréquence) | TSL2591 (ADC 16-bit) |
| Durée de mesure typique | ~300 ms | ~6 s (plancher) |
| Signal accumulé en ciel sombre | 1× | **~20×** |
| SNR théorique en ciel sombre | référence | **meilleur** (sqrt(20) ≈ 4.5×) |
| Calibration | usine | offset utilisateur (SqmCalOffset) |
| Validation terrain | référence | 19.72 vs 19.77 mpsas (écart 0.05) |

### Implémentation choisie pour SQM Pro v2.2.17 / v2.3.13

Ajout d'une constante `TSL_MIN_TOTAL_INTEGRATION_MS` (défaut 6000 ms) dans
`Config.h`. La fonction `takeReading()` continue d'accumuler les lectures en
condition ciel sombre (gain MAX + 600 ms d'intégration) jusqu'à ce que le
temps cumulé `niter * 600 ms` atteigne ce plancher, **même si** le seuil
`vis >= 128` est déjà satisfait.

Cela combine le meilleur des deux mondes :
- ✅ Auto-tuning gain/time intelligent (avantage SQM Pro d'origine)
- ✅ Plancher SNR garanti en ciel sombre (avantage FreeDSM)
- ✅ Signal intégré supérieur au SQM-LU officiel en ciel sombre

## Boîtier 3D

Les fichiers STL du boîtier FreeDSM sont disponibles dans le wiki original
et sont une excellente base pour un futur boîtier extérieur SQM Pro :
https://gitlab.citic.udc.es/lia2-publico/g4s/-/wikis/FreeDSM/Enclosure

## Mode Eclipse (12 août 2026)

FreeDSM v2.5 inclut un "eclipse mode" spécifique pour l'éclipse partielle
du 12 août 2026 (visible depuis l'Espagne, le sud de la France et la Corse).
À étudier pour SQM Pro : mesure haute cadence + buffer local + push
synchronisé pendant la fenêtre d'événement.

## Licence FreeDSM

GPL 3.0 (firmware) — Compatible avec la licence GPL 3.0 de SQM Pro
(v2.2.17 / v2.3.13).

## Téléchargement local pour analyse

Le ZIP source (37 MB) et son contenu décompressé (129 MB) ne sont **pas**
commités dans ce repo (cf. `.gitignore`). Pour les récupérer localement :

```bash
cd docs/freedsm-reference/
curl -L -o freedsm.v24.source.zip \
  "https://gitlab.citic.udc.es/lia2-publico/g4s/-/wikis/fw/freedsm.v24.source.zip"
unzip freedsm.v24.source.zip -d freedsm.v24.source
```
