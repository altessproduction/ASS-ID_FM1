# ATEK303 1.3 « DRUMS » — une piste drum TR-808 / 909 / 707 sur le FM-1

Modification du firmware **ATEK303 v1.2** (dépôt `animatek/FM1-Animatek`, branche `atek303-v1.2`)
qui ajoute **une piste drum complète** — huit voix de percussion synthétisées à l'analogique dans
l'esprit des Roland TR-808, TR-909 et TR-707 — **sur le même principe que la piste 303 déjà
présente** : un séquenceur de 64 pas avec générateur de patterns à semée, des mutations, un LOCK,
un éditeur pas-à-pas, le tout sauvegardé dans les presets.

GPL-3.0-or-later, comme le reste. Tout a été développé et vérifié dans le **simulateur host** de
fm1-base (le même code que le firmware) ; voir plus bas ce qui reste à faire sur le matériel réel.

---

## 1. Ce qui a été ajouté

### Le moteur de sons — `app/dsp/drum_voice.c/.h`

Huit voix, toute la synthèse en C float sans libm (comme `atek_voice.c`), **aucun sample** :

| Voix | Modélisation | Sur le kit 707 |
| --- | --- | --- |
| **KICK** | sine à chute exponentielle de pitch (le circuit résonnant du 808) + click de bruit | — |
| **SNARE** | deux sines de corps (185/330 Hz sur le 808) + bruit passe-bande + passe-haut | plus propre |
| **CLAP** | bruit passe-bande frappé 4 fois : trois pré-échecs à 11 ms puis la queue | — |
| **HAT C** | **808 : les six oscillateurs carrés métalliques (263/400/421/474/587/845 Hz doublés)** ; 909/707 : bruit filtré | plus sombre |
| **HAT O** | le même générateur, queue longue. **Un charley fermé étrangle toujours l'ouvert** | — |
| **TOM L / TOM H** | sine à chute de pitch + second partiel | balayage plus large |
| **COWBELL** | deux carrés 540/800 Hz en passe-bande | **RIMSHOT** |

- Le knob **KIT** (page DKIT, ou K1 de cette page) bascule la saveur **808 / 909 / 707** des huit
  voix d'un coup : chute de pitch et click du kick, brillance des hats, corps de la snare,
  cowbell↔rimshot.
- Chaque voix a **LEVEL / TUNE / DECAY** (page DVOX), TUNE à ±12 demi-tons, LEVEL agit en direct.
- Une voix qui ne sonne pas ne coûte rien : le moteur ne rend que les voix actives.
- L'**accent** ouvre le niveau (montant réglable, ACC AMT sur la page KIT).

### Le séquenceur drum — `app/dsp/drum_pattern.c/.h`

- **64 pas** (4 pages de 16), **un bit de coup + un bit d'accent par voix et par pas**, compacté en
  RAM (128 octets + semée).
- **Générateur à semée**, le même principe que le SEQ de la 303 : une semée donne *toujours* le
  même groove. Trois knobs : **DENSITY** (kick à contratiempo, backbeat de caisse, charleys),
  **ACCENTS** (probabilité d'accent, le premier coup de kick est toujours accentué), **FILL**
  (notes fantômes, claps de réponse, toms et remplissage des 4 derniers pas). **STEPS** 1-64 :
  le pattern drum a sa propre longueur, indépendante de celle du 303.
- **Mutations déterministes** par couches, comme le 303 : **ENV** mute le BACKBONE (kick, snare,
  clap), **LFO** les TOPS (hats, toms, cowbell), **ARP** les ACCENTS — sur la page de 16 pas
  affichée. **LOCK** (OCT− + SEQ) : SEQ mute les trois couches au lieu de tirer une nouvelle semée.
  **REC** défait la dernière mutation ou édition (du 303 *ou* des drums : le dernier touché).
- En marche, SEQ attend le **prochain compas** (NEXT en ambre), comme le 303.
- **SWING** (page KIT, 50 % droit … 75 % triplet) : les contretemps attendent jusqu'à un tiers de
  double-croche. Le reste de la grille ne bouge pas.

### L'intégration dans la machine

- **Les deux pistes tournent ensemble sur la même horloge** (interne ou MIDI) : PLAY, tempo, reloj
  MIDI externe (start/stop/continue, 6 ticks par pas) sont communs. Le bus drum rejoint le 303
  **avant les effets** (drive → chorus → delay → reverb), niveau réglable **D-LVL**.
- **MUTE du drum track : OCT+ tenu + SEQ** (« DRUMS OFF/ON » ; OCT+ seul, relâché, fait toujours
  l'octave +1 — symétrique du LOCK à OCT− + SEQ). La ligne d'état affiche MUTE.
- **Pages 8-10 (par SELECT) : DVOX, DGEN, DKIT** — sur ces pages, SEQ/ENV/LFO/ARP/EDIT/REC
  s'appliquent aux drums, et les touches font écouter la voix choisie (note General MIDI 36/38/39/
  42/46/45/50/56 en sortie USB). HOME, FX et les pages 0-7 de la 1.2 sont **inchangés**.
- **Éditeur matrice 8×16** (EDIT depuis une page drum ; EDIT fait le cycle matrice → DGEN → DVOX) :
  SELECT déplace le curseur (la page suit), **K1** la voix, **K2** l'accent, **K3** décale la
  rangée d'un pas, **K4** le coup, une **touche** pose/enlève le coup et avance (et l'écoute,
  parado), **LFO** vide la rangée, **ENV** l'accent, **OCT− + K4** la longueur, **REC** défait.
