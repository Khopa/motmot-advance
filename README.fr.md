# MotMot Advance

*English version: [README.md](README.md)*

Un jeu de lettres pour Game Boy Advance dans l'esprit de l'émission *Motus* :
trouver un mot de 5 lettres en 6 essais, feedback vert / jaune / gris sur
chaque lettre, clavier virtuel au pad, deux langues (français, anglais),
effets sonores chiptune, statistiques sauvegardées en SRAM et un mode
Marathon avec des vies et des records. Écrit en C avec libtonc, sans assembleur ni C++.

| Titre | Menu | Partie | Marathon | Résultat |
|---|---|---|---|---|
| ![](docs/title.png) | ![](docs/menu.png) | ![](docs/game.png) | ![](docs/marathon.png) | ![](docs/result.png) |

## Fonctionnalités

- Choix de la langue au démarrage, écran titre, menu (mode, statistiques,
  langue, son).
- **Mode Classique** : mot tiré au hasard parmi ~500 mots courants, sans
  répéter les 8 derniers mots joués.
- **Mode Marathon** : des mots aléatoires enchaînés, avec des vies et un
  record sauvegardé par difficulté. Facile : 3 vies, un mot raté en coûte
  une. Difficile : 5 vies, chaque essai à partir du troisième en coûte une —
  il faut trouver vite.
