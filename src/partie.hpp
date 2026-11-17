// FALLING LIGHT - parties : mode classique, point de contrôle, parcours.
#pragma once
#include "entites.hpp"

struct Partie {
    std::string nom_mode = "classique", titre = "SURVIS 10 MINUTES", description = "Survis, tue des zombies (+1) et vole les points des pilotes";
    bool loot_actif = true, nuit = false, en_cours = false, fini = false, pause = false, equipes = false;
    float t = 0, acc_loot = 0; bool a_detect = false; float t_detect = 0; std::string resultat;
    std::map<int, int> deficit; std::vector<std::pair<float, std::function<void()>>> taches;
    virtual ~Partie() {}
    bool actif() const { return en_cours && !fini && !pause; }
    void plus_tard(float d, std::function<void()> fn) { taches.push_back({t + d, fn}); }
    void appliquer_ciel(int nuit_force = -1);
    virtual void demarrer(const Classe* c);
    virtual void preparer_pilotes(const Classe* c);
    V3 pos_spawn();
    virtual void lancer_zombies();
    virtual void mort_zombie(Zombie* z, Cible* src);
    void renforts();
    virtual void mort_pilote(Pilote* v, Cible* src);
    virtual void mort_titan(Titan* t, Cible* src) { (void)t; (void)src; }
    virtual V3 point_respawn(Pilote* v) { (void)v; return G::spawn_loin(); }
    virtual float delai_renfort() { return DELAI_RESPAWN_ZOMBIE; }      // délai avant qu'un zombie tué soit remplacé
    virtual bool respawn_payant() const { return true; }                // la réapparition coûte-t-elle des points ?
    virtual bool renfort_par_mort() { return true; }                    // true : chaque mort programme un renfort ; false : le mode gère le rythme lui-même
    void reapparaitre(Pilote* v);
    virtual void verifier_fin();
    void finir(Pilote* g, const std::string& raison);
    virtual std::vector<Pilote*> pilotes_actifs() { std::vector<Pilote*> r; for (auto p : G::pilotes) if (p->participe) r.push_back(p); return r; }
    virtual float temps_restant() { return std::max(0.0f, (float)DUREE_PARTIE - t); }
    virtual std::string phase_texte() { return nuit ? "NUIT - zombies enrages" : "JOUR - zombies lents"; }
    virtual std::string lignes_score(Pilote* j);
    virtual std::string info_mode() { return ""; }
    virtual bool objectif(V3& o) { (void)o; return false; }
    virtual bool barre(float& frac, Col& c) { (void)frac; (void)c; return false; }
    virtual bool objectif_ia(Pilote* p, V3& o) { (void)p; (void)o; return false; }
    virtual float rayon_zone() { return 8; }
    virtual void maj_mode(float dt) { (void)dt; }
    virtual void dessiner_visuels() {}
    void update(float dt);
    void ouvrir_menu();
    virtual void vider_visuels() {}
};
struct PartieControle : Partie {
    bool a_zone = false; V3 zone{0, 0, 0}; float progres = 0, age_zone = 0; Pilote* capteur = nullptr; bool conteste = false; std::map<Pilote*, int> captures;
    PartieControle();
    void demarrer(const Classe* c) override;
    void placer_zone(); void retirer_zone(); std::vector<Pilote*> dans_la_zone(); void capturer(Pilote* p);
    void maj_mode(float dt) override; bool objectif(V3& o) override; bool barre(float& f, Col& c) override;
    std::string info_mode() override; bool objectif_ia(Pilote* p, V3& o) override; float rayon_zone() override { return 8; }
    void dessiner_visuels() override; void vider_visuels() override { a_zone = false; }
    Col couleur_zone{.1f, .8f, 1, 1};
};
struct PartieParcours : Partie {
    std::vector<V3> points; int idx = 0; float reste = 0, t_vague = 0, temps_total = 0; int tues = 0; bool reussi = false; V3 depart{0, 0, 0};
    PartieParcours();
    void demarrer(const Classe* c) override; void preparer_pilotes(const Classe* c) override; void lancer_zombies() override;
    void generer_parcours(V3 dep); float temps_trajet(V3 a, V3 b); void groupe_aleatoire(V3 centre); V3 pos_vague();
    void mort_zombie(Zombie* z, Cible* src) override; void verifier_point(); void finir_parcours(bool gagne, const std::string& raison = "");
    void mort_pilote(Pilote* v, Cible* src) override; void verifier_fin() override;
    std::vector<Pilote*> pilotes_actifs() override { return {G::joueur}; }
    float temps_restant() override { return std::max(0.0f, reste); }
    std::string lignes_score(Pilote* j) override; bool objectif(V3& o) override; std::string info_mode() override; void maj_mode(float dt) override;
    void dessiner_visuels() override;
};
// ---- Match à mort par équipe (2 x 10 pilotes) et Mêlée générale (10 pilotes) : pas de loot, zombies et jour/nuit conservés
constexpr int SCORE_VICTOIRE = 150, PTS_PILOTE = 5, PTS_TITAN = 10, PTS_ZOMBIE = 1;
struct PartieDM : Partie {
    bool par_equipe; int stat_pilotes = 0, stat_zombies = 0, stat_titans = 0;
    explicit PartieDM(bool eq);
    int total_equipe(int e); std::vector<Pilote*> classement();
    void preparer_pilotes(const Classe* c) override; V3 point_respawn(Pilote* v) override;
    void mort_zombie(Zombie* z, Cible* src) override; void mort_pilote(Pilote* v, Cible* src) override; void mort_titan(Titan* t, Cible* src) override;
    void verifier_fin() override; void terminer(const std::string& texte);
    std::vector<Pilote*> pilotes_actifs() override; std::string lignes_score(Pilote* j) override; std::string info_mode() override;
    bool objectif_ia(Pilote* p, V3& o) override; float rayon_zone() override { return 10; }
    bool respawn_payant() const override { return false; }
    bool renfort_par_mort() override { return false; }                  // renforts au goutte-à-goutte : les zombies restent une menace, pas une ferme à points
    float acc_renfort = 0; void maj_mode(float dt) override;
};
Partie* creer_partie(const std::string& mode, bool nuit);
