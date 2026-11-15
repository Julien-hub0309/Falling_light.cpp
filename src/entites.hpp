// FALLING LIGHT - entités : armes, pilotes (joueur + IA), zombies, Titan, grenades, combat.
#pragma once
#include "etat.hpp"
#include <memory>

struct Reglages { float sensibilite = 40, fov = 100, fov_vitesse = .8f; bool inverser_y = false; };
extern Reglages REG;

struct Pilote; struct Titan; struct Zombie; struct Player; struct PiloteIA;

struct Cible {
    V3 pos{0, 0, 0}; float yaw = 0, r = .4f, h = 1.8f, vy = 0; bool vivant = true;
    int kind = 0;                       // 0 pilote, 1 zombie, 2 titan
    float detour_t = 0, detour_side = 1, wd_t = 0, wd_d = 1e9f; V3 wd_p{0, 0, 0};   // contournement d'obstacle + chien de garde anti-blocage
    virtual void damage(float n, Cible* src = nullptr, bool tete = false) = 0;
    virtual ~Cible() {}
};

// ------------------------------------------------------------------ armes
struct Arme {
    std::string nom, modele; float degats = 10, cadence = .1f; int chargeur = 30, munitions = 30, reserve = 0;   // reserve < 0 = infinie
    float portee = 100, dispersion = .02f, t_recharge = 1.5f, zoom = 0, ads_prec = .4f, recul = .05f, pref = 15, ep = .04f;
    bool autom = false, melee = false, titan = false, explosif = false; float exp_rayon = 0, exp_degats = 0;
    int plombs = 1; V3 vue{.32f, -.28f, .7f}; Col trace{1, .85f, .3f, 1}; float cool = 0, rech = 0, bruit = RAYON_BRUIT_TIR;
    void update(float dt) { cool -= dt; if (rech > 0) { rech -= dt; if (rech <= 0) fin_recharge(); } }
    bool pret() const { return cool <= 0 && rech <= 0; }
    bool recharger() { if (melee || rech > 0 || munitions >= chargeur) return false; if (reserve >= 0 && reserve <= 0) return false; rech = t_recharge; return true; }
    void fin_recharge() { int besoin = chargeur - munitions; if (reserve < 0) munitions = chargeur; else { int p = std::min(besoin, reserve); munitions += p; reserve -= p; } }
    void ajouter_munitions(int n) { if (reserve >= 0 && !melee) reserve += n; }
    bool tirer(Cible* owner, V3 origin, V3 d, const V3* debut = nullptr, bool zoomed = false);
};
using ArmeP = std::shared_ptr<Arme>;
namespace Armes {
ArmeP fusil(); ArmeP smg(); ArmeP sniper(); ArmeP shotgun(); ArmeP pistolet(); ArmeP couteau(); ArmeP epee();
ArmeP pour_titan(const ArmeP& a);
ArmeP par_nom(const std::string& n);
}
struct Classe { const char* nom; const char* desc; int primaire; const char* tactique; int nb; float hp, vitesse; };   // primaire : 0 fusil,1 smg,2 sniper,3 pompe,-1 aucune
extern const Classe CLASSES[4]; extern const Classe CLASSE_PARCOURS;
const Classe* classe_aleatoire();
ArmeP arme_classe(const Classe* c);

// ------------------------------------------------------------------ modèles articulés
struct Humano {
    Piece jambe, bras, torse, tete, yeux, fusil; bool pilote; Col couleur_tete, couleur_yeux;
    float a_jambe[2]{0, 0}, a_bras[2]{0, 0}, tete_rz = 0;
};
void humano_construire(Humano& m, Col veste, Col pantalon, Col peau, bool pilote, Col botte = {.1f, .1f, .1f, 1});
void humano_dessiner(Humano& m, V3 pos, float yaw, float scale, float flash);

// ------------------------------------------------------------------ pilotes
struct CraftData { int rouge = 0, orange = 0, bleu = 0, vert = 0, kits_soin = 0, kits_armure = 0, niveau_melee = 0; bool arme_classe = false, augmentation = false; };
struct Pilote : Cible {
    std::string nom; Col couleur; bool elimine = false, attente = false;
    int score = 0, score_final = 0, kills = 0; float t_elim = -1;
    float hp = 100, hp_max = 100, armure = 0; int mode = 0;     // 0 pilote, 1 titan
    Titan* titan = nullptr; float titan_cd = 0, titan_hp = 0, invuln = 0, since_hit = 99, cool_tact = 0;
    const Classe* classe = nullptr; std::vector<ArmeP> armes; int idx = 0;
    int equipe = 0, pts = 0; bool participe = true, arme_depart = false; Col couleur0{1, 1, 1, 1};   // équipe (0 = aucune), points de victoire, présent dans la partie, démarre avec l'arme de classe
    std::string tactique = "frag"; int nb_tactique = 0; float vitesse_mult = 1, mult_melee = 1; CraftData craft;
    Pilote(const std::string& n, Col c) : nom(n), couleur(c), couleur0(c) { kind = 0; r = .4f; h = 1.8f; vivant = false; }
    Arme* arme() { return armes[idx].get(); }
    void nouvelle_partie(const Classe* c, V3 p);
    void reapparaitre(V3 p);
    void mourir() { vivant = false; avant_mort(); }
    virtual void apres_respawn() {}
    virtual void avant_mort() {}
    virtual void sur_degats(Cible*) {}
    virtual void sur_changement_arme() {}
    void damage(float n, Cible* src = nullptr, bool tete = false) override;
    virtual void tick(float dt);
    void choisir_arme(int i) { if (i >= 0 && i < (int)armes.size() && i != idx) { idx = i; sur_changement_arme(); } }
    bool lancer_tactique(V3 origin, V3 vitesse);
    bool titan_actif() const;
    bool appeler_titan();
    ArmeP arme_pour_titan();
    bool donner_ordre(const std::string& ordre);
    bool ramasser(Loot* l);
    void appliquer_classe(const Classe* c);
    virtual V3 avant() const { return {std::sin(rad(yaw)), 0, std::cos(rad(yaw))}; }
};

