# FALLING LIGHT — version C++ / raylib

Port complet de la version Python (Ursina) : même carte, mêmes modes, mêmes règles, mêmes constantes.

## Compiler et lancer

**Windows** : installer CMake + Visual Studio (ou MinGW), puis double-clic sur `build.bat`.
L'exécutable est dans `build\Release\falling_light.exe`.

**macOS** : `brew install cmake`, puis `./build.sh` et `./build/falling_light`.

**Linux** : `sudo apt install cmake build-essential libgl1-mesa-dev libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev`
puis `./build.sh` et `./build/falling_light`.

Le premier build télécharge et compile raylib automatiquement (connexion internet requise une fois).

## Contenu
- Modes : Classique (10 min, Titans, pilotes IA), Point de contrôle, Parcours (couteau seul), **Match à mort par équipe**, **Mêlée générale**. Jour ou nuit.
- Carte 360 m x 360 m générée avec le même générateur aléatoire que Python (graines 3 et 11), enceinte et skyline comprises.
- 4 classes (Assaut, Tactic, Sniper, Brèche), armes, couteau, épée de Titan, grenades frag / fumigènes.
- Mouvement : sprint, saut, double saut, course sur mur, glissade, grimpe, grappin.
- Titan : TITANFALL, embarquement, éjection, ordres (suivre / garder / aller / attaquer).
- Zombies de jour / spéciaux, de nuit / rapaces / géants, en groupes avec alerte au bruit.
- Craft par loots (amélioration mêlée, arme de classe, augmentation, kits de soin / armure, grenades).
- Rendu : ombres dynamiques du soleil, éclairage jour/nuit, ciel procédural (dégradé, nuages, étoiles, soleil et lune), lueur (bloom) sur les lumières et les fenêtres la nuit, anti-crénelage FXAA, relief sur les façades, reflets sur le verre et les métaux, brume atmosphérique, textures détaillées (fenêtres avec cadres et rebords, briques, asphalte usé, dalles), effets lumineux (tirs, explosions).

## Match à mort par équipe et Mêlée générale
- **Match à mort par équipe** : 2 équipes de 10 pilotes (bleue et orange, tu es dans la bleue). **Mêlée générale** : 10 pilotes, chacun pour soi.
- Pas de loot. Chacun démarre avec son arme de classe et son couteau (sans loot, il n'y aurait aucun moyen d'obtenir une arme). **Munitions infinies** : la réserve ne s'épuise jamais, on recharge simplement le chargeur (affichage « INF »). Les grenades et fumigènes restent en nombre limité. Les zombies et le cycle jour/nuit sont conservés.
- Points : **pilote tué +5, Titan détruit +10, zombie tué +1**. La partie se termine dès qu'une équipe (ou un pilote en mêlée) atteint ou dépasse **150 points**, ou après 10 minutes : le plus haut score gagne.
- On réapparaît toujours quelques secondes après sa mort, sans perdre de points : en équipe sur la base de son camp (bleus à l'ouest, oranges à l'est), en mêlée loin des autres pilotes. Pas de tir allié.
- Les points marqués comptent pour la victoire ; les points « dépensables » (Titan à 50 pts) sont comptés à part, donc appeler un Titan ne fait pas reculer ton équipe.
- Les zombies sont remplacés au goutte-à-goutte (un groupe toutes les 12 s) pour rester une menace sans devenir une réserve de points. Réglages dans `src/partie.hpp` : `SCORE_VICTOIRE`, `PTS_PILOTE`, `PTS_TITAN`, `PTS_ZOMBIE`.

## Touches
ZQSD / WASD bouger · Espace saut / grimper · Maj sprint · Ctrl ou C glisse · X grappin
Clic gauche tir · Clic droit viser · R recharge · V mêlée · 1 / 2 ou molette armes · G grenade
F appeler le Titan · E embarquer / éjecter · 3 / 4 / 5 / 6 ordres du Titan · H kit de soin · B kit d'armure
Échap options (sensibilité, FOV, graphismes, plein écran) et **CHANGER LES TOUCHES** · F11 plein écran · F12 capture · R après élimination/fin : retour au menu

## Changer les touches
Échap → **CHANGER LES TOUCHES**. Chaque action a deux cases (clavier ou souris) : clique sur une case, appuie sur la touche ou le bouton voulu (Échap annule, Suppr efface la case). Une touche déjà utilisée ailleurs est retirée de l'ancienne action. **RÉINITIALISER** remet les touches par défaut.
Les réglages sont enregistrés dans `falling_light.cfg` (à côté de l'exécutable) : on peut aussi les éditer à la main, une ligne `touche <action> <code1> <code2>` par action (codes raylib ; souris = 1000 + bouton).
Les noms affichés suivent la disposition QWERTY : sur AZERTY, Z s'affiche W, Q s'affiche A, A s'affiche Q.
Le code est dans `src/touches.cpp` (liste des actions et touches par défaut en haut du fichier).

## Qualité graphique
Échap → Options : **Graphismes (préréglage)** bas / moyen / haut, puis réglage fin des **ombres** (non / moyennes / hautes) et de la **lueur + anti-crénelage**. Le préréglage par défaut est « moyen ».
- **bas** : éclairage simple, sans ombres ni post-traitement (petites configurations).
- **moyen** : ombres 1024, lueur, anti-crénelage, relief, reflets.
- **haut** : ombres 2048 plus douces et plus étendues.
Si la carte graphique ne gère pas les textures HDR, le jeu retombe automatiquement sur un rendu 8 bits ; s'il ne peut pas créer les cibles de rendu, il désactive le post-traitement. Les réglages sont dans `falling_light.cfg` (`qualite`, `ombres`, `post`).
Le code du rendu est dans `src/rendu.cpp` : shaders en haut du fichier, textures procédurales au milieu, post-traitement et ombres à la suite.

## Navigation des bots et zombies
Pathfinding A* sur la carte réelle des collisions (une grille par niveau de toit et une grille pour les Titans), descente des escaliers, contournement des obstacles, détection de blocage et recalcul de route. Les socles d'immeubles, lampadaires et feux sont solides, les gravats n'encombrent plus les routes, et les points d'apparition sont vérifiés (jamais dans une zone fermée).
Outil de contrôle : `tools/diag.cpp` simule une partie sans affichage et signale les entités coincées ou incrustées dans la géométrie
(`g++ -std=c++17 -O2 -DFL_DIAG -Isrc src/*.cpp tools/diag.cpp -lraylib ... -o diag` puis `FL_DIAG=classique:nuit FL_FRAMES=15000 ./diag`).

## Différences avec la version Python
- Pas de multijoueur LAN ni d'installateur (non portés).
- Le repère de raylib est inversé par rapport à Ursina : la carte est donc l'image miroir (gauche/droite) de celle de Python.
