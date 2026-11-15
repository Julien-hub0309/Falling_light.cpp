#include "partie.hpp"
#include "rlgl.h"

void ui_menu_revenir(); void ui_menu_fermer(); void ui_curseur(bool libre);
void hud_nuit(bool n);

// ====================================================================== base (mode classique)
void Partie::appliquer_ciel(int nf) { bool n = nf < 0 ? nuit : (nf != 0); R.maj_jour(n ? 1.0f : 0.0f); hud_nuit(n); }
void Partie::ouvrir_menu() {
    en_cours = fini = pause = false; G::reset_entites(); vider_visuels(); FX::vider(); appliquer_ciel(0); ui_menu_revenir(); ui_curseur(true); hud_banniere("");
}
void Partie::demarrer(const Classe* c) {
    G::reset_entites(); FX::vider(); taches.clear(); t = 0; a_detect = false; deficit.clear(); fini = pause = false; resultat.clear();
    for (Loot* l : E::loots) { if (l->temporaire) { l->actif = false; l->vie = 0; } else l->actif = true; }
    E::loots.erase(std::remove_if(E::loots.begin(), E::loots.end(), [](Loot* l) { if (l->temporaire) { delete l; return true; } return false; }), E::loots.end());
    en_cours = true; preparer_pilotes(c); appliquer_ciel(); lancer_zombies(); ui_menu_fermer(); ui_curseur(false);
    hud_banniere(titre, 3); G::msg(description);
    G::msg(!nuit ? "JOUR : zombies lents, les tirs attirent les spéciaux" : "NUIT : zombies enragés, les tirs attirent les rapaces");
}
void Partie::preparer_pilotes(const Classe* c) {
    std::vector<V3> sp = E::spawns; RG.shuffle(sp); int i = 0;
    for (size_t k = 0; k < G::pilotes.size(); k++) {
        Pilote* p = G::pilotes[k]; p->equipe = 0; p->pts = 0; p->arme_depart = false;
        if (auto ia = dynamic_cast<PiloteIA*>(p)) ia->recolorer(ia->couleur0);
        p->participe = k < 5;                                   // classique / contrôle : le joueur et 4 IA
        if (!p->participe) { p->vivant = false; p->elimine = true; continue; }
        p->nouvelle_partie(p == G::joueur ? c : classe_aleatoire(), sp[i % sp.size()]); i++;
    }
}
V3 Partie::pos_spawn() {
    std::vector<Pilote*> viv; for (auto p : G::pilotes) if (p->vivant) viv.push_back(p);
    float lim = DEMI_MAP - 6;
    for (int k = 0; k < 40; k++) {
        float x, z;
        if (!viv.empty()) { Pilote* c = RG.choice(viv); float a = rnd(0, 6.283f), d = rnd(35, 110); x = c->pos.x + std::sin(a) * d; z = c->pos.z + std::cos(a) * d; }
        else { x = rnd(-150, 150); z = rnd(-150, 150); }
        bool ok = std::fabs(x) < lim && std::fabs(z) < lim && !E::touche(x, 0, z, 3.5f, 3) && Nav::sol_joignable(x, z);
        if (ok) for (auto p : viv) if (hyp(x - p->pos.x, z - p->pos.z) < 30) { ok = false; break; }
        if (ok) return {x, 0, z};
    }
    std::vector<V3> loin; for (auto p : viv) loin.push_back(p->pos); return E::point_libre(3.5f, loin, 30);
}
void Partie::lancer_zombies() { for (auto& c : composition(nuit)) generer_groupes(c.t, c.total, c.tmin, c.tmax, [this] { return pos_spawn(); }); }
void Partie::mort_zombie(Zombie* z, Cible* src) {
    Pilote* k = G::pilote_de(src); if (k) { k->score += 1; k->kills += 1; }
    if (actif()) {
        if (!z->geant) { deficit[z->type]++; if (renfort_par_mort()) plus_tard(delai_renfort(), [this] { renforts(); }); }
        if (rnd01() < .12 && loot_actif) E::nouveau_loot(z->pos, -1, true);
    }
}
void Partie::renforts() {
    if (!actif()) return;
    for (auto& c : composition(nuit)) { int d = deficit[c.t]; while (d >= c.tmin && c.tmax > 1) { int n = std::min(d, c.tmax); creer_groupe(c.t, n, pos_spawn()); d -= n; } deficit[c.t] = d; }
}
void Partie::mort_pilote(Pilote* v, Cible* src) {
    if (!v->vivant || v->elimine) return;
    Pilote* k = G::pilote_de(src); if (!k || k == v) k = nullptr;
    v->score_final = v->score; v->mourir();
    if (loot_actif) { static const int T[3] = {L_SOIN, L_MUNITIONS, L_ARME}; E::nouveau_loot(v->pos, T[RG.randbelow(3)], true); }
    std::string qui = k ? k->nom : "les zombies";
    if (v->score >= COUT_RESPAWN) {
        v->score -= COUT_RESPAWN; if (k) { k->score += v->score; k->kills++; } v->score = 0; v->attente = true;
        G::msg(v->nom + " tué par " + qui + " (-" + std::to_string(COUT_RESPAWN) + " pts, respawn)"); plus_tard(3, [this, v] { reapparaitre(v); });
    } else {
        v->elimine = true; v->t_elim = t; if (k) { k->score += v->score; k->kills++; } v->score = 0; G::msg(v->nom + " ÉLIMINÉ par " + qui);
        if (v->titan_actif()) v->titan->detruire(false); if (v == G::joueur) hud_banniere("ÉLIMINÉ", 4);
    }
}
void Partie::reapparaitre(Pilote* v) { if (!actif() || v->elimine) return; v->reapparaitre(point_respawn(v)); }
void Partie::verifier_fin() {
    std::vector<Pilote*> rest; for (auto p : G::pilotes) if (!p->elimine) rest.push_back(p);
    if (rest.size() <= 1) {
        if (!a_detect) { a_detect = true; t_detect = t; }
        else if (t - t_detect >= .5f) {
            if (!rest.empty()) finir(rest[0], "Dernier survivant");
            else { Pilote* b = nullptr; for (auto p : G::pilotes) if (p->t_elim >= t_detect - .1f && (!b || p->score_final > b->score_final)) b = p; if (!b) b = G::pilotes[0]; finir(b, "Mort simultanee : plus de points"); }
        }
    } else if (t >= DUREE_PARTIE) { Pilote* b = rest[0]; for (auto p : rest) if (p->score > b->score) b = p; finir(b, "Temps ecoule : plus de points"); }
}
void Partie::finir(Pilote* g, const std::string& raison) {
    fini = true; ui_curseur(true); resultat = g == G::joueur ? "VICTOIRE !  (" + raison + ")" : g->nom + " GAGNE  (" + raison + ")"; hud_banniere("FIN DE PARTIE", 5);
}
std::string Partie::lignes_score(Pilote* j) {
    auto l = pilotes_actifs(); std::stable_sort(l.begin(), l.end(), [](Pilote* a, Pilote* b) { return a->score > b->score; });
    std::string s; for (auto q : l) s += std::string(q == j ? "> " : "  ") + q->nom + "  " + std::to_string(q->score) + " pts  " + (q->elimine ? "ELIM" : (!q->vivant ? "..." : "")) + "\n";
    return s + "Zombies : " + std::to_string(G::zombies.size());
}
void ramassage(float dt);
void Partie::update(float dt) {
    if (!actif()) return;
    t += dt;
    if (loot_actif) { acc_loot += dt; if (acc_loot >= .1f) { ramassage(acc_loot); acc_loot = 0; } }
    for (size_t i = 0; i < taches.size();) { if (taches[i].first <= t) { auto fn = taches[i].second; taches.erase(taches.begin() + i); fn(); } else i++; }
    maj_mode(dt); if (!fini) verifier_fin();
}
void ramassage(float dt) {
    for (auto p : G::pilotes) craft_auto(p);
    for (Loot* l : std::vector<Loot*>(E::loots)) {
        if (!l->actif) continue;
        if (l->temporaire) { l->vie -= dt; if (l->vie <= 0) { E::loots.erase(std::remove(E::loots.begin(), E::loots.end(), l), E::loots.end()); delete l; continue; } }
        for (auto p : G::pilotes) {
            if (p->vivant && !p->elimine && std::fabs(p->pos.x - l->pos.x) < 1.6f && std::fabs(p->pos.z - l->pos.z) < 1.6f && p->pos.y - l->pos.y > -1 && p->pos.y - l->pos.y < 2.4f) {
                bool gere; bool r = craft_on_loot(p, l->type, gere); if (!gere) r = p->ramasser(l);
                if (r) {
                    if (l->temporaire) { E::loots.erase(std::remove(E::loots.begin(), E::loots.end(), l), E::loots.end()); delete l; }
                    else { l->actif = false; G::partie->plus_tard(45, [l] { l->actif = true; }); }
                    break;
                }
            }
        }
    }
}

