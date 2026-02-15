# NGPC Sound Creator - Documentation Complete

Outil C++/Qt pour composer, previewer et exporter du son pour **Neo Geo Pocket / Color**.
Emulation PSG T6W28 (3 canaux tone + 1 noise), moteur Z80 integre, export C arrays.

---

## Demarrage rapide (2 min)

1. Lancez `ngpc_sound_creator.exe`
2. Dans la fenetre de demarrage, choisissez:
   - **Nouveau Projet** (workflow recommande),
   - **Ouvrir Projet**,
   - **Edition libre** (tests rapides sans projet)
3. Choisissez la langue UI (**Francais/English**) dans cette meme fenetre.
4. Si la langue change, l'application redemarre automatiquement.
5. Pour un rendu en jeu conforme, utilisez le **Pack Driver NGPC** (`driver_custom_latest`).

---

## Onglets

| Onglet | Role |
|--------|------|
| **Projet** | Gestion du projet audio (songs, autosave, export all C/ASM, SFX projet) |
| **Player** | Charger un MIDI / driver SNK, play BGM, export |
| **Tracker** | Sequenceur 4 canaux avec edition au clavier |
| **Instruments** | Editeur d'instruments (enveloppe, vibrato, sweep, pitch curve) |
| **SFX Lab** | Labo SFX (preview driver-faithful : sweep/env/burst + ADSR5/LFO tone) |
| **Debug** | Registres PSG, pas-a-pas Z80 |
| **Aide** | Tutoriel integre complet (13 sections, pour debutants absolus) |

---

## Build

### Prerequis (Windows)
- Qt 6.x (Open Source) avec le kit **MinGW 64-bit**
- CMake 3.20+
- Ninja (inclus avec Qt)

### Compilation MinGW

```bat
set PATH=C:\Qt\6.10.2\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%
cmake -S . -B build-mingw -G Ninja
cmake --build build-mingw
```

Executable : `build-mingw\app\ngpc_sound_creator.exe`

### Compilation MSVC

```bat
cmake -S . -B build
cmake --build build --config Release
```

### Lancement

```bat
.\build-mingw\app\ngpc_sound_creator.exe
```

### Packaging Windows (zip + installateur)

Script fourni : `scripts/package_windows.ps1`

1) Construire l'app d'abord (ex: `build-mingw`)

2) Generer un package zip portable (EXE + DLL Qt + driver + docs) :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows.ps1 -BuildDir build-mingw -Version 0.1.0
```

Sortie : `dist\ngpc_sound_creator-windows-0.1.0.zip`

Option version auto (date + commit git court) :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows.ps1 -BuildDir build-mingw -AutoVersion
```

Exemple de version generee : `2026.02.10+a1b2c3d` (ou `+nogit` hors depot Git)

