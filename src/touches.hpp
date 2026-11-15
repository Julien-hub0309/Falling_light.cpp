// FALLING LIGHT - touches configurables : chaque action a 2 emplacements (clavier ou souris).
#pragma once
#include "common.hpp"

namespace T {
enum Action { AVANT, ARRIERE, GAUCHE, DROITE, SAUT, SPRINT, GLISSE, TIR, VISER, RECHARGE, MELEE, ARME1, ARME2, TACTIQUE, GRAPPIN, TITAN, INTERAGIR, SOIN, ARMURE,
              ORDRE_SUIVRE, ORDRE_GARDER, ORDRE_ALLER, ORDRE_ATTAQUER, REJOUER, NB };
constexpr int SOURIS = 1000;                 // code >= 1000 : bouton de souris (1000 + MOUSE_BUTTON_x)
bool tenu(Action a);                         // maintenue
bool presse(Action a);                       // appuyée à cet instant
const char* id(Action a); const char* libelle(Action a);
int code(Action a, int slot);                // 0 = aucun
void affecter(Action a, int slot, int code); // retire ce code des autres actions (pas de doublon)
void defauts(); void defauts(Action a);
std::string nom_code(int c);                 // « W », « ESPACE », « CLIC GAUCHE »...
std::string nom(Action a);                   // premier emplacement non vide
int capter();                                // renvoie le code de la prochaine touche/bouton pressé, -1 si rien, -2 si Échap
void ecrire(FILE* f); bool lire_ligne(const char* ligne);
}
