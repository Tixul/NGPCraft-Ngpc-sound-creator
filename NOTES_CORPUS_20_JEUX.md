# Ce que 20 jeux commerciaux disent de cet outil

Mesure du 2026-09-23 : chaque écriture atteignant le T6W28 a été capturée, avec
horodatage cycle, sur **20 jeux NGPC commerciaux** (10 s de musique chacun), puis les
registres reconstruits depuis ce flux. Dossier complet, outillage et relevés :
`NGPC_RAG/04_MY_PROJECTS/Doc de dev/recherche_bgm/` (12 documents).

Cette note ne résume pas le corpus — elle liste **ce qui concerne ce dépôt**.

---

## ✅ Fait

### Presets d'usine recalibrés — `core/src/instrument.cpp`

Deux défauts mesurés, corrigés dans `FactoryInstrumentPresets()` :

| | avant | après | référence |
|---|---|---|---|
| sustain médian de la banque | 10 | **6** | atténuation tenue médiane du corpus : **4** |
| LFO de profondeur > 2 | 5 presets | **0** | vibrato du corpus : ±4-6 unités, **±17 cents max** |
| LFO en onde carrée | 2 presets | **0** | le corpus est en triangle |
| instruments | 32 | **38** | +6 relevés dans les jeux (`Corpus Pluck/Plateau/Lead/Nappe/Bass/Staccato`) |

**Pourquoi ça comptait :** les sustains de 7 à 12 faisaient jouer les instruments 8 dB
sous le niveau des jeux commerciaux ; et les LFO, dont la profondeur est exprimée en
**unités de diviseur**, modulaient jusqu'à **196 cents (deux demi-tons) à C6** — un lead
qui monte sonnait faux. Le même réglage vaut 12 cents à E2 : le défaut est structurel,
pas un mauvais réglage isolé.

⚠️ **Le piège** : les LFO agressifs ne sont **pas dans la table** de presets. Ils sont
posés par le post-traitement qui la suit (`presets[4]`, `[5]`, `[8]`, `[25]`, `[27]`).
Corriger la table seule n'aurait rien changé.

Vérification faite en **exécutant** `FactoryInstrumentPresets()`, pas en lisant le diff.

⚠️ `instruments.json` vit à la racine de chaque projet : les projets **existants gardent
leur banque**. Seuls les nouveaux partent de la banque corrigée. Pour corriger un projet
existant, y déposer `recherche_bgm/data/compo/instruments_corpus.json`.

---

## Ce qui reste, par rapport bénéfice / coût

### 1. Un bit de côté L/R dans la commande Z80 — *fort bénéfice, coût faible*

Le blob Z80 (`driver_custom_latest/sounds.c`) écrit **chaque octet aux deux portes** :

```asm
ld a,(hl) : ld (0x4001),a : ld (0x4000),a    ; ×3 par commande
```

Trois conséquences mesurées :

- **La chaîne est mono par construction.** 12 jeux sur 20 pannent, avec une règle simple :
  voie 2 centrée (17/20), voies 0 et 1 écartées de ±1 à ±4 crans (2 à 8 dB), écart constant
  sur tout le morceau. `BgmInstrumentDef` n'a d'ailleurs aucun champ de panoramique.
- **La période de bruit indépendante du T6W28 est inatteignable.** Écrire la période de la
  voie 2 l'écrase, donc notre mode bruit 3 se comporte comme un SN76489. Dix jeux sur vingt
  s'en servent pour une batterie accordée à trois timbres **sans sacrifier la voie 2**.
- **Le bruit périodique accordé** (un jeu du corpus l'utilise comme quatrième voix
  mélodique) est hors de portée pour la même raison.

Un bit de côté par commande (`0` = les deux portes, `1` = gauche, `2` = droite) lève les
trois d'un coup : ~15 octets de blob et un champ `pan` signé dans l'instrument.

### 2. `SND_BUF_MAX 5` — un plafond qui se paie en notes perdues

`sounds.c` empile au plus **5 commandes de 3 octets par trame**, soit 15 octets. Les jeux
commerciaux émettent **11,8 à 47,9 écritures par trame** (pic mesuré 62).

