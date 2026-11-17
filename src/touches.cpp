#include "touches.hpp"
#include <cstring>

namespace T {
struct Def { const char* id; const char* libelle; int d0, d1; };
static const Def DEFS[NB] = {
    {"avant", "Avancer", KEY_W, KEY_UP}, {"arriere", "Reculer", KEY_S, KEY_DOWN}, {"gauche", "Aller à gauche", KEY_A, KEY_LEFT}, {"droite", "Aller à droite", KEY_D, KEY_RIGHT},
    {"saut", "Sauter / grimper", KEY_SPACE, 0}, {"sprint", "Sprint", KEY_LEFT_SHIFT, 0}, {"glisse", "Glisser", KEY_LEFT_CONTROL, KEY_C},
    {"tir", "Tirer", SOURIS + MOUSE_BUTTON_LEFT, 0}, {"viser", "Viser", SOURIS + MOUSE_BUTTON_RIGHT, 0}, {"recharge", "Recharger", KEY_R, 0}, {"melee", "Attaque de mêlée", KEY_V, 0},
    {"arme1", "Arme 1", KEY_ONE, 0}, {"arme2", "Arme 2", KEY_TWO, 0}, {"tactique", "Grenade / fumigène", KEY_G, 0}, {"grappin", "Grappin", KEY_X, 0},
    {"titan", "Appeler le Titan", KEY_F, 0}, {"interagir", "Embarquer / éjecter", KEY_E, 0}, {"soin", "Kit de soin", KEY_H, 0}, {"armure", "Kit d'armure", KEY_B, 0},
    {"ordre_suivre", "Titan : suivre", KEY_THREE, 0}, {"ordre_garder", "Titan : garder", KEY_FOUR, 0}, {"ordre_aller", "Titan : aller au point", KEY_FIVE, 0}, {"ordre_attaquer", "Titan : attaquer", KEY_SIX, 0},
    {"rejouer", "Rejouer / retour menu", KEY_R, 0}};
static int C[NB][2];
static bool init_ = false;
static void init() { if (!init_) { defauts(); init_ = true; } }
void defauts(Action a) { C[a][0] = DEFS[a].d0; C[a][1] = DEFS[a].d1; }
void defauts() { for (int i = 0; i < NB; i++) defauts((Action)i); init_ = true; }
const char* id(Action a) { return DEFS[a].id; }
const char* libelle(Action a) { return DEFS[a].libelle; }
int code(Action a, int slot) { init(); return C[a][slot]; }
void affecter(Action a, int slot, int c) {
    init();
    if (c > 0) for (int i = 0; i < NB; i++) for (int s = 0; s < 2; s++) if (C[i][s] == c && !(i == a && s == slot)) {
        // « rejouer » partage volontairement R avec « recharger » : on ne les considère pas en conflit
        if ((i == REJOUER && a == RECHARGE) || (i == RECHARGE && a == REJOUER)) continue;
        C[i][s] = 0;
    }
    C[a][slot] = c;
}
static bool bas(int c) { if (c <= 0) return false; return c >= SOURIS ? IsMouseButtonDown(c - SOURIS) : IsKeyDown(c); }
static bool haut(int c) { if (c <= 0) return false; return c >= SOURIS ? IsMouseButtonPressed(c - SOURIS) : IsKeyPressed(c); }
bool tenu(Action a) { init(); return bas(C[a][0]) || bas(C[a][1]); }
bool presse(Action a) { init(); return haut(C[a][0]) || haut(C[a][1]); }
std::string nom_code(int c) {
    if (c <= 0) return "-";
    if (c >= SOURIS) { switch (c - SOURIS) { case MOUSE_BUTTON_LEFT: return "CLIC GAUCHE"; case MOUSE_BUTTON_RIGHT: return "CLIC DROIT"; case MOUSE_BUTTON_MIDDLE: return "CLIC MOLETTE"; case MOUSE_BUTTON_SIDE: return "SOURIS 4"; case MOUSE_BUTTON_EXTRA: return "SOURIS 5"; default: return "SOURIS"; } }
    if (c >= KEY_A && c <= KEY_Z) return std::string(1, (char)('A' + (c - KEY_A)));
    if (c >= KEY_ZERO && c <= KEY_NINE) return std::string(1, (char)('0' + (c - KEY_ZERO)));
    if (c >= KEY_KP_0 && c <= KEY_KP_9) return "PAVE " + std::string(1, (char)('0' + (c - KEY_KP_0)));
    if (c >= KEY_F1 && c <= KEY_F12) return "F" + std::to_string(c - KEY_F1 + 1);
    switch (c) {
        case KEY_SPACE: return "ESPACE"; case KEY_ENTER: return "ENTREE"; case KEY_TAB: return "TAB"; case KEY_BACKSPACE: return "RETOUR";
        case KEY_LEFT_SHIFT: return "MAJ GAUCHE"; case KEY_RIGHT_SHIFT: return "MAJ DROITE"; case KEY_LEFT_CONTROL: return "CTRL GAUCHE"; case KEY_RIGHT_CONTROL: return "CTRL DROIT";
        case KEY_LEFT_ALT: return "ALT GAUCHE"; case KEY_RIGHT_ALT: return "ALT DROIT"; case KEY_UP: return "FLECHE HAUT"; case KEY_DOWN: return "FLECHE BAS"; case KEY_LEFT: return "FLECHE GAUCHE"; case KEY_RIGHT: return "FLECHE DROITE";
        case KEY_CAPS_LOCK: return "VERR MAJ"; case KEY_INSERT: return "INSER"; case KEY_DELETE: return "SUPPR"; case KEY_HOME: return "DEBUT"; case KEY_END: return "FIN"; case KEY_PAGE_UP: return "PAGE HAUT"; case KEY_PAGE_DOWN: return "PAGE BAS";
        case KEY_COMMA: return ","; case KEY_PERIOD: return "."; case KEY_MINUS: return "-"; case KEY_EQUAL: return "="; case KEY_SEMICOLON: return ";"; case KEY_APOSTROPHE: return "'"; case KEY_SLASH: return "/"; case KEY_BACKSLASH: return "\\";
        case KEY_LEFT_BRACKET: return "["; case KEY_RIGHT_BRACKET: return "]"; case KEY_GRAVE: return "`"; case KEY_KP_ADD: return "PAVE +"; case KEY_KP_SUBTRACT: return "PAVE -"; case KEY_KP_ENTER: return "PAVE ENTREE";
    }
    return "TOUCHE " + std::to_string(c);
}
std::string nom(Action a) { init(); for (int s = 0; s < 2; s++) if (C[a][s] > 0) return nom_code(C[a][s]); return "-"; }
int capter() {
    if (IsKeyPressed(KEY_ESCAPE)) { while (GetKeyPressed()) {} return -2; }
    for (int b = 0; b < 7; b++) if (IsMouseButtonPressed(b)) return SOURIS + b;
    int k = GetKeyPressed(); int dernier = -1; while (k) { dernier = k; k = GetKeyPressed(); }
    return dernier;
}
void ecrire(FILE* f) { init(); for (int i = 0; i < NB; i++) fprintf(f, "touche %s %d %d\n", DEFS[i].id, C[i][0], C[i][1]); }
bool lire_ligne(const char* l) {
    char nom_[40]; int a, b; if (sscanf(l, "touche %39s %d %d", nom_, &a, &b) != 3) return false;
    for (int i = 0; i < NB; i++) if (!strcmp(DEFS[i].id, nom_)) { init(); C[i][0] = a; C[i][1] = b; return true; }
    return false;
}
}