// kits / craft
bool craft_on_loot(Pilote* p, LootType t, bool& gere);
bool craft_soin(Pilote* p); bool craft_armure(Pilote* p); void craft_auto(Pilote* p);
void donner_arme_classe(Pilote* p);

// ------------------------------------------------------------------ Player
struct Player : Pilote {
    float pitch = 0, bob = 0, flash_t = 0, melee_t = 0;
    V3 vel{0, 0, 0}; bool grounded = true, wallrunning = false, zoom = false, lunette = false;
    int jumps = 1; float wall_t = 0; V3 wall_n{0, 0, 0}; bool has_wall = false;
    float slide_t = 0, slide_v = 0; V3 slide_dir{0, 0, 1};
    bool grimpe = false; V3 g0{0, 0, 0}, g1{0, 0, 0}; float gp = 0;
    bool grappin = false; V3 g_pt{0, 0, 0}; float grappin_t = 0, grappin_cd = 0;
    ArmeP arme_titan; float pivot_y = 1.65f, roll = 0, fov = 100; V3 gun{.32f, -.28f, .7f}; float gun_recul = 0, shake_x = 0, shake_y = 0;
    ArmeP melee_arme; Player();
    ArmeP arme_active();
    void apres_respawn() override; void avant_mort() override; void sur_degats(Cible*) override; void sur_changement_arme() override;
    void damage(float n, Cible* src = nullptr, bool tete = false) override;
    void tick(float dt) override;
    void update(float dt);
    void entrees();              // appuis ponctuels (touches de la frame)
    void embarquer(); void ejecter(bool force = false, Cible* tueur = nullptr);
    bool tente_grimper(); void lancer_grappin(); void fin_grappin(bool boost = false);
    void tirer(); void mouvement(float dt, float f, V3 wish, bool titan); void appliquer_vitesse(float dt, V3 wish, float speed, bool sprint);
    void regarder(); void vol_spectateur(float dt); void animer_grimpe(float dt, V3 wish);
    V3 fwd3() const { return {std::cos(rad(pitch)) * std::sin(rad(yaw)), std::sin(rad(pitch)), std::cos(rad(pitch)) * std::cos(rad(yaw))}; }
    V3 right3() const { return {-std::cos(rad(yaw)), 0, std::sin(rad(yaw))}; }     // droite à l'écran (repère raylib)
    V3 oeil() const { return {pos.x, pos.y + pivot_y, pos.z}; }
    V3 avant() const override { return {std::sin(rad(yaw)), 0, std::cos(rad(yaw))}; }
    void frapper();
};

// ------------------------------------------------------------------ pilotes IA
struct PiloteIA : Pilote {
    Humano m; Cible* cible = nullptr; bool vu = false, marche = false; float pense = 0, flash = 0, react = 0, strafe = 1, strafe_t = 0, bloque = 0, regen_mun = 0, phase = 0;
    std::vector<V3> route; bool a_dest = false; V3 dest{0, 0, 0}; bool veut_zone = false, veut_set = false; float a_jambe0 = 0;
    float memoire_t = 0, evite_t = 0, cw_t = 0, cw_d = 1e9f; Cible* evite = nullptr;   // mémoire de cible, cible ignorée, garde-fou d'approche
    PiloteIA(const std::string& n, Col c);
    void apres_respawn() override; void avant_mort() override; void sur_degats(Cible* src) override;
    void recolorer(Col c);
    V3 oeil() const { return {pos.x, pos.y + 1.6f, pos.z}; }
    void penser(); void choisir_destination(); void actions_combat(); void bouger(float dt); void tirer_sur_cible(float dt); void update(float dt);
    void afficher();
};