- Validation des mots contre une liste large (~8 700 mots en anglais,
  ~6 600 en français, accents retirés comme dans l'émission).
- Clavier AZERTY en français, QWERTY en anglais, avec touches Entrée / Effacer.
- Effets sonores sur les générateurs de son Game Boy : clic de touche, buzzer
  sur un mot inconnu, une note différente par couleur révélée, fanfare de
  victoire, jingle de défaite. Désactivables dans le menu (sauvegardé).
- Statistiques persistantes (SRAM) : parties, victoires, série en cours,
  meilleure série, répartition par nombre d'essais, records du Marathon.
- Interface entièrement traduite dans la langue choisie.

## Contrôles

| Touche | Action |
|---|---|
| D-pad | Déplacer le curseur sur le clavier virtuel / naviguer dans les menus |
| A | Saisir la lettre sélectionnée (ou activer Entrée / Effacer sur le clavier) |
| B | Effacer la dernière lettre |
| START | Valider le mot |
| SELECT | Quitter la partie (avec confirmation) |
| Gauche / Droite (menu) | Modifier l'option sélectionnée (langue, difficulté du Marathon, son) |

## Compilation

Prérequis : [devkitPro](https://devkitpro.org/wiki/Getting_Started) avec le
groupe `gba-dev` (devkitARM, libtonc, gbafix), GNU make, Python 3 avec
[Pillow](https://pypi.org/project/pillow/) (génération des tuiles).

```sh
make            # -> build/motmot.gba
make run        # lance la ROM dans mGBA (variable MGBA pour changer le chemin)
make test       # tests unitaires de la logique, compilés avec le gcc hôte
make smoke      # test de bout en bout dans mGBA (voir plus bas)
make clean
```

Sous Windows, lancer `make` depuis le shell MSYS2 fourni par devkitPro
(`DEVKITPRO=/opt/devkitpro`). Si Python n'est pas dans le PATH de ce shell :
`make PYTHON="py -3"`.

La ROM déclare une sauvegarde SRAM (`SRAM_V113`) ; mGBA et les linkers de
flashcart la détectent automatiquement.

## Chaîne d'outils maison (pas de grit)

Tout ce qui est généré l'est à la compilation, dans `build/gen/` :

| Script | Rôle |
|---|---|
| `tools/make_assets.py` | Dessine les images sources `assets/*.png` (PNG indexés) à partir de pixel-art ASCII : police 6×7 (A-Z, 0-9, ponctuation, icônes Entrée/Effacer, segment de barre), cases 16×16 de la grille et touches arrondies du clavier avec la lettre pré-composée, sprite curseur, motif de fond du titre. `make assets` pour les régénérer. |
| `tools/png2gba.py` | Convertit un PNG en tuiles 4bpp + palette BGR555 sous forme de tableaux C. Option `--meta 2 2` (via `assets/<nom>.opts`) pour émettre les tuiles par métatuile 16×16. |
| `tools/gen_wordlist.py` | Transforme `data/<langue>_solutions.txt` et `data/<langue>_valid.txt` en tableaux C : solutions et mots valides triés (recherche dichotomique). |
| `tools/build_wordlists.py` | Reconstruit les fichiers `data/*.txt` depuis les sources externes (réseau nécessaire, `make wordlists`). |

Les couleurs (vert / jaune / gris / neutre) ne sont pas dans les tuiles : une
seule tuile par lettre, colorée par bank de palette (`SE_PALBANK`).

## Architecture

```
source/main.c        boucle de jeu, machine à états : langue / titre / menu / jeu / résultat / stats
source/logic.c       règles du jeu (score avec lettres répétées, validation, saisie) — sans dépendance matérielle
source/lang.c        table des langues : listes de mots, disposition clavier, textes
source/render.c      Mode 0 : BG0 texte, BG1 grille + clavier, BG2 motif titre, sprite curseur
source/sound.c       effets sonores PSG : séquenceur à pas sur carré 1, carré 2 et bruit
source/keyboard.c    navigation du curseur sur le clavier virtuel
source/input.c       lecture des touches, auto-répétition du D-pad
source/stats.c       lecture / écriture SRAM (statistiques, état du RNG, options)
source/rng.c         xorshift32
include/game_state.h état d'une partie (mot cible, essais, feedback, clavier)
include/stats.h      structure sauvegardée
```

Mémoire vidéo : charblock 0 = police, charblock 1 = cases et touches,
charblock 2 = motif ; screenblocks 28/29/30 ; l'unique sprite est le curseur.

## Tests

- `make test` : `tests/test_logic.c` compile `logic.c` avec le compilateur de
  l'hôte et vérifie le scoring (dont les lettres doublées), la validation, la
  saisie, la victoire / défaite et la mise à jour des couleurs du clavier.
- `make smoke` : `tests/smoke.py` pilote la ROM dans mGBA avec un script Lua
  (nécessite un mGBA avec l'option `--script`, disponible dans les builds de
  développement 0.11 : `--mgba` pour indiquer le chemin). Le script lit
  l'état du jeu en RAM (adresses tirées de l'ELF), tape des mots au clavier
  virtuel, gagne une partie, en perd une, vérifie les statistiques, annule
  puis confirme une sortie, joue des Marathons dans les deux difficultés,
  surveille les registres son, puis redémarre la ROM
  pour vérifier la persistance SRAM. Les captures d'écran vont dans
  `tests/out/`.

## Sources des listes de mots

- Anglais : [an-array-of-english-words](https://github.com/words/an-array-of-english-words)
  (MIT) comme dictionnaire ; classement par fréquence via
  [FrequencyWords](https://github.com/hermitdave/FrequencyWords)
  (OpenSubtitles, CC BY-SA 4.0) ; la liste
  [google-10000-english](https://github.com/first20hours/google-10000-english)
  pour ne garder que des mots courants comme solutions ; une liste de prénoms
  pour écarter les noms propres.
- Français : [Lexique 3.83](http://www.lexique.org) (CC BY-SA 4.0) pour les
  formes, lemmes, catégories grammaticales et fréquences ;
  [an-array-of-french-words](https://github.com/words/an-array-of-french-words)
  (MIT) pour élargir la liste des mots acceptés.

Les solutions sont les 500 mots courants les plus fréquents (français : noms,
adjectifs, verbes, adverbes de 5 lettres après suppression des accents), moins
une courte liste d'exclusion.

Code © 2026 Clément Perreau, licence MIT (voir `LICENSE`). Publié par Khopa.