3) Generer aussi un installateur `.exe` (Inno Setup 6 requis) :

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows.ps1 -BuildDir build-mingw -Version 0.1.0 -CreateInstaller
```

Sortie attendue : `dist\ngpc_sound_creator_setup_0.1.0.exe`

Notes:
- Si `ISCC.exe` n'est pas installe, le zip est produit et l'installateur est saute.
- Si le linker echoue avec `Permission denied` sur `ngpc_sound_creator.exe`, fermer toute instance de l'app avant de rebuild/package.
- Priorite de version: `-Version` explicite > `-AutoVersion` > fallback `0.1.0`.

---

## Tutoriel : Composer une melodie de A a Z

Ce tutoriel vous guide pas-a-pas, meme si vous n'y connaissez rien en musique.

### Etape 1 : Comprendre les bases

Le Neo Geo Pocket a une puce sonore **T6W28** avec :
- **3 canaux Tone** (T0, T1, T2) : jouent des notes musicales
- **1 canal Noise** (N) : bruits, percussions

Chaque "note" a 3 proprietes :
- **Note** : quelle touche du piano (C=Do, D=Re, E=Mi, F=Fa, G=Sol, A=La, B=Si)
- **Instrument** : quel "son" utiliser (00 a 7F, defini dans l'onglet Instruments)
- **Attn** (attenuation) : le volume (0 = fort, F = silence)

### Etape 2 : Preparer un instrument

1. Allez dans l'onglet **Instruments**
2. Selectionnez l'instrument **00** dans la liste
3. Reglez :
   - **Attenuation** = 0 (volume max)
   - **Mode** = 0 (tone)
   - Cochez **Envelope** et choisissez une courbe (ex: "Decay" pour un son qui diminue)
4. Cliquez sur **Preview** pour entendre le son
5. Ajustez jusqu'a ce que ca sonne bien

Quelques exemples d'instruments utiles :
- **Instrument 00** : Melodie principale (envelope decay moyen)
- **Instrument 01** : Basse (envelope long, octave basse)
- **Instrument 02** : Arpege/accords (envelope court)

### Etape 3 : Ouvrir le Tracker

1. Allez dans l'onglet **Tracker**
2. Vous voyez une grille avec 4 colonnes : T0, T1, T2, Noise
3. Chaque ligne = 1 "pas" dans le temps (de haut en bas)
4. La grille fait 64 lignes par defaut (modifiable avec **Len**)

### Etape 4 : Entrer des notes

Le tracker fonctionne comme un clavier de piano :

**Disposition AZERTY (par defaut) :**
```
Octave basse :  W=Do  X=Do#  C=Re  D=Re#  V=Mi  B=Fa  G=Fa#  N=Sol  H=Sol#  ,=La  J=La#  ;=Si
Octave haute :  A=Do  2=Do#  Z=Re  3=Re#  E=Mi  R=Fa  5=Fa#  T=Sol  6=Sol#  Y=La  7=La#  U=Si
```

**Disposition QWERTY :**
```
Octave basse :  Z=Do  S=Do#  X=Re  D=Re#  C=Mi  V=Fa  G=Fa#  B=Sol  H=Sol#  N=La  J=La#  M=Si
Octave haute :  Q=Do  2=Do#  W=Re  3=Re#  E=Mi  R=Fa  5=Fa#  T=Sol  6=Sol#  Y=La  7=La#  U=Si
```

Pour entrer votre premiere melodie :

1. Verifiez que le bouton **REC** est rouge (mode enregistrement actif)
2. Placez le curseur sur la colonne **T0** (clic ou Tab)
3. Reglez l'**octave** a 4 (spinbox "Oct")
4. Reglez le **step** a 1 (avance d'une ligne apres chaque note)
5. Tapez les touches pour entrer les notes :
   - Exemple melodie simple (AZERTY) : `W` `C` `V` `B` `V` `C` `W`
   - Ca donne : Do Re Mi Fa Mi Re Do

### Etape 5 : Ajouter une ligne de basse

1. Appuyez sur **Tab** pour passer au canal **T1**
2. Appuyez sur **Home** pour revenir a la ligne 0
3. Baissez l'octave a 2 (spinbox "Oct" ou Numpad -)
4. Augmentez le step a 4 (spinbox "Step" ou Numpad *)
5. Tapez des notes tous les 4 pas : `W` (Do) `V` (Mi) `B` (Fa) `W` (Do)

### Etape 6 : Ecouter

1. Appuyez sur **Espace** pour lancer la lecture
2. Appuyez sur **Espace** pour stopper
3. **F5** = jouer depuis le debut
4. **F8** = stop

### Etape 7 : Ajuster le tempo

- **TPR** (Ticks Per Row) controle la vitesse :
  - TPR = 4 : rapide (~225 BPM)
  - TPR = 8 : moyen (~112 BPM) -- defaut
  - TPR = 12 : lent (~75 BPM)
- Le BPM approximatif est affiche a droite

### Etape 8 : Raffiner

- **Changer le volume** : naviguez dans la sous-colonne Attn (fleche droite x2), tapez 0-F
- **Changer l'instrument** : sous-colonne Inst (fleche droite x1), tapez 00-7F en hex
- **Note-off** (couper le son) : Backspace
- **Effacer** : Delete
- **Muter un canal** : F1-F4 ou clic sur l'en-tete du canal

### Etape 9 : Exporter

1. Choisissez le **mode d'export** dans le combo de la barre d'outils :
   - **Pre-baked** : fidelite parfaite (simulation tick-by-tick, tous effets inclus, streams plus gros)
   - **Hybride** : instruments geres par le driver NGPC (opcodes 0xF0-0xFA, streams compacts)
2. Cliquez sur **Export C** (ou **ASM**)
3. Choisissez l'emplacement et le nom du fichier
4. Le tracker exporte maintenant:
   - le fichier musique (`.c` ou `.inc`) avec `NOTE_TABLE` + `BGM_CHx`,
   - un fichier compagnon `*_instruments.c` (table `s_bgm_instruments[]`).
5. L'export lance aussi un **audit automatique**:
   - les warnings apparaissent dans le log tracker sous la forme `WARN export: ...`,
   - les memes warnings sont recopies en commentaire au debut du fichier exporte.
   - exemple: instrument hors plage, FX non supporte runtime, depassement de table de notes.
6. Si vous voyez des warnings, corrigez dans le tracker puis re-exportez.
7. Incluez le fichier musique dans votre projet NGPC.
8. Synchronisez la table d'instruments du driver avec `*_instruments.c`.
9. Le format est compatible avec le PlayerTab (chargement direct, effets instrument joues en temps reel)

> Important: pour garder la parite audio tool -> jeu, utilisez le **Pack Driver NGPC**
> (source technique: `driver_custom_latest`, minimum `sounds.c` + `sounds.h`).
> Sans ce pack, des fonctions avancees (hybride, opcodes etendus, rendu effets) peuvent differer.

### Choisir Pre-baked vs Hybride (decision rapide)

| Critere | Pre-baked | Hybride |
|--------|-----------|---------|
| Fidelite sonore | Max (tick-by-tick) | Tres bonne, depend du driver |
| Taille des donnees | Plus lourde | Plus compacte |
| Charge runtime console | Faible (moins de logique driver instrument) | Plus elevee (driver applique les effets) |
| Robustesse "copier-coller direct" | Tres simple | Demande driver compatible |
| Recommande pour | Debug exact / cas limites | Jeu final / projet multi-musiques |

Regle pratique:
- **Debutant / integration simple**: commencez en **Hybride** avec le Pack Driver NGPC.
- **Verification parite stricte ou bug subtil**: testez aussi en **Pre-baked**.

### Multi-musiques / banques d'instruments

Si vous importez plusieurs musiques, vous avez 2 strategies:

1. **Banque globale unique (recommande)**  
   - Une seule table d'instruments pour tout le jeu.
   - Les IDs d'instruments (00..0F) restent stables entre morceaux.
   - Workflow simple, moins de surprises en integration.

2. **Banques par morceau (avance)**  
   - Chaque morceau a son couple `songX.c` + `songX_instruments.c`.
   - Il faut garder les paires synchronisees.
   - Le driver actuel ayant une table active unique, vous devez resynchroniser la table
     avant integration/build (ou maintenir une table globale derivee).

### Etape 10 : Sauvegarder / Charger

- **Ctrl+S** ou bouton **Save** : sauvegarde le pattern actif en fichier `.ngpat` (JSON)
- **Ctrl+O** ou bouton **Load** : charge un fichier `.ngpat`
- **Save Song** : sauvegarde le morceau complet en `.ngps` (tous les patterns + ordre + boucle)
- **Load Song** : charge un fichier `.ngps` (ou `.ngpat` pour compatibilite)

### Etape 11 : Multi-pattern et Song mode

Pour composer un morceau avec couplet, refrain, pont :
1. Composez votre premier pattern (intro)
2. Cliquez **+Pat** pour ajouter un pattern vide (couplet)
3. Cliquez **Clone** pour dupliquer et creer une variation (refrain)
4. Configurez la **liste d'ordre** a gauche : 00, 01, 02, 01, 02
5. Cliquez **Song** pour ecouter le morceau enchaine
6. Cliquez **WAV** pour exporter en fichier audio

### Etape 11b : Debug runtime (nouveau)

Le tracker a un bouton **Dbg RT** (ligne Mute/Solo) pour diagnostiquer les ecarts tool/driver:
- Activez **Dbg RT** puis lancez la lecture.
- A chaque nouvelle ligne, le log affiche un dump runtime par canal:
  - note cellule,
  - instrument/attn/fx cellule,
  - sortie runtime (`divider` tone ou mode noise),
  - attenuation runtime, fx runtime actif, expression, pitch bend.
- Desactivez ensuite le bouton pour eviter de saturer le log.

### Etape 12 : Workflow Projet (recommande)

Au demarrage, vous avez 3 choix :
- **Nouveau Projet**
- **Ouvrir Projet**
- **Edition libre** (test rapide sans projet)

En mode projet :
- Onglet **Projet** pour creer/ouvrir/renommer/supprimer des songs
- Boutons projet dedies : **Ouvrir projet**, **Sauver projet**, **Sauver projet sous...**
- Bouton utilitaire : **Exporter Pack Driver NGPC...** (copie un pack d'integration pret a ajouter au jeu)
- Sauvegarde automatique configurable (timer/changement onglet/fermeture)
- **Export All C/ASM** : export de toutes les songs en une passe

Sortie `Export All` (dossier `exports/`) :
- `song_xxx.c` (ou `.inc`) : streams musicaux
- `project_instruments.c` : banque globale d'instruments du projet
- `project_sfx.c` : banque globale SFX du projet (depuis SFX Lab)
- `project_audio_manifest.txt` : index lisible des songs/fichiers/symboles
- `project_audio_api.h` + `project_audio_api.c` (export C) : table C centralisee `NGPC_PROJECT_SONGS[]`

Details techniques `Export All` :
- Les symboles songs sont namespaced (`PROJECT_<SONG_ID>_*`) pour eviter les collisions au link.
- Le manifest reference chaque song exportee et son prefixe symbole.
- L'API C expose des pointeurs streams/loops par song et un helper de demarrage runtime:
  `NgpcProject_BgmStartLoop4ByIndex(i)` (auto-switch `NOTE_TABLE` + streams).
- `project_audio_api.c` genere aussi un `NOTE_TABLE` fallback (weak) pour compatibilite link.
- Un rappel dans l'UI signale que l'export est prevu pour le **Pack Driver NGPC**
  (source technique: `driver_custom_latest`).

### Etape 13 : Integrer `project_sfx.c` dans le jeu

1. Dans **SFX Lab**, reglez un son puis cliquez **Save To Project**.
2. Lancez **Export All C** depuis l'onglet Projet.
3. Ajoutez `exports/project_sfx.c` au build du jeu.
4. Mappez un ID gameplay vers la banque exportee.

Exemple de mapping (fichier jeu, en plus du driver) :

```c
#include "sounds.h"