- **Presets « DR64 »** : avec les drums, un preset ne tenait plus dans le huitième d'objet de la
  mémoire de la 1.2 (480 octets). Les presets voyagent donc **compactés** (320 octets : knobs en
  octets, pattern 303 bit-packé 2+9 bits par pas, pattern drum en bits) — 32 slots, 8 par objet.
  Les presets **AT64 de la 1.2 ne se chargent pas** ; les 8 presets d'usine emportent chacun son
  groove drum (kits et swing variés). Copie/restauration USB : `fm1_backup.py --magic DR64`.

## 2. Ce qui a été vérifié

- **Les 8 tests de la 1.2 passent** (basic, clock, controls, editor, fx, options, presets,
  steps64) — la navigation, les effets et les presets existants ne changent pas.
- **`app/tests/drums.txt` (nouveau, 63 vérifications)** : pattern d'usine déterministe, horloge
  commune (dstep avance avec step), MUTE OCT+ + SEQ, kit 909 au knob, DVOX (part/level), DGEN
  (STEPS 32, génération, mutations +2/+3, undo), matrice (cursor, rangée, coup, accent, undo,
  LFO clear), swing 50→75 %, **sauvegarde + reboot** avec le drum track complet rendu.
- **`tests/drum_render.c` (nouveau, 79 vérifications)** : les 24 voix (8 × 3 kits) sonnent,
  meurent (one-shots), pas de NaN ni d'emballement, l'accent est plus fort, crêtes équilibrées
  (kick ≈ 1,0, snare ≈ 1,1 comme le 303), swing = tiers de croche à 75 %, même semée = même
  pattern, sanitize. Écrit 3 WAV d'écoute (un par kit).
- **Écrans vérifiés à la capture** (simulateur) : DVOX, DGEN, KIT, matrice avec la colonne qui
  joue, tirette du 303 conservée.
- Gains calibrés : le bus drum (D-LVL 70 % par défaut) arrive dans le limiteur existant avec le
  303, sans écrêtage dur.

## 3. Ce qui n'est PAS fait (et pourquoi)

- **Pas de .fwsc compilé** : le toolchain JieLi (~1 Go) n'est pas dans cet environnement, et la
  règle du projet est de compiler/aprouver sur la machine de Javier. Le code est C99 strict, sans
  libm, aligné sur la 1.2 — `./build.sh` et `./build.sh --web` doivent passer tels quels une fois
  `base/` cloné (`herramientas/clonar-terceros.sh`).
- **CPU non mesuré dans fm1-emu** : le moteur ne rend que les voix qui sonnent (une voix = quelques
  oscs + 2 filtres), bien en dessous du 42 % du 303 ; à mesurer dans l'émulateur comme d'habitude.
- **Pas de MIDI drum** (canal 10 en entrée, notes du séquenceur drum en sortie) ni
  d'enregistrement temps réel : hors périmètre choisi.
- **Sons non écoutés sur matériel** : la modélisation suit les recettes des TR (six carrés du 808,
  pré-échecs du clap, etc.) et les crêtes ont été calibrées par mesure — l'oreille de Javier
  décidera des derniers réglages (c'est écrit pour : tout est dans des constantes nommées en tête
  de `drum_voice.c`).

## 4. Comment l'utiliser

```sh
# sur votre clone de atek303-v1.2 :
git am patch/0001-ATEK303-1.3-*.patch    # ou copiez le dossier atek303/
herramientas/clonar-terceros.sh          # fm1-base dans terceros/ (base/ pointe dessus)
cd atek303
sh build.sh --host                       # le simulateur
app/build/host/atek303_host app/tests/drums.txt app/build/host      # le test drum
app/build/host/atek303_host app/tests/basic.txt app/build/host      # la régression
sh build.sh                              # le .fwsc (toolchain JieLi requis)
```

Prise en main : **PLAY** (les deux pistes) → **SELECT** jusqu'à DGEN, **SEQ** tire un autre groove
→ **ENV/LFO/ARP** le déplient → **EDIT** pose les coups dans la matrice → **SELECT** sur DKIT pour
le kit et le swing → **OCT+ + SEQ** coupe les drums. Sauvegarde avec **SAVE** comme avant.

## 5. Contenu de la livraison

| Fichier | Quoi |
| --- | --- |
| `atek303/` | le firmware modifié complet (source), prêt à compiler |
| `CHANGELOG.md` | la ligne 1.3 (le dépôt d'origine, mis à jour) |
| `patch/0001-ATEK303-1.3-*.patch` | le patch git propre à appliquer sur `atek303-v1.2` |
| `demo/demo_drums_kit*.wav` | les 3 kits jouant le même groove (moteur seul, sans effets) |
| `docs/screenshots/drum_*.png` | les écrans du simulateur (DVOX, DGEN, KIT, matrice) |

Fichiers nouveaux : `app/dsp/drum_voice.{c,h}`, `app/dsp/drum_pattern.{c,h}`,
`app/tests/drums.txt`, `tests/drum_render.c`. Modifiés : `app/app.c`, `app/app.h`, `app/fm1.toml`,
`app/words.tsv`, `tools/factory_from_backup.py`, `app/factory_presets.h` → `.at64.bak` (format
AT64, remplacé par DR64), `CHANGELOG.md`, `README.md`.