// ------------------------------------------------------------------ Zombies
struct Groupe { std::vector<Zombie*> m; };
enum ZType { ZJOUR, ZSPECIAL, ZNUIT, ZRAPACE, ZGEANT };
struct Zombie : Cible {
    ZType type; float HP, MULTHP, VIT, MULTVIT, DEGATS, DETECTION, DELAI, ANIM, TAILLE; bool ecoute = false, geant = false;
    Col ctete, cyeux; float hp = 0, hp_max = 0, var = 1, pense = 0, atk = 0, flash = 0, anim = 0;
    Cible* cible = nullptr; bool a_dest = false; V3 dest{0, 0, 0}; float dest_t = 0, rt = 0, bloque = 0, bonus_detection = 0; V3 route_but{0, 0, 0}; bool route_valide = false;
    std::vector<V3> route; Groupe* groupe = nullptr; bool a_alerte = false; V3 alerte_pos{0, 0, 0}; float alerte_t = 0; bool proche = true;
    Humano* m = nullptr; Humano mm; float j_a = 0;
    Zombie(ZType t, V3 p);
    float vitesse() const { return VIT * MULTVIT * var; }
    void alerter(V3 p, bool propage = true);
    Cible* trouver_cible(); bool avancer(float dx, float dz, float dt); void enquete(float dt); void aller_vers(V3 but, float dt);
    void tick(float dt, bool proche); void errer(float dt);
    void damage(float n, Cible* src = nullptr, bool tete = false) override;
};
std::vector<int> tailles_groupes(int total, int tmin, int tmax);
std::vector<Zombie*> creer_groupe(ZType t, int n, V3 centre, float bonus_det = 0);
void generer_groupes(ZType t, int total, int tmin, int tmax, const std::function<V3()>& pos_fn);
struct CompoZ { ZType t; int total, tmin, tmax; };
std::vector<CompoZ> composition(bool nuit);
void gestion_zombies_update(float dt);

// ------------------------------------------------------------------ Titan
struct TitanAuto { Cible* cible = nullptr; float pense = 0, bloque = 0, rt = 0; std::vector<V3> route; bool a_but = false; V3 but_prec{0, 0, 0}; ArmeP arme, epee; };
struct Titan : Cible {
    Pilote* owner; int state = 0;       // 0 falling, 1 ready, 2 embarque
    float hp = 400, hp_max = 400, t_epee = 0, epee_a = 0; std::string ordre = "suivre"; bool a_point = false; V3 point{0, 0, 0}; V3 spot{0, 0, 0};
    TitanAuto ia;
    Titan(Pilote* o, V3 spot);
    void ordonner(const std::string& o, const V3* p = nullptr);
    void balancer() { t_epee = .45f; epee_a = 0; }
    void update(float dt);
    void embarquer(); void debarquer(V3 p, float hp_);
    void damage(float n, Cible* src = nullptr, bool tete = false) override;
    void detruire(bool explose = true, Cible* tueur = nullptr);
    V3 tete() const { return {pos.x, pos.y + 5.2f, pos.z}; }
};
void titan_dessiner(Titan* t);

// ------------------------------------------------------------------ grenades
struct Grenade { Pilote* owner; V3 pos, v; bool frag; float fuse = 2.4f; bool mort = false; };

// ------------------------------------------------------------------ combat / registres
namespace G {
extern std::vector<Pilote*> pilotes; extern std::vector<Zombie*> zombies; extern std::vector<Titan*> titans;
extern std::vector<Grenade*> grenades; extern Player* joueur; extern Partie* partie;
Cible* proprietaire(Cible* s);
void msg(const std::string& t);
std::vector<Cible*> toutes_cibles();
void bruit(V3 pos, float rayon = RAYON_BRUIT_TIR);
void explode(V3 pos, float rayon, float degats, Cible* src = nullptr, Cible* ignore = nullptr);
void hitscan(Cible* owner, V3 origin, V3 d, float portee, float degats, bool explosif, float er, float ed, const V3* debut, Col col, float ep);
bool marcher(Cible* e, float dx, float dz, float v, float dt);
bool watchdog(Cible* e, float dt, bool veut_bouger, float dist_but);   // true si l'entité n'avance plus alors qu'elle devrait
V3 spawn_loin();
void reset_entites();
void melee_coup(Cible* p, V3 origin, V3 dir, Arme* a, int& touche);
bool melee_est_epee(Cible* p);
void update_grenades(float dt);
void dessiner_grenades();
Pilote* pilote_de(Cible* c);
bool allies(Cible* a, Cible* b);          // même pilote, ou même équipe (jamais de tir allié)
}
namespace Nav {
std::vector<V3> route(V3 depart, V3 but, float but_y = 0);
std::vector<V3> vers_sol(V3 a, V3 b);
std::vector<V3> vers_sol_gros(V3 a, V3 b);
bool sol_joignable(float x, float z);
std::vector<V3> chemin_toit(V3 a, V3 b, float niv);
bool joignable(float x, float z, float y);
}
// hooks vers l'interface
void hud_msg(const std::string& t, float ttl = 5);
void hud_degats(); void hud_touche(bool tete); void hud_banniere(const std::string& t, float d = 3);