extern const unsigned char PROJECT_SFX_COUNT;
extern const unsigned char PROJECT_SFX_TONE_ON[];
extern const unsigned char PROJECT_SFX_TONE_CH[];
extern const unsigned short PROJECT_SFX_TONE_DIV[];
extern const unsigned char PROJECT_SFX_TONE_ATTN[];
extern const unsigned char PROJECT_SFX_TONE_FRAMES[];
extern const unsigned char PROJECT_SFX_TONE_SW_ON[];
extern const unsigned short PROJECT_SFX_TONE_SW_END[];
extern const signed short PROJECT_SFX_TONE_SW_STEP[];
extern const unsigned char PROJECT_SFX_TONE_SW_SPEED[];
extern const unsigned char PROJECT_SFX_TONE_SW_PING[];
extern const unsigned char PROJECT_SFX_TONE_ENV_ON[];
extern const unsigned char PROJECT_SFX_TONE_ENV_STEP[];
extern const unsigned char PROJECT_SFX_TONE_ENV_SPD[];
extern const unsigned char PROJECT_SFX_NOISE_ON[];
extern const unsigned char PROJECT_SFX_NOISE_RATE[];
extern const unsigned char PROJECT_SFX_NOISE_TYPE[];
extern const unsigned char PROJECT_SFX_NOISE_ATTN[];
extern const unsigned char PROJECT_SFX_NOISE_FRAMES[];
extern const unsigned char PROJECT_SFX_NOISE_BURST[];
extern const unsigned char PROJECT_SFX_NOISE_BURST_DUR[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_ON[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_STEP[];
extern const unsigned char PROJECT_SFX_NOISE_ENV_SPD[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_ON[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_AR[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_DR[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_SL[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_SR[];
extern const unsigned char PROJECT_SFX_TONE_ADSR_RR[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_ON[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_WAVE[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_HOLD[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_RATE[];
extern const unsigned char PROJECT_SFX_TONE_LFO1_DEPTH[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_ON[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_WAVE[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_HOLD[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_RATE[];
extern const unsigned char PROJECT_SFX_TONE_LFO2_DEPTH[];
extern const unsigned char PROJECT_SFX_TONE_LFO_ALGO[];

void Sfx_Play(u8 id) {
    if (id >= PROJECT_SFX_COUNT) return;
    if (PROJECT_SFX_TONE_ON[id]) {
        Sfx_PlayToneEx(PROJECT_SFX_TONE_CH[id], PROJECT_SFX_TONE_DIV[id], PROJECT_SFX_TONE_ATTN[id],
                       PROJECT_SFX_TONE_FRAMES[id], PROJECT_SFX_TONE_SW_END[id], PROJECT_SFX_TONE_SW_STEP[id],
                       PROJECT_SFX_TONE_SW_SPEED[id], PROJECT_SFX_TONE_SW_PING[id], PROJECT_SFX_TONE_SW_ON[id],
                       PROJECT_SFX_TONE_ENV_ON[id], PROJECT_SFX_TONE_ENV_STEP[id], PROJECT_SFX_TONE_ENV_SPD[id]);
    }
    if (PROJECT_SFX_NOISE_ON[id]) {
        Sfx_PlayNoiseEx(PROJECT_SFX_NOISE_RATE[id], PROJECT_SFX_NOISE_TYPE[id], PROJECT_SFX_NOISE_ATTN[id],
                        PROJECT_SFX_NOISE_FRAMES[id], PROJECT_SFX_NOISE_BURST[id], PROJECT_SFX_NOISE_BURST_DUR[id],
                        PROJECT_SFX_NOISE_ENV_ON[id], PROJECT_SFX_NOISE_ENV_STEP[id], PROJECT_SFX_NOISE_ENV_SPD[id]);
    }
}
```

Notes:
- Le mapping `Sfx_PlayToneEx/Sfx_PlayNoiseEx` ci-dessus reste le chemin simple/compatible.
- Les tableaux `PROJECT_SFX_TONE_ADSR_*` et `PROJECT_SFX_TONE_LFO*` sont exportes pour les workflows avances (runtime custom/etendu).

Puis dans le code gameplay :

```c
Sfx_Play(0); /* exemple: menu confirm */
```

### Etape 14 : Integrer les songs via `project_audio_api` (Export All C)

`Export All C` genere aussi:
- `exports/project_audio_manifest.txt`
- `exports/project_audio_api.h`
- `exports/project_audio_api.c`

Usage:
1. Ajoutez ces fichiers au build du jeu, en plus des `song_xxx.c`.
2. Soit utilisez `NGPC_PROJECT_SONGS[i]` directement, soit appelez
   `NgpcProject_BgmStartLoop4ByIndex(i)` pour lancer une song.
3. `NgpcProject_BgmStartLoop4ByIndex(i)` configure automatiquement la bonne
   `NOTE_TABLE` via `Bgm_SetNoteTable(...)`, puis appelle `Bgm_StartLoop4Ex(...)`.

Exemple minimal :

```c
/* Lance la 1ere song du projet exporte */
NgpcProject_BgmStartLoop4ByIndex(0);
```

---

## Raccourcis clavier du Tracker

### Navigation

| Touche | Action |
|--------|--------|
| Fleches haut/bas | Deplacer le curseur d'une ligne |
| Fleches gauche/droite | Changer de sous-colonne (Note / Inst / Attn) |
| Tab / Shift+Tab | Canal suivant / precedent |
| Page Up / Page Down | Sauter de 16 lignes |
| Home / End | Aller au debut / fin du pattern |

### Edition

| Touche | Action |
|--------|--------|
| Touches piano (voir au-dessus) | Entrer une note |
| Backspace | Note-off (couper le son) |
| Delete | Effacer la cellule |
| 0-9, A-F | Entrer valeur hex (sous-colonne Inst ou Attn) |
| Insert | Inserer une ligne vide (decale vers le bas) |
| Shift+Delete | Supprimer la ligne (decale vers le haut) |

### Raccourcis Ctrl

| Touche | Action |
|--------|--------|
| Ctrl+Z | Annuler |
| Ctrl+Shift+Z / Ctrl+Y | Refaire |
| Ctrl+C | Copier la selection |
| Ctrl+X | Couper la selection |
| Ctrl+V | Coller |
| Ctrl+A | Selectionner tout (canal courant) |
| Ctrl+Shift+A | Selectionner tout (tous les canaux) |
| Ctrl+Shift+C | Copier en texte (presse-papiers systeme) |
| Ctrl+D | Dupliquer la ligne |
| Ctrl+I | Interpoler la colonne active (Inst / Attn / FX param) |
| Ctrl+H | Humanize Attn (variation aleatoire legere) |
| Ctrl+B | Batch apply de la colonne active |
| Ctrl+T | Appliquer le template selectionne |
| Ctrl+Up / Ctrl+Down | Transposer +1 / -1 demi-ton |
| Ctrl+Shift+Up / Ctrl+Shift+Down | Transposer +12 / -12 demi-tons (octave) |
| Ctrl+S | Sauvegarder le pattern |
| Ctrl+O | Charger un pattern |
| Ctrl+Delete | Effacer tout le pattern |

### Transport et divers

| Touche | Action |
|--------|--------|
| Espace | Play / Stop |
| F5 | Play depuis le debut |
| F8 | Stop |
| F1 / F2 / F3 / F4 | Muter canal T0 / T1 / T2 / Noise |
| Numpad + / - | Octave +1 / -1 |
| Numpad * / / | Step +1 / -1 |

### Selection

| Touche | Action |
|--------|--------|
| Shift + fleches | Etendre la selection |
| Shift + clic | Selection jusqu'au clic |
| Double-clic | Selectionner tout le canal |
| Echap | Annuler la selection |
| Clic droit | Menu contextuel |

---

## Concepts musicaux rapides

Si vous n'y connaissez rien, voici le minimum :

### Les notes
Il y a 12 notes qui se repetent en boucle a chaque **octave** :
```
Do  Do#  Re  Re#  Mi  Fa  Fa#  Sol  Sol#  La  La#  Si
C   C#   D   D#   E   F   F#   G    G#    A   A#   B
```

- Le **#** (diese) = note entre deux notes (touche noire du piano)
- **Octave** = a quel registre (grave/aigu). Oct 2 = basse, Oct 4 = medium, Oct 6 = aigu
- Le NGPC supporte environ octave 1 a 7

### Le rythme
- Chaque **ligne** du tracker = un pas de temps
- **TPR** (Ticks Per Row) = combien de frames (1/60 seconde) par ligne
- **4 lignes** = 1 temps (beat). Donc ligne 0, 4, 8, 12... = les temps forts
- Les lignes sont colorees pour aider : chaque 4e ligne est plus claire, chaque 16e est encore plus marquee

### Le volume (Attenuation)
- **0** = volume maximum (le plus fort)
- **F** (15) = silence total
- Si vous ne mettez rien (tiret `-`), le volume est gere par l'instrument
- Si vous mettez une valeur en colonne **Attn** sur une note, c'est un **override absolu**
  (pas une addition). Exemple: note Attn=3 + instrument Attn=2 => base a 3, pas 5.
- En lecture/export hybride, l'ordre est:
  1) base = Attn note (si presente) sinon Attn instrument
  2) puis effets instrument (ADSR/env/macro) qui font evoluer cette base
  3) puis offsets optionnels expression/fade (driver), avec clamp final a 0..15

### Les instruments
Chaque note peut utiliser un instrument different (00 a 7F = jusqu'a 128 instruments).
L'instrument definit :
- L'**enveloppe** : comment le volume evolue dans le temps (attaque, decay...)
- Le **vibrato** : oscillation du pitch
- Le **sweep** : glissement de pitch
- La **pitch curve** : profil de pitch initial (pour les effets "zap", "slide"...)

---

## Exemples de patterns

### Arpege simple (canal T0)
```
Ligne  Note  Inst  Attn
00     C-4   00    -      <- Do
01     E-4   00    -      <- Mi
02     G-4   00    -      <- Sol
03     C-5   00    -      <- Do octave superieure
04     G-4   00    -
05     E-4   00    -
06     C-4   00    -
07     OFF   --    -      <- couper le son
```

### Basse (canal T1, octave 2, step 4)
```
Ligne  Note  Inst  Attn
00     C-2   01    -
04     C-2   01    -
08     F-2   01    -
0C     G-2   01    -
```

### Rythme noise (canal Noise)
```
Ligne  Note   Inst  Attn  FX
00     P.H    02    0     ---   <- kick periodique rapide (fort)
04     W.H    02    4     C02   <- hihat blanc rapide (coupe courte)
08     P.H    02    0     ---
0C     W.H    02    4     C02
```

---

## Fonctionnalites

### Tracker
- **Demarrage** : Nouveau Projet / Ouvrir Projet / **Edition libre (sans projet)**
- **Mode Projet** : ouverture/creation/chargement de projet, liste projets recents, option reouvrir dernier projet
- Onglet **Projet** : liste des morceaux, ouverture rapide, creation, renommage, suppression
- Boutons projet dedies: **Ouvrir projet**, **Sauver projet**, **Sauver projet sous...**
- Bouton **Exporter Pack Driver NGPC...** (package `ngpc_audio_driver_pack` pour integration jeu)
- Creation de morceau depuis **MIDI** (Nouveau depuis MIDI)
- **Autosave** configurable (off/30s/1m/2m/5m), autosave changement d'onglet, autosave fermeture
- **Export All** depuis l'onglet Projet (toutes les songs en une passe)
- Export projet en lot : songs + `project_instruments.c` + `project_sfx.c`
- Export projet: symboles songs namespaced + `project_audio_manifest.txt` + API C auto (`project_audio_api.h/.c`)
- Banque SFX projet complete: tone/noise on/off, channel, frames, sweep, envelope, burst
- Outils de niveau projet: **Analyser niveau song**, **Normaliser song active**, **Normaliser SFX projet** (offset attenuation)
- **Multi-pattern / Song mode** : jusqu'a 64 patterns, liste d'ordre configurable, point de boucle
- Grille 4 canaux (3 tone + 1 noise), 16-128 lignes par pattern
- En-tete a 2 niveaux : nom du canal + sous-colonnes (Note, In, Vo, FX)
- Saisie clavier AZERTY / QWERTY (2 octaves, reference affichee en temps reel)
- Playback 60fps avec effets instruments (enveloppe, vibrato, sweep, pitch curve)
- Effets par cellule : Arpege (0xy), Pitch slide (1xx/2xx), Portamento (3xx), Volume slide (Axy), Set speed (Bxx), Note cut (Cxx), Note delay (Dxx)
- Commandes FX reservees/non implementees dans ce runtime: `5xx..9xx`
- **Loop Selection** : boucler la lecture sur une selection de lignes
- Undo/Redo (100 niveaux)
- Copy / Cut / Paste, Transpose (+/-1, +/-12 demi-tons)
- Selection souris (clic, drag, shift-clic, double-clic) et clavier (Shift+fleches)
- Selection multi-canal (Ctrl+Shift+A)
- Insert / Delete / Duplicate row, Interpolation attenuation
- Mute / Solo par canal (boutons + F1-F4)
- Follow mode, Record mode, center-scroll, cursor wrap
- Instrument color coding (16 couleurs, cycliques), volume bars, beat hierarchy visuelle
- Scrollbar cliquable avec indicateur de position playback
- Menu contextuel complet (clic droit)
- Save / Load pattern (.ngpat JSON)
- **Save / Load Song** (.ngps JSON — multi-pattern + ordre + loop)
- **Export WAV** : rendu offline en fichier audio PCM 16-bit mono 44100 Hz
- **Import MIDI** : import natif .mid/.midi avec allocation voix et conversion automatique
- **Export C / ASM** deux modes :
  - **Pre-baked** : simulation tick-by-tick, fidelite parfaite tracker = jeu
  - **Hybride** : opcodes instrument (0xF0..0xF4 + 0xF6/0xF7/0xF8/0xF9/0xFA), streams compacts, effets driver
- **Audit export** automatique: checks bornes/IDs/FX + warnings clairs (log + commentaire dans l'export)
- **Debug runtime par canal** avec bouton `Dbg RT` (dump note/divider/attn/fx runtime par row)
- **PlayerTab aligne** : traite les opcodes du driver (attenuation, env/vib/sweep, inst, host cmd, expression, ADSR, LFO)
- Correctif volume/fade documente: le driver reset l'etat de fade sur `Bgm_Start*`/`Bgm_Stop`, et `Bgm_FadeOut(0)` annule + reset le fade
- Copy as text (Ctrl+Shift+C) pour partage forum/chat
- Affichage noise explicite (P.H/P.M/P.L/P.T/W.H/W.M/W.L/W.T — type + rate)
- Barre de status (note, instrument, attenuation, selection)
- BPM display

### Instruments
- Jusqu'a 128 instruments editables (00-7F)
- Code tracker explicite par slot (`0x00..0x7F`) affiche dans l'editeur
- Renommage direct du slot (champ Nom + bouton appliquer)
- Overwrite cible: appliquer un preset factory dans le slot courant
- Reset du slot courant (defaut) et reset complet de la banque factory
- Stats projet dans l'onglet Projet: **total / custom / modifies**
- Enveloppe configurable (courbes pre-definies)
- Vibrato (depth, speed, delay)
- Sweep (direction, step, speed)
- Pitch curves
- Preview en temps reel
- **Loop Preview** : lecture en boucle avec mise a jour des parametres en temps reel

### Aide integree
- 13 sections de tutoriel dans l'application
- Ecrit pour les debutants absolus (aucune connaissance musicale requise)
- Concepts musicaux, instruments, tracker, clavier, edition, avance, playback, export, FAQ

### Autre
- Player MIDI / BGM avec driver SNK
- PlayerTab: preview MIDI force en **Hybride opcodes driver-like** (profil export toujours selectable)
- SFX Lab pour tests tone/noise + preview complet (frames/sweep/env/burst + tone ADSR5/LFO1/LFO2) + sauvegarde SFX projet
- Meter audio simple (peak + indicateur clip) dans Player et SFX Lab
- Debug PSG / Z80 pas-a-pas
- Emulation PSG T6W28 complete
- Moteur Z80 integre

---

## Etat du projet / Roadmap

**Fait :**
- Mode Projet + Edition libre, gestion multi-morceaux dans un dossier projet
- Autosave configurable + sauvegarde fermeture
- Export projet en lot (tous les morceaux) + banque d'instruments globale + banque SFX projet
- Export All Hybride: auto-merge banque instruments (dedup) + remap IDs `0xF4` dans les streams songs
- Export projet selectif (songs seuls, instruments seuls, SFX seuls, all)
- Choix explicite du mode songs a l'export Projet (Hybride recommande novice / Pre-baked avance)
- Pipeline projet (base): manifest export + API C + namespacing symboles songs
- Pipeline projet one-click: switch/runtime `NOTE_TABLE` par song via `Bgm_SetNoteTable` + helper API C `NgpcProject_BgmStartLoop4ByIndex`
- Audit export tool vs driver: checks bornes/IDs/FX + warnings clairs (log + commentaire dans fichiers exportes)
- Debug playback runtime par canal (toggle `Dbg RT`: note/divider/attn/fx runtime par ligne)
- Mixing/levels (base): meter peak/clip visible en temps reel dans Player et SFX Lab
- Mixing/levels: normalisation per-song (analyse peak offline + offset attenuation explicite) + normalisation banque SFX (offset global tone/noise)
- Tracker complet avec edition, playback, export C, save/load
- Multi-pattern / Song mode (64 patterns, liste d'ordre, point de boucle)
- Export WAV (rendu offline PCM 16-bit mono 44100 Hz)
- Export C/ASM double mode : "pre-baked" (tick-by-tick, fidelite parfaite) + "hybride" (opcodes instrument, streams compacts)
- PlayerTab aligne avec le driver (traite opcodes 0xF0-0xFA, effets instrument en temps reel)
- LFO `0xFA` implemente de bout en bout (driver + export hybride + preview tool)
- Import MIDI natif (allocation voix, conversion tempo, multi-pattern, mapping GM drum)
- Loop Selection (boucle sur lignes selectionnees)
- Loop Preview instruments (lecture en boucle avec MAJ parametres)
- Jusqu'a 128 instruments avec enveloppe, ADSR, LFO pitch, vibrato, sweep, pitch curve (mode tone + noise)
- Effets par cellule : arpege, pitch slides, portamento, volume slide, set speed, note cut, note delay
- Outils batch tracker : interpolation contextuelle (Inst/Attn/FX param), humanize attn, batch apply, templates rapides (Ctrl+T)
- Noise channel : 8 configurations explicites (rate + type), saisie clavier dediee, combo box descriptif
- Preview audio driver-faithful unifie : Player force en hybride driver-like, SFX Lab force en driver-like, preview note Tracker aligne instrument
- ADSR complet AR/DR/SL/SR/RR implemente de bout en bout (instrument, export, preview, driver)
- Double LFO (LFO1+LFO2) + algorithm routing 0..7 implementes (instrument, export, preview, driver)
- Robustesse buffer PSG amelioree (pending+commit shadow, retry auto des commandes non envoyees)
- Ecriture noise control rationalisee cote driver (evite reseed LFSR inutile quand seul l'attn change)
- Aide integree complete (13 sections)

**A faire (upgrades priorises) :**
- Edition tracker avancee : interpolation automations (attn/pitch/fx), humanize leger (restant: templates enrichis + batch ops supplementaires)
- Tests de non-regression export/driver (format export, bornes parametres, compatibilite)
- Parite SNK/K1 (audio) restant:
  - Clarifier/valider le mapping "white/pink" du tool SNK vs periodic/white PSG T6W28 (UI + export + doc)
  - Calibration niveaux tone/noise (comparatif tool vs emu vs hardware) + presets de validation prebacked/hybrid
- PSG preview fidelity (ex-PSG_NOTES centralise, restant):
  - P1 (preview parity): review mono mixer gain staging in `PsgMixer::render()` (`(tone + noise) / 2`) and validate against hardware/emu captures before changing.
  - P1 (preview quality): finaliser l'audit anti-redondance noise control dans tous les chemins UI (Player/Tracker/SFX/preview direct), avec cache `rate/type` uniforme.
  - P2 (API cleanup): split helper API into explicit noise-mode write vs noise-attenuation write to prevent accidental LFSR reseed patterns from repeated register-6 writes.
  - P2 (diagnostics): add debug counters for noise control writes and RNG reseed events to make 60 Hz repetition issues measurable during tests.
  - P2 (hardware fidelity): verify white-noise LFSR polynomial/seed for T6W28 on hardware references; keep NeoPop-compatible defaults until confirmed.
  - P3: add targeted tests (long noise continuity, tone+noise level calibration vs reference captures, regression checks for note-table playback and existing exports).
  - P3: add stereo if needed (currently mono).

**Hors scope V1 (volontaire) :**
- Import/lecture directe **VGM** dans le workflow principal (priorite au tracker + MIDI + export projet).
- Lecture **DAC/PCM samples** dans ce driver PSG.
- **PAN stereo runtime** (`0xF5`) : reserve dans le format stream, actuellement no-op cote driver (comportement mono-safe).
- Compatibilite "universelle" avec des drivers tiers : la parite est ciblee sur `driver_custom_latest`.

---

## Architecture

```
app/src/
  main.cpp
  MainWindow.cpp/.h
  audio/
    AudioOutput.cpp/.h             -- sortie QtMultimedia
    EngineHub.cpp/.h                -- pont entre Z80/PSG et l'UI
    PsgHelpers.cpp/.h               -- ecriture directe registres PSG
    InstrumentPlayer.cpp/.h         -- lecture d'instruments avec effets
    TrackerPlaybackEngine.cpp/.h    -- moteur de playback reutilisable (voix, effets, tick/row)
    WavExporter.cpp/.h              -- rendu offline + ecriture fichier WAV
    MidiImporter.cpp/.h             -- import MIDI natif (parsing + conversion)
  models/
    ProjectDocument.cpp/.h   -- metadonnees projet (songs, autosave, actif)
    TrackerDocument.cpp/.h   -- modele d'un pattern (grille, undo, clipboard, JSON)
    SongDocument.cpp/.h      -- morceau complet (banque patterns + ordre + loop)
    InstrumentStore.cpp/.h   -- banque d'instruments (max 128)
  widgets/
    ProjectStartDialog.cpp/.h -- dialogue create/open projet au lancement
    TrackerGridWidget.cpp/.h -- rendu QPainter de la grille tracker
    EnvelopeCurveWidget.cpp/.h -- visualisation des courbes d'enveloppe
  tabs/
    ProjectTab.cpp/.h        -- gestion songs/autosave/export projet
    PlayerTab.cpp/.h         -- lecteur MIDI/BGM
    TrackerTab.cpp/.h        -- controleur du sequenceur (song mode, order list)
    InstrumentTab.cpp/.h     -- editeur d'instruments (loop preview)
    SfxLabTab.cpp/.h         -- labo SFX
    DebugTab.cpp/.h          -- debug PSG/Z80
    HelpTab.cpp/.h           -- aide integree (tutoriel complet)
core/
  include/ngpc/              -- types partages (instrument, PSG, midi)
  src/                       -- emulateur Z80 + PSG T6W28
```

## Depannage rapide

- `Permission denied` au link de `ngpc_sound_creator.exe`:
  fermer toute instance de l'application, puis relancer le build.
- Pas de son dans un scenario de preview:
  verifier le device audio systeme et relancer Play/Preview du tab courant.
- Export hybride qui sonne differemment en jeu:
  verifier que le jeu utilise bien le **Pack Driver NGPC** (`driver_custom_latest`).
- `ISCC.exe` introuvable:
  installer **Inno Setup 6** (le zip portable reste utilisable sans installateur).

## Licence
- Qt 6 : GPL/LGPL (option GPL utilisee ici)
- Z80 core (Marat Fayzullin) : restriction non-commerciale -- remplacer si distribution commerciale