C'est tout juste de quoi rafraîchir les 4 voies une fois par trame, sans marge pour un
bruitage simultané. Et le dépassement n'est pas mis en attente : `WaitBufferFree()` est
non bloquant, il **jette** le paquet et incrémente `s_sound_drops`. Une commande jetée =
un volume figé ou une note qui ne démarre pas.

Deux choses à faire : agrandir `SND_BUF_MAX` (la RAM Z80 partagée est large, la boucle
`djnz` du blob gère déjà un compteur quelconque), et **rendre `s_sound_drops` visible dans
l'outil** — un compteur de drops pendant la lecture dirait immédiatement qu'un morceau est
trop dense pour le transport.

### 3. Ergonomie : afficher la durée du pas

Le tempo est bien dans le `.ngps` (effet `0xB` = trames par ligne, défaut 8 = 133 ms).
Mais à 60 Hz, seuls les multiples de 16,7 ms tombent juste : un pas de 48 ms vaut
2,9 trames et la grille devient irrégulière. Afficher **la durée du pas en ms et le BPM**
à côté du réglage éviterait de choisir une grille que la console ne peut pas tenir.

### 4. Cadence : le VBlank n'est pas ce que font les jeux

`Bgm_Update()` avance sur `VBCounter` → **60 Hz**. Les 20 jeux rafraîchissent le chip à
**~245 Hz** (4,1 fois par trame). Le mécanisme officiel, lu dans le désassemblage K1SND,
est **plus simple que le nôtre** : l'IRQ Z80 (7,8 kHz) incrémente un compteur, la boucle
principale lit ce compteur et avance la musique quand un accumulateur déborde —
`Σ(ΔIRQ × 4 × vitesse) ≥ 1250`, soit `25,08 × vitesse` Hz. **Le tempo est un entier
réglable**, pas une constante.

⚠️ À relativiser : une musique **composée** dans l'outil sonne juste sur console — c'est
constaté. Le 60 Hz n'est pas un défaut de fidélité, c'est un **vocabulaire plus étroit**
(pas de ré-attaque toutes les 35-45 ms, pas de note de 16 ms, pas de vibrato à 15-20 Hz).
Ce chantier ouvre le vocabulaire ; il ne répare rien d'urgent. Il dépend du point 2.

### 5. Autres pistes

- **Bouton « réinitialiser cet instrument aux valeurs d'usine »** — complément naturel de
  la correction des presets.
- **Sélecteur / import de banque** — utile pour comparer, mais ne répare pas un défaut :
  celui qui ouvre l'outil pour la première fois ne sait pas qu'il doit changer de banque.
- **Le bruit périodique** reste le coin le moins exploré du chip (2 jeux sur 20). Le champ
  existe déjà (`noise_config`, bit de type) ; l'accorder demande le point 1.

---

## Deux choses utiles à savoir pour travailler sur ce dépôt

**La ré-attaque est le procédé de pulsation de tout le corpus** (relancer la même note
toutes les 8-11 ticks), et elle est **inaudible** sur un instrument dont le niveau ne
redescend jamais — Clean Tone et Pulse Organ tiennent un niveau plat. C'est un piège
d'ergonomie : ces instruments semblent corrects et ne peuvent pas pulser.

**Le diviseur de ton fait 10 bits** → plancher à **~94 Hz (F#2)**. En dessous il sature et
toutes les notes graves sonnent la même hauteur. Le corpus s'arrête à A2 (110 Hz). L'outil
pourrait le signaler au moment de la saisie.

---

## Outillage disponible (hors de ce dépôt)

Dans `recherche_bgm/tools/` : écriture d'un `.ngps` par script et **rendu en WAV**
(`ngps_tools.py`, séquenceur 60 Hz + ADSR + enveloppe + vibrato + les deux LFO portés
depuis `sounds.c`), vérification d'une compo contre le corpus (`verifier_compo.py`),
export C **sans passer par l'interface** (`ngps_export.py`), génération de la banque
(`banque_corpus.py`), et confrontation à une ROM réellement compilée (`valider_rom.py` —
96 hauteurs sur 96 identiques).