// ====================================================================== point de contrôle
static const float DUREE_CAPTURE = 30, RAYON_ZONE = 8, VIE_ZONE = 150; static const int PTS_CAPTURE = 10;
PartieControle::PartieControle() { nom_mode = "controle"; titre = "POINT DE CONTRÔLE"; description = "Reste 30 s dans la zone : +10 pts. Zombie tué : +1 pt"; }
void PartieControle::demarrer(const Classe* c) { captures.clear(); a_zone = false; progres = 0; capteur = nullptr; Partie::demarrer(c); placer_zone(); }
void PartieControle::placer_zone() {
    if (!actif()) return; bool anc = a_zone; V3 ancp = zone; V3 p{};
    for (int k = 0; k < 60; k++) { p = E::point_libre(RAYON_ZONE * .6f, {}, 0, 30); if (!anc || hyp(p.x - ancp.x, p.z - ancp.z) > 70) break; }
    zone = {p.x, 0, p.z}; a_zone = true; progres = 0; capteur = nullptr; conteste = false; age_zone = 0; G::msg("Nouveau point de contrôle !");
}
void PartieControle::retirer_zone() { a_zone = false; }
std::vector<Pilote*> PartieControle::dans_la_zone() {
    std::vector<Pilote*> r; for (auto p : G::pilotes) if (p->vivant && !p->elimine && hyp(p->pos.x - zone.x, p->pos.z - zone.z) < RAYON_ZONE && std::fabs(p->pos.y - zone.y) < 5) r.push_back(p); return r;
}
void PartieControle::maj_mode(float dt) {
    if (!a_zone) return; age_zone += dt; auto d = dans_la_zone();
    if (d.size() == 1) { Pilote* p = d[0]; if (capteur != p) { capteur = p; progres = 0; } conteste = false; progres += dt; couleur_zone = {.3f, 1, .4f, 1}; if (progres >= DUREE_CAPTURE) capturer(p); }
    else if (d.size() > 1) { conteste = true; couleur_zone = {1, .55f, .1f, 1}; }
    else { conteste = false; progres = std::max(0.0f, progres - 2 * dt); if (progres <= 0) capteur = nullptr; couleur_zone = {.1f, .8f, 1, 1}; }
    if (a_zone && age_zone > VIE_ZONE && progres <= 0) { retirer_zone(); G::msg("La zone se déplace..."); plus_tard(2, [this] { placer_zone(); }); }
}
void PartieControle::capturer(Pilote* p) {
    p->score += PTS_CAPTURE; captures[p]++; G::msg(p->nom + " capture le point (+10 pts)"); if (p == G::joueur) hud_banniere("POINT CAPTURÉ  +10", 2);
    retirer_zone(); progres = 0; capteur = nullptr; plus_tard(6, [this] { placer_zone(); });
}
bool PartieControle::objectif(V3& o) { if (!a_zone) return false; o = zone; return true; }
bool PartieControle::barre(float& f, Col& c) { if (!a_zone) return false; f = progres / DUREE_CAPTURE; c = conteste ? Col{1, .55f, .1f, 1} : Col{.3f, 1, .4f, 1}; return true; }
std::string PartieControle::info_mode() {
    Pilote* j = G::joueur; if (!a_zone) return "Prochaine zone...";
    float d = hyp(zone.x - j->pos.x, zone.z - j->pos.z); std::string e;
    if (conteste) e = "CONTESTÉ !"; else if (capteur == j) e = "CAPTURE " + std::to_string((int)progres) + "/30 s"; else if (capteur) e = capteur->nom + " capture..."; else e = "Entre dans la zone";
    return "POINT DE CONTRÔLE  " + std::to_string((int)d) + " m  -  " + e;
}
bool PartieControle::objectif_ia(Pilote* p, V3& o) {
    if (!a_zone) return false; PiloteIA* ia = dynamic_cast<PiloteIA*>(p); if (!ia) return false;
    if (!ia->veut_set || rnd01() < .1) { ia->veut_zone = rnd01() < .65; ia->veut_set = true; }
    if (!ia->veut_zone) return false; o = zone; return true;
}
void PartieControle::dessiner_visuels() {
    if (!a_zone) return; Col c = couleur_zone; float r = RAYON_ZONE; rlDisableDepthMask(); rlDisableBackfaceCulling();
    DrawCylinder({zone.x, .12f, zone.z}, r, r, .02f, 40, toC({c.r, c.g, c.b, .28f}));
    for (auto d : {std::pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) DrawCube({zone.x + d.first * r, 8, zone.z + d.second * r}, .5f, 16, .5f, toC({c.r, c.g, c.b, .5f}));
    DrawCube({zone.x, 45, zone.z}, 1.2f, 90, 1.2f, toC({c.r, c.g, c.b, .3f})); rlEnableDepthMask(); rlEnableBackfaceCulling();
}

// ====================================================================== parcours
static const int NB_POINTS = 8; static const float TEMPS_DEPART = 12, VITESSE_REF = 7, MARGE = 12, RAYON_POINT = 5;
PartieParcours::PartieParcours() { nom_mode = "parcours"; titre = "PARCOURS"; description = "Couteau seulement : passe tous les points dans le temps imparti"; loot_actif = false; }
void PartieParcours::demarrer(const Classe*) { points.clear(); idx = 0; tues = 0; reussi = false; t_vague = 3; temps_total = 0; Partie::demarrer(&CLASSE_PARCOURS); }
void PartieParcours::preparer_pilotes(const Classe*) {
    std::vector<V3> sp = E::spawns; RG.shuffle(sp);
    for (Pilote* p : G::pilotes) {
        p->equipe = 0; p->pts = 0; p->arme_depart = false; p->participe = p == G::joueur;
        if (auto ia = dynamic_cast<PiloteIA*>(p)) ia->recolorer(ia->couleur0);
        if (p == G::joueur) p->nouvelle_partie(&CLASSE_PARCOURS, sp[0]); else { p->vivant = false; p->elimine = true; }
    }
    generer_parcours(sp[0]);
}
void PartieParcours::generer_parcours(V3 dep) {
    std::vector<V3> pts; V3 cur = dep;
    for (int i = 0; i < NB_POINTS; i++) {
        bool ok = false; V3 p{};
        for (int k = 0; k < 80; k++) {
            V3 cand;
            if (i % 3 == 2 && !E::acces_toit.empty()) {
                std::vector<const AccesToit*> bas; for (auto& a : E::acces_toit) if (a.y <= 16) bas.push_back(&a);
                std::vector<const AccesToit*> src = bas; if (src.empty()) for (auto& a : E::acces_toit) src.push_back(&a);
                const AccesToit* a = RG.choice(src); cand = {a->toit.x, a->toit.y, a->toit.z};
            } else cand = E::point_libre(2.5f, {}, 0, 20);
            float d = hyp(cand.x - cur.x, cand.z - cur.z);
            if (55 <= d && d <= 135) { p = cand; ok = true; break; }
        }
        if (!ok) p = E::point_libre(2.5f);
        pts.push_back(p); cur = p;
    }
    points = pts; reste = TEMPS_DEPART + temps_trajet(dep, pts[0]); depart = {dep.x, 0, dep.z};
}
float PartieParcours::temps_trajet(V3 a, V3 b) { return hyp(b.x - a.x, b.z - a.z) / VITESSE_REF + MARGE + std::max(0.0f, b.y - a.y) * 1.2f; }
void PartieParcours::lancer_zombies() {
    for (size_t i = 0; i < points.size() && i < 4; i++) {
        V3 c = {points[i].x, 0, points[i].z};
        if (points[i].y > 2.5f) {                                  // point sur un toit : le groupe attend au pied de l'escalier
            float bd = 1e9f; for (auto& a : E::acces_toit) { float d = hyp(a.toit.x - points[i].x, a.toit.z - points[i].z); if (d < bd && std::fabs(a.y - points[i].y) < 1) { bd = d; c = {a.base.x, 0, a.base.z}; } }
        }
        groupe_aleatoire(c);
    }
}
void PartieParcours::groupe_aleatoire(V3 c) {
    double n = rnd01();
    if (!nuit) { if (n < .25) creer_groupe(ZSPECIAL, rndint(2, 3), c, 300); else creer_groupe(ZJOUR, rndint(3, 4), c, 300); }
    else {
        int geants = 0; for (auto z : G::zombies) if (z->geant) geants++;
        if (n < .15 && geants < 2) creer_groupe(ZGEANT, 1, c, 300); else if (n < .45) creer_groupe(ZRAPACE, rndint(2, 3), c, 300); else creer_groupe(ZNUIT, rndint(3, 4), c, 300);
    }
}
V3 PartieParcours::pos_vague() {
    Pilote* j = G::joueur; float cap = 0; if (idx < (int)points.size()) cap = std::atan2(points[idx].x - j->pos.x, points[idx].z - j->pos.z);
    float lim = DEMI_MAP - 6;
    for (int k = 0; k < 30; k++) { float a = cap + rnd(-1.6f, 1.6f), d = rnd(38, 65), x = j->pos.x + std::sin(a) * d, z = j->pos.z + std::cos(a) * d; if (std::fabs(x) < lim && std::fabs(z) < lim && !E::touche(x, 0, z, 3.5f, 3) && Nav::sol_joignable(x, z)) return {x, 0, z}; }
    return E::point_libre(3.5f, {j->pos}, 35);
}
void PartieParcours::mort_zombie(Zombie*, Cible* src) { if (G::proprietaire(src) == G::joueur) { tues++; G::joueur->score++; G::joueur->kills++; } }
void PartieParcours::verifier_point() {
    Pilote* j = G::joueur; if (idx >= (int)points.size()) return; V3 p = points[idx];
    if (hyp(j->pos.x - p.x, j->pos.z - p.z) < RAYON_POINT && std::fabs(j->pos.y - p.y) < 3.5f) {
        idx++; if (idx >= (int)points.size()) { reussi = true; finir_parcours(true); return; }
        float bonus = temps_trajet(p, points[idx]); reste += bonus; hud_banniere("POINT " + std::to_string(idx) + "/" + std::to_string(points.size()) + "   +" + std::to_string((int)bonus) + " s", 2);
    }
}
void PartieParcours::finir_parcours(bool gagne, const std::string& raison) {
    fini = true; ui_curseur(true); int tt = (int)temps_total; char b[200];
    if (gagne) { snprintf(b, sizeof b, "PARCOURS TERMINÉ !  %d:%02d  -  temps restant %d s  -  %d zombies", tt / 60, tt % 60, (int)reste, tues); resultat = b; hud_banniere("PARCOURS TERMINÉ", 5); }
    else { resultat = "ÉCHEC : " + raison + "  -  point " + std::to_string(idx) + "/" + std::to_string(points.size()); hud_banniere("ÉCHEC", 5); }
}
void PartieParcours::mort_pilote(Pilote* v, Cible*) { if (v == G::joueur && v->vivant) { v->mourir(); finir_parcours(false, "tu es mort"); } }
void PartieParcours::verifier_fin() {}
std::string PartieParcours::lignes_score(Pilote*) { return "Point " + std::to_string(std::min(idx + 1, (int)points.size())) + "/" + std::to_string(points.size()) + "\nZombies tués : " + std::to_string(tues) + "\nZombies : " + std::to_string(G::zombies.size()); }
bool PartieParcours::objectif(V3& o) { if (idx >= (int)points.size()) return false; o = points[idx]; return true; }
std::string PartieParcours::info_mode() { V3 o; if (!objectif(o)) return ""; Pilote* j = G::joueur; float d = hyp(o.x - j->pos.x, o.z - j->pos.z); return "POINT " + std::to_string(idx + 1) + "/" + std::to_string(points.size()) + "  -  " + std::to_string((int)d) + " m" + (o.y > 3 ? "  (en hauteur)" : ""); }
void PartieParcours::maj_mode(float dt) {
    Pilote* j = G::joueur; if (!j->vivant) return; temps_total += dt; reste -= dt;
    if (reste <= 0) { finir_parcours(false, "temps écoulé"); return; }
    verifier_point(); t_vague -= dt;
    if (t_vague <= 0 && !fini) { t_vague = (nuit ? 5.5f : 7.0f) * rnd(.8f, 1.2f); if ((int)G::zombies.size() < (nuit ? 26 : 22)) groupe_aleatoire(pos_vague()); }
}
void PartieParcours::dessiner_visuels() {
    rlDisableDepthMask(); rlDisableBackfaceCulling();
    for (int k : {idx, idx + 1}) {
        if (k >= (int)points.size()) continue; V3 p = points[k]; bool act = k == idx; Col c = act ? Col{.1f, .9f, 1, 1} : Col{.6f, .65f, .7f, 1}; float a = act ? .55f : .18f;
        DrawCylinder({p.x, p.y + .12f, p.z}, RAYON_POINT * 1.2f, RAYON_POINT * 1.2f, .02f, 32, toC({c.r, c.g, c.b, a * .6f}));
        float w = act ? .9f : .5f; DrawCube({p.x, p.y + 60, p.z}, w, 120, w, toC({c.r, c.g, c.b, a}));
    }
    rlEnableDepthMask(); rlEnableBackfaceCulling();
}

// ====================================================================== match à mort par équipe / mêlée générale
static const Col COUL_EQ[3] = {{1, 1, 1, 1}, {.15f, .45f, 1, 1}, {1, .55f, .1f, 1}};
static const char* NOM_EQ[3] = {"", "BLEU", "ORANGE"};
PartieDM::PartieDM(bool eq) : par_equipe(eq) {
    equipes = eq; loot_actif = false;
    if (eq) { nom_mode = "mortequipe"; titre = "MATCH À MORT PAR ÉQUIPE"; description = "Bleus contre Oranges : pilote +5, Titan +10, zombie +1. Premier camp à 150 pts !"; }
    else { nom_mode = "melee"; titre = "MÊLÉE GÉNÉRALE"; description = "Chacun pour soi : pilote +5, Titan +10, zombie +1. Premier à 150 pts !"; }
}
int PartieDM::total_equipe(int e) { int t = 0; for (auto p : G::pilotes) if (p->participe && p->equipe == e) t += p->pts; return t; }
std::vector<Pilote*> PartieDM::classement() { auto l = pilotes_actifs(); std::stable_sort(l.begin(), l.end(), [](Pilote* a, Pilote* b) { return a->pts > b->pts; }); return l; }
std::vector<Pilote*> PartieDM::pilotes_actifs() { std::vector<Pilote*> r; for (auto p : G::pilotes) if (p->participe) r.push_back(p); return r; }
void PartieDM::preparer_pilotes(const Classe* c) {
    int n = par_equipe ? 20 : 10;
    for (size_t k = 0; k < G::pilotes.size(); k++) {
        Pilote* p = G::pilotes[k]; p->pts = 0; p->participe = (int)k < n; p->arme_depart = true;
        if (!p->participe) { p->equipe = 0; p->vivant = false; p->elimine = true; continue; }
        p->equipe = par_equipe ? (k < 10 ? 1 : 2) : 0;                // le joueur est dans l'équipe bleue
        if (auto ia = dynamic_cast<PiloteIA*>(p)) ia->recolorer(par_equipe ? COUL_EQ[p->equipe] : hsv(fmodf((float)k * 37.0f, 360.0f), .85f, 1.0f));
        p->vivant = false; p->elimine = false;
        p->nouvelle_partie(p == G::joueur ? c : classe_aleatoire(), point_respawn(p));
    }
}
V3 PartieDM::point_respawn(Pilote* v) {
    float lim = DEMI_MAP - 8; auto ok = [&](float x, float z) { return std::fabs(x) < lim && std::fabs(z) < lim && !E::touche(x, 0, z, 1.2f, 3) && Nav::sol_joignable(x, z); };
    if (par_equipe && v->equipe) {                                   // bleus à l'ouest, oranges à l'est
        float bx = v->equipe == 1 ? -120.0f : 120.0f, bz = RG.randbelow(2) ? 120.0f : -120.0f;
        for (int k = 0; k < 80; k++) { float x = bx + rnd(-30, 30), z = bz + rnd(-30, 30); if (ok(x, z)) return {x, 0, z}; }
        return E::point_libre(1.2f);
    }
    V3 best = E::point_libre(1.2f); float bd = -1;                    // mêlée : on réapparaît loin des autres pilotes
    for (int k = 0; k < 30; k++) {
        float x = rnd(-lim, lim), z = rnd(-lim, lim); if (!ok(x, z)) continue; float d = 1e9f;
        for (auto p : G::pilotes) if (p != v && p->participe && p->vivant) d = std::min(d, hyp(x - p->pos.x, z - p->pos.z));
        if (d > bd) { bd = d; best = {x, 0, z}; }
    }
    return best;
}
void PartieDM::maj_mode(float dt) {
    acc_renfort += dt; if (acc_renfort < 12) return; acc_renfort = 0;        // un seul groupe de zombies toutes les 12 s
    for (auto& c : composition(nuit)) { int d = deficit[c.t]; if (d >= c.tmin && c.tmax > 1) { int n = std::min(d, c.tmax); creer_groupe(c.t, n, pos_spawn()); deficit[c.t] = d - n; break; } }
}
void PartieDM::mort_zombie(Zombie* z, Cible* src) { Partie::mort_zombie(z, src); Pilote* k = G::pilote_de(src); if (k && k->participe) { k->pts += PTS_ZOMBIE; stat_zombies++; } }
void PartieDM::mort_pilote(Pilote* v, Cible* src) {
    if (!v->vivant) return; Pilote* k = G::pilote_de(src); bool ennemi = k && k != v && !G::allies(k, v);
    v->mourir();
    if (ennemi) { k->pts += PTS_PILOTE; k->score += PTS_PILOTE; k->kills++; stat_pilotes++; }
    if (v == G::joueur) G::msg(ennemi ? k->nom + " t'a éliminé" : "Les zombies t'ont tué");
    else if (ennemi && k == G::joueur) G::msg("Pilote éliminé : " + v->nom + " (+" + std::to_string(PTS_PILOTE) + ")");
    v->attente = true; plus_tard(4, [this, v] { reapparaitre(v); });    // pas d'élimination : on revient toujours
}
void PartieDM::mort_titan(Titan* t, Cible* src) {
    Pilote* k = G::pilote_de(src); if (!k || !k->participe || k == t->owner || G::allies(k, t->owner)) return;
    k->pts += PTS_TITAN; k->score += PTS_TITAN; stat_titans++;
    if (k == G::joueur) G::msg("Titan détruit (+" + std::to_string(PTS_TITAN) + ")"); else if (t->owner == G::joueur) G::msg("Ton Titan a été détruit par " + k->nom);
}
void PartieDM::terminer(const std::string& texte) { fini = true; ui_curseur(true); resultat = texte; hud_banniere("FIN DE PARTIE", 5); }
void PartieDM::verifier_fin() {
    char b[160];
    if (par_equipe) {
        int a = total_equipe(1), o = total_equipe(2);
        if (a >= SCORE_VICTOIRE || o >= SCORE_VICTOIRE || t >= DUREE_PARTIE) {
            const char* raison = (a >= SCORE_VICTOIRE || o >= SCORE_VICTOIRE) ? "objectif atteint" : "temps écoulé";
            if (a == o) snprintf(b, sizeof b, "MATCH NUL  (%d - %d)", a, o);
            else snprintf(b, sizeof b, "%s  Équipe %s gagne  (%d - %d, %s)", a > o ? "VICTOIRE !" : "DÉFAITE", a > o ? "bleue" : "orange", a, o, raison);
            terminer(b);
        }
    } else {
        auto c = classement(); Pilote* g = c.empty() ? nullptr : c[0];
        if (g && (g->pts >= SCORE_VICTOIRE || t >= DUREE_PARTIE)) {
            const char* raison = g->pts >= SCORE_VICTOIRE ? "objectif atteint" : "temps écoulé";
            if (g == G::joueur) snprintf(b, sizeof b, "VICTOIRE !  (%d pts, %s)", g->pts, raison); else snprintf(b, sizeof b, "%s GAGNE  (%d pts, %s)", g->nom.c_str(), g->pts, raison);
            terminer(b);
        }
    }
}
std::string PartieDM::info_mode() {
    Pilote* j = G::joueur; char b[160];
    if (par_equipe) { snprintf(b, sizeof b, "BLEU %d  -  ORANGE %d   (premier à %d)", total_equipe(1), total_equipe(2), SCORE_VICTOIRE); return b; }
    auto c = classement(); int rang = 1; for (size_t i = 0; i < c.size(); i++) if (c[i] == j) rang = (int)i + 1;
    snprintf(b, sizeof b, "Premier à %d pts  -  tu as %d pts  (%de sur %d)", SCORE_VICTOIRE, j->pts, rang, (int)c.size()); return b;
}
std::string PartieDM::lignes_score(Pilote* j) {
    auto l = classement(); std::string s; int n = 0;
    for (auto q : l) { if (n++ >= (par_equipe ? 8 : 10) && q != j) continue; s += std::string(q == j ? "> " : "  ") + (par_equipe ? std::string("[") + (q->equipe == 1 ? "B" : "O") + "] " : "") + q->nom + "  " + std::to_string(q->pts) + (q->vivant ? "" : "  ...") + "\n"; }
    return s + "Zombies : " + std::to_string(G::zombies.size());
}
bool PartieDM::objectif_ia(Pilote* p, V3& o) {
    if (rnd01() < .25) return false;                                  // parfois on rôde au hasard
    std::vector<std::pair<float, Pilote*>> c;
    for (auto q : G::pilotes) if (q != p && q->participe && q->vivant && !G::allies(p, q)) c.push_back({hyp(q->pos.x - p->pos.x, q->pos.z - p->pos.z), q});
    if (c.empty()) return false;
    std::sort(c.begin(), c.end(), [](auto& a, auto& b) { return a.first < b.first; });
    Pilote* cible = c[RG.randbelow((uint32_t)std::min<size_t>(3, c.size()))].second; o = cible->pos; return true;
}

Partie* creer_partie(const std::string& mode, bool nuit) {
    Partie* p;
    if (mode == "controle") p = new PartieControle(); else if (mode == "parcours") p = new PartieParcours();
    else if (mode == "mortequipe") p = new PartieDM(true); else if (mode == "melee") p = new PartieDM(false); else p = new Partie();
    p->nuit = nuit; return p;
}
