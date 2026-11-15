#include "partie.hpp"
#include "touches.hpp"
#include "rlgl.h"
#include <queue>

// ====================================================================== armes
namespace Armes {
static ArmeP mk() { return std::make_shared<Arme>(); }
ArmeP fusil() { auto a = mk(); a->nom = a->modele = "AR-15"; a->degats = 15; a->cadence = .09f; a->chargeur = a->munitions = 30; a->reserve = 120; a->portee = 130; a->dispersion = .014f; a->autom = true; a->t_recharge = 1.5f; a->zoom = 62; a->ads_prec = .35f; a->vue = {.32f, -.28f, .7f}; a->recul = .06f; a->pref = 22; a->trace = {1, .85f, .3f, 1}; return a; }
ArmeP smg() { auto a = mk(); a->nom = a->modele = "Kriss Vector"; a->degats = 9; a->cadence = .055f; a->chargeur = a->munitions = 33; a->reserve = 165; a->portee = 90; a->dispersion = .03f; a->autom = true; a->t_recharge = 1.3f; a->zoom = 72; a->ads_prec = .4f; a->vue = {.3f, -.27f, .6f}; a->recul = .04f; a->pref = 12; a->trace = {.3f, .95f, 1, 1}; return a; }
ArmeP sniper() { auto a = mk(); a->nom = a->modele = "DRS-50"; a->degats = 95; a->cadence = 1.2f; a->chargeur = a->munitions = 5; a->reserve = 20; a->portee = 300; a->dispersion = .06f; a->t_recharge = 2.4f; a->zoom = 20; a->ads_prec = .05f; a->vue = {.3f, -.28f, .9f}; a->recul = .2f; a->pref = 45; a->trace = {.85f, .93f, 1, 1}; a->ep = .05f; return a; }
ArmeP shotgun() { auto a = mk(); a->nom = a->modele = "Pompe"; a->degats = 9; a->plombs = 8; a->cadence = .85f; a->chargeur = a->munitions = 6; a->reserve = 30; a->portee = 40; a->dispersion = .07f; a->t_recharge = 2.0f; a->zoom = 80; a->ads_prec = .55f; a->vue = {.32f, -.28f, .7f}; a->recul = .15f; a->pref = 8; a->trace = {1, .55f, .15f, 1}; a->ep = .03f; return a; }
ArmeP pistolet() { auto a = mk(); a->nom = a->modele = "Pistolet"; a->degats = 17; a->cadence = .2f; a->chargeur = a->munitions = 12; a->reserve = 60; a->portee = 80; a->dispersion = .012f; a->t_recharge = 1.0f; a->zoom = 78; a->ads_prec = .3f; a->vue = {.3f, -.25f, .55f}; a->recul = .05f; a->pref = 14; a->trace = {1, .96f, .8f, 1}; return a; }
ArmeP couteau() { auto a = mk(); a->nom = a->modele = "Couteau"; a->degats = 34; a->cadence = .55f; a->chargeur = a->munitions = 0; a->reserve = 0; a->portee = 3; a->dispersion = 0; a->t_recharge = 0; a->zoom = 0; a->ads_prec = 1; a->vue = {.32f, -.3f, .62f}; a->recul = 0; a->pref = 2.2f; a->trace = {1, .25f, .25f, 1}; a->melee = true; a->bruit = 0; return a; }
ArmeP epee() { auto a = mk(); a->nom = a->modele = "Épée"; a->degats = 120; a->cadence = .9f; a->chargeur = a->munitions = 0; a->reserve = 0; a->portee = 7; a->dispersion = 0; a->t_recharge = 0; a->zoom = 0; a->ads_prec = 1; a->vue = {.9f, -.75f, 1.5f}; a->recul = 0; a->pref = 5; a->trace = {1, .35f, .1f, 1}; a->melee = true; a->titan = true; a->bruit = 0; return a; }
ArmeP pour_titan(const ArmeP& s) {
    if (s->melee) return epee();
    auto t = std::make_shared<Arme>(*s); t->titan = true; t->nom = s->nom + " TITAN"; t->degats = s->degats * 3; t->cadence = s->cadence * 1.15f;
    t->chargeur = t->munitions = s->chargeur * 3; t->reserve = -1; t->t_recharge = s->t_recharge * .8f; t->portee = s->portee * 1.4f; t->dispersion = s->dispersion * .7f;
    t->recul = s->recul * 2; t->ep = s->ep * 4; t->pref = s->pref * 1.4f; t->vue = {.95f, -.85f, 1.5f}; t->cool = t->rech = 0; return t;
}
ArmeP par_nom(const std::string& n) { if (n == "AR-15") return fusil(); if (n == "Kriss Vector") return smg(); if (n == "DRS-50") return sniper(); return shotgun(); }
}
const Classe CLASSES[4] = {{"ASSAUT", "AR-15\n2 grenades\nPolyvalent", 0, "frag", 2, 100, 1.0f}, {"TACTIC", "Kriss Vector\n2 grenades\nRapide (+10%)", 1, "frag", 2, 90, 1.1f},
                           {"SNIPER", "DRS-50 (lunette)\n2 fumigenes\nLongue portee", 2, "fumigene", 2, 85, 1.0f}, {"BRECHE", "Fusil a pompe\nResistant (120 PV)\nLent (-8%)", 3, "frag", 3, 120, .92f}};
const Classe CLASSE_PARCOURS = {"COUREUR", "Couteau uniquement", -1, "frag", 0, 100, 1.1f};
const Classe* classe_aleatoire() { return &CLASSES[RG.randbelow(4)]; }
ArmeP arme_classe(const Classe* c) { int k = c ? c->primaire : 0; return k == 1 ? Armes::smg() : (k == 2 ? Armes::sniper() : (k == 3 ? Armes::shotgun() : Armes::fusil())); }
bool Arme::tirer(Cible* owner, V3 origin, V3 d, const V3* debut, bool zoomed) {
    if (!pret()) return false;
    if (melee) { cool = cadence; int t; G::melee_coup(owner, origin, d, this, t); return true; }
    if (munitions <= 0) { recharger(); return false; }
    munitions--; cool = cadence;
    V3 base = norm(d), droite = {base.z, 0, -base.x};
    droite = len(droite) > 1e-6f ? norm(droite) : V3{1, 0, 0};
    V3 haut = norm(cross3(base, droite)); float sp = dispersion * (zoomed ? ads_prec : 1);
    for (int i = 0; i < plombs; i++) {
        float a = rnd(-sp, sp), b = rnd(-sp, sp); V3 dd = base + droite * a + haut * b;
        G::hitscan(owner, origin, dd, portee, degats, explosif, exp_rayon, exp_degats, debut, trace, ep);
    }
    if (bruit > 0) G::bruit(origin, bruit * (titan ? 1.3f : 1));
    return true;
}

// ====================================================================== modèles articulés
static std::map<std::string, Piece> pieces_cache;
static Piece& piece_cache(const std::string& k, const std::vector<Cube>& c) { auto it = pieces_cache.find(k); if (it == pieces_cache.end()) it = pieces_cache.emplace(k, baker(c)).first; return it->second; }
static std::string cle(Col c) { char b[48]; snprintf(b, sizeof b, "%.2f,%.2f,%.2f", c.r, c.g, c.b); return b; }
void humano_construire(Humano& m, Col veste, Col pantalon, Col peau, bool pilote, Col botte) {
    m.pilote = pilote;
    m.jambe = piece_cache("j" + cle(pantalon) + cle(botte), {{{.26f, .88f, .3f}, {0, -.44f, 0}, pantalon}, {{.28f, .14f, .42f}, {0, -.84f, .06f}, botte}});
    Col ceinture = hsv(30, .3f, .2f);
    std::vector<Cube> pt = {{{.74f, .72f, .4f}, {0, 1.26f, 0}, veste}, {{.7f, .12f, .38f}, {0, .93f, 0}, ceinture}};
    if (pilote) { pt.push_back({{.4f, .2f, .4f}, {0, 1.99f, -.01f}, veste}); pt.push_back({{.3f, .09f, .05f}, {0, 1.85f, .18f}, {.3f, .9f, 1, 1}}); pt.push_back({{.5f, .55f, .22f}, {0, 1.3f, -.3f}, gris(.13f)}); }
    m.torse = piece_cache(std::string("t") + (pilote ? "p" : "z") + cle(veste), pt);
    Col main = pilote ? gris(.08f) : peau;
    m.bras = piece_cache("b" + cle(veste) + cle(main), {{{.2f, .72f, .22f}, {0, -.34f, 0}, veste}, {{.16f, .16f, .18f}, {0, -.74f, 0}, main}});
    m.tete = piece_cache("tete", {{{.34f, .36f, .34f}, {0, 0, 0}, BLANCF}});
    m.couleur_tete = peau;
    if (pilote) { m.fusil = piece_cache("fusil", {{{1, 1, 1}, {0, 0, 0}, BLANCF}}); m.a_bras[1] = -75; }
    else { m.a_bras[0] = m.a_bras[1] = -80; m.yeux = piece_cache("yeux", {{{.07f, .05f, .04f}, {-.08f, 1.88f, .18f}, BLANCF}, {{.07f, .05f, .04f}, {.08f, 1.88f, .18f}, BLANCF}}); m.tete_rz = 8; }
}
static void tint(Piece& p, Col c) { p.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = toC(c); }
void humano_dessiner(Humano& m, V3 pos, float yaw, float scale, float flash) {
    Matrix root = matTRS(pos, yaw, scale);
    auto child = [&](V3 off, float rx, float rz) { return MatrixMultiply(MatrixMultiply(MatrixRotateZ(rad(rz)), MatrixMultiply(MatrixRotateX(rad(rx)), MatrixTranslate(off.x, off.y, off.z))), root); };
    for (int s = 0; s < 2; s++) { float sx = s == 0 ? -1 : 1; dessiner(m.jambe, child({sx * .18f, .88f, 0}, m.a_jambe[s], 0), 0, flash); dessiner(m.bras, child({sx * .47f, 1.55f, 0}, m.a_bras[s], 0), 0, flash); }
    dessiner(m.torse, root, 0, flash);
    tint(m.tete, m.couleur_tete); dessiner(m.tete, child({0, 1.84f, 0}, 0, m.tete_rz), 0, flash); tint(m.tete, BLANCF);
    if (m.pilote) { /* fusil porté : dessiné par l'appelant (couleur/échelle selon l'arme) */ }
    else { tint(m.yeux, m.couleur_yeux); dessiner(m.yeux, child({0, 0, 0}, 0, m.tete_rz), 0, 0); tint(m.yeux, BLANCF); }
}

// ====================================================================== registres / combat
namespace G {
std::vector<Pilote*> pilotes; std::vector<Zombie*> zombies; std::vector<Titan*> titans; std::vector<Grenade*> grenades; Player* joueur = nullptr; Partie* partie = nullptr;
static std::vector<Cible*> cimetiere;
Cible* proprietaire(Cible* s) { if (s && s->kind == 2) return static_cast<Titan*>(s)->owner; return s; }
Pilote* pilote_de(Cible* c) { Cible* p = proprietaire(c); return (p && p->kind == 0) ? static_cast<Pilote*>(p) : nullptr; }
bool allies(Cible* a, Cible* b) {
    Pilote* pa = pilote_de(a); Pilote* pb = pilote_de(b); if (!pa || !pb) return false;
    return pa == pb || (pa->equipe != 0 && pa->equipe == pb->equipe);
}
void msg(const std::string& t) { hud_msg(t); }
std::vector<Cible*> toutes_cibles() {
    std::vector<Cible*> r; for (auto p : pilotes) if (p->vivant && !p->elimine) r.push_back(p);
    for (auto z : zombies) r.push_back(z); for (auto t : titans) if (t->vivant && t->state == 1) r.push_back(t); return r;
}
void bruit(V3 pos, float rayon) { float r2 = rayon * rayon; for (auto z : std::vector<Zombie*>(zombies)) if (z->vivant && z->ecoute && (z->pos.x - pos.x) * (z->pos.x - pos.x) + (z->pos.z - pos.z) * (z->pos.z - pos.z) < r2) z->alerter(pos); }
static bool gros(Cible* c) { return c->kind == 2 || (c->kind == 0 && static_cast<Pilote*>(c)->mode == 1); }
void explode(V3 pos, float rayon, float degats, Cible* src, Cible* ignore) {
    Cible* prop = proprietaire(src);
    for (Cible* c : toutes_cibles()) {
        if (c == src || c == ignore) continue;
        if (c != prop && allies(src, c)) continue;
        V3 centre = {c->pos.x, c->pos.y + c->h * .5f, c->pos.z}; float dd = std::max(0.0f, dist(centre, pos) - c->r);
        if (dd < rayon) { float n = degats * std::max(.25f, 1 - dd / rayon); if (c == prop) n *= .5f; c->damage(n, src); }
    }
    FX::explosion(pos, rayon * 1.6f);
    bruit(pos, 55 + rayon * 4);
    if (joueur) FX::secousse(std::max(0.0f, 1 - dist(joueur->pos, pos) / 35) * rayon * .12f);
}
void hitscan(Cible* owner, V3 origin, V3 d, float portee, float degats, bool explosif, float er, float ed, const V3* debut, Col col, float ep) {
    d = norm(d); Cible* prop = proprietaire(owner); Titan* tt = (prop && prop->kind == 0) ? static_cast<Pilote*>(prop)->titan : nullptr;
    float tm = E::ray_world(origin, d, portee); Cible* cible = nullptr;
    for (Cible* c : toutes_cibles()) {
        if (c == owner || c == prop || c == tt || allies(owner, c)) continue;
        float t = E::ray_cyl(origin, d, c->pos.x, c->pos.z, c->pos.y, c->r, c->h);
        if (t >= 0 && t < tm) { tm = t; cible = c; }
    }
    V3 fin = origin + d * tm; bool tete = false;
    if (cible) {
        tete = !gros(cible) && fin.y > cible->pos.y + cible->h * .8f;
        if (degats > 0) cible->damage(degats * (tete ? 1.5f : 1.0f), owner, tete);
        if (prop == joueur) hud_touche(tete);
    }
    if (explosif && (cible || tm < portee - 1e-3f)) explode(fin - d * .15f, er, ed, owner);
    FX::trace(debut ? *debut : origin, fin, col, ep);
    bool titan_tire = owner && owner->kind == 2;
    if (debut && (titan_tire || prop != joueur)) FX::flash(*debut, titan_tire ? .45f : .2f);
    if (!cible) { if (tm < portee - 1e-3f) FX::impact(fin); }
    else if (cible->kind == 2) FX::etincelles(fin, {1, .55f, .1f, 1}, 4);
    else if (cible->kind == 0 && static_cast<Pilote*>(cible)->mode == 0) FX::sang(fin, 4);
}
static bool essai_pas(Cible* e, float dx, float dz, float v, float dt) {
    float nx = e->pos.x + dx * v * dt, nz = e->pos.z + dz * v * dt; bool ok = false;
    if (!E::touche(nx, e->pos.y + .5f, e->pos.z, e->r, e->h - .5f)) { e->pos.x = nx; ok = true; }
    if (!E::touche(e->pos.x, e->pos.y + .5f, nz, e->r, e->h - .5f)) { e->pos.z = nz; ok = true; }
    return ok;
}
bool marcher(Cible* e, float dx, float dz, float v, float dt) {
    bool ok = false; float l = std::sqrt(dx * dx + dz * dz);
    if (l > 1e-6f && v > 0) {
        dx /= l; dz /= l;
        if (e->detour_t > 0) {                      // contournement : on longe l'obstacle en biais, puis on retente tout droit
            e->detour_t -= dt; float a = rad(70 * e->detour_side), c = std::cos(a), s = std::sin(a);
            ok = essai_pas(e, dx * c - dz * s, dx * s + dz * c, v, dt);
            if (!ok) e->detour_side = -e->detour_side;
        } else {
            float x0 = e->pos.x, z0 = e->pos.z; ok = essai_pas(e, dx, dz, v, dt);
            float fait = hyp(e->pos.x - x0, e->pos.z - z0), voulu = v * dt;
            if (fait < voulu * .35f) { e->detour_t = clampf(4.0f / v, .7f, 2.2f); e->detour_side = -e->detour_side; }   // bloqué de face : on contourne
        }
    }
    float sol = E::hauteur_sol(e->pos.x, e->pos.z, e->r, e->pos.y + .55f);
    if (e->pos.y > sol + .02f) { e->vy += 22 * dt; e->pos.y = std::max(sol, e->pos.y - e->vy * dt); } else { e->pos.y = sol; e->vy = 0; }
    return ok && l > 1e-6f;
}
bool watchdog(Cible* e, float dt, bool veut, float dist) {
    if (!veut) { e->wd_t = 0; e->wd_d = 1e9f; return false; }
    if (e->wd_d > 1e8f) e->wd_d = dist;
    e->wd_t += dt; if (e->wd_t < 3.5f) return false;
    bool bloque = dist > e->wd_d - 1.0f; e->wd_t = 0; e->wd_d = dist; return bloque;      // pas rapproché d'au moins 1 m en 3,5 s
}
V3 spawn_loin() {
    V3 best = E::spawns[0]; float bd = -1;
    for (auto& s : E::spawns) { float d = 999; for (auto p : pilotes) if (p->vivant) d = std::min(d, hyp(s.x - p->pos.x, s.z - p->pos.z)); if (d > bd) { best = s; bd = d; } }
    return best;
}
void reset_entites() {
    for (auto z : zombies) { z->vivant = false; cimetiere.push_back(z); } zombies.clear();
    for (auto t : titans) cimetiere.push_back(t); titans.clear();
    for (auto g : grenades) delete g; grenades.clear(); E::fumees.clear();
}
// ---- corps à corps
bool melee_est_epee(Cible* p) { return p->kind == 2 || (p->kind == 0 && static_cast<Pilote*>(p)->mode == 1); }
static float degats_melee(Cible* c, bool epee, float mult) {
    if (c->kind == 1) { Zombie* z = static_cast<Zombie*>(c); if (z->geant) return epee ? std::ceil(z->hp_max / 4) * mult : 34 * mult; return z->hp + 1; }
    if (gros(c)) { float n = epee ? std::ceil(400 / 5.0f) : 12; float f = c->kind == 2 ? .7f : .8f; return n * mult / f; }
    float hm = static_cast<Pilote*>(c)->hp_max; return std::ceil(hm / (epee ? 2 : 3)) * mult;
}
void melee_coup(Cible* p, V3 origin, V3 dir, Arme* a, int& touche) {
    bool epee = melee_est_epee(p) || (a && a->nom == "Épée");
    Pilote* pr = pilote_de(p); float mult = pr ? (pr->mult_melee ? pr->mult_melee : 1) : 1;
    float portee = epee ? 7.0f : 3.0f, cosm = epee ? .25f : .45f, bas = epee ? 8.5f : 2.4f, haut = epee ? 2.0f : 1.4f; int maxi = epee ? 5 : 1;
    V3 fwd = {dir.x, 0, dir.z}; float fl = len(fwd); fwd = fl > 1e-6f ? fwd * (1 / fl) : V3{0, 0, 1};
    Cible* prop = proprietaire(p); Titan* tt = (prop && prop->kind == 0) ? static_cast<Pilote*>(prop)->titan : nullptr;
    std::vector<std::pair<float, Cible*>> res;
    for (Cible* c : toutes_cibles()) {
        if (c == p || c == prop || c == tt || allies(p, c)) continue;
        float dx = c->pos.x - origin.x, dz = c->pos.z - origin.z, dh = hyp(dx, dz), d = dh - c->r;
        if (d > portee) continue;
        float cy = c->pos.y + c->h * .5f - origin.y; if (cy < -bas || cy > haut) continue;
        if (dh > c->r + .6f && (dx * fwd.x + dz * fwd.z) / dh < cosm) continue;
        if (!E::visible(origin, {c->pos.x, c->pos.y + c->h * .55f, c->pos.z})) continue;
        res.push_back({std::max(0.0f, d), c});
    }
    std::sort(res.begin(), res.end(), [](auto& a, auto& b) { return a.first < b.first; });
    if ((int)res.size() > maxi) res.resize(maxi);
    touche = 0;
    for (auto& pc : res) {
        Cible* c = pc.second; float n = degats_melee(c, epee, mult);
        if (c->kind == 1) FX::sang({c->pos.x, c->pos.y + c->h * .6f, c->pos.z}, 6);
        else if (gros(c)) FX::etincelles({c->pos.x, c->pos.y + c->h * .5f, c->pos.z}, {1, .6f, .2f, 1}, 8);
        else FX::sang({c->pos.x, c->pos.y + c->h * .6f, c->pos.z}, 5);
        c->damage(n, p, false); touche++;
    }
    if (epee) { V3 o = origin + norm({dir.x, 0, dir.z}) * 4 + V3{0, -3.2f, 0}; FX::etincelles(o, {1, .5f, .15f, 1}, 7, 3.5f, .3f, .12f); if (prop == joueur) FX::secousse(touche ? .5f : .25f); }
    if (touche && prop == joueur) hud_touche(false);
}
// ---- grenades
void update_grenades(float dt) {
    for (size_t i = 0; i < grenades.size();) {
        Grenade* g = grenades[i]; g->fuse -= dt; g->v.y -= 22 * dt; V3& p = g->pos;
        float nx = p.x + g->v.x * dt; if (!E::touche(nx, p.y - .12f, p.z, .12f, .24f)) p.x = nx; else g->v.x *= -.4f;
        float nz = p.z + g->v.z * dt; if (!E::touche(p.x, p.y - .12f, nz, .12f, .24f)) p.z = nz; else g->v.z *= -.4f;
        float ny = p.y + g->v.y * dt, top = E::hits(p.x, ny - .12f, p.z, .12f, .24f); bool t = top > HNONE / 2;
        if (ny <= .12f || (t && g->v.y < 0)) { p.y = (t ? top : 0) + .12f; g->v.y = std::fabs(g->v.y) > 2 ? std::fabs(g->v.y) * .35f : 0; g->v.x *= .75f; g->v.z *= .75f; }
        else if (t) { g->v.y = 0; p.y = ny; } else p.y = ny;
        if (g->fuse <= 0) {
            if (g->frag) explode(p, 6, 110, g->owner); else E::fumees.push_back({{p.x, std::max(p.y, .5f), p.z}, 6, 12});
            delete g; grenades.erase(grenades.begin() + i);
        } else i++;
    }
    for (size_t i = 0; i < E::fumees.size();) { E::fumees[i].vie -= dt; if (E::fumees[i].vie <= 0) E::fumees.erase(E::fumees.begin() + i); else i++; }
}
void dessiner_grenades() {
    for (auto g : grenades) DrawSphereEx(g->pos, .14f, 6, 6, g->frag ? DARKGRAY : LIGHTGRAY);
    rlDisableDepthMask(); rlDisableBackfaceCulling();
    for (auto& f : E::fumees) DrawSphereEx(f.pos, f.rayon, 14, 16, {204, 204, 204, 190});
    rlEnableDepthMask(); rlEnableBackfaceCulling();
}
}

// ====================================================================== navigation
namespace Nav {
static const float AXES[5] = {-120, -60, 0, 60, 120};
static int idx(float v) { int b = 0; for (int i = 1; i < 5; i++) if (std::fabs(AXES[i] - v) < std::fabs(AXES[b] - v)) b = i; return b; }
static V3 proj_route(V3 p) { int ix = idx(p.x), iz = idx(p.z); if (std::fabs(p.x - AXES[ix]) < std::fabs(p.z - AXES[iz])) return {AXES[ix], 0, p.z}; return {p.x, 0, AXES[iz]}; }
static std::vector<V3> chemin(V3 a, V3 b) {
    int s[2] = {idx(a.x), idx(a.z)}, g[2] = {idx(b.x), idx(b.z)}; int prev[5][5]; bool vu[5][5] = {};
    for (auto& r : prev) for (int& c : r) c = -1;
    std::vector<std::array<int, 2>> q = {{s[0], s[1]}}; vu[s[0]][s[1]] = true;
    for (size_t h = 0; h < q.size(); h++) {
        auto c = q[h]; if (c[0] == g[0] && c[1] == g[1]) break;
        for (auto d : std::vector<std::array<int, 2>>{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) { int nx = c[0] + d[0], nz = c[1] + d[1]; if (nx < 0 || nz < 0 || nx >= 5 || nz >= 5 || vu[nx][nz]) continue; vu[nx][nz] = true; prev[nx][nz] = c[0] * 5 + c[1]; q.push_back({nx, nz}); }
    }
    std::vector<V3> r; int c = g[0] * 5 + g[1];
    while (c >= 0) { r.push_back({AXES[c / 5], 0, AXES[c % 5]}); c = prev[c / 5][c % 5]; if (c == s[0] * 5 + s[1]) { r.push_back({AXES[c / 5], 0, AXES[c % 5]}); break; } }
    std::reverse(r.begin(), r.end()); return r;
}
// ---- pathfinding A* sur la carte réelle des collisions (deux grilles : humains/zombies et Titans)
struct Grille {
    float cell = 2, r = .75f, org = 0, niveau = 0; int n = 0; std::vector<unsigned char> ok; std::vector<int> comp; int principale = -1; bool pret = false;
    void construire(float c, float rayon, float niv = 0) {
        cell = c; r = rayon; niveau = niv; org = -(DEMI_MAP + 4); n = (int)std::ceil(2 * (DEMI_MAP + 4) / cell); ok.assign(n * n, 0); comp.assign(n * n, -1);
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { float x = org + (i + .5f) * cell, z = org + (j + .5f) * cell; bool dedans = std::fabs(x) < DEMI_MAP - 1.5f && std::fabs(z) < DEMI_MAP - 1.5f; ok[i * n + j] = dedans && (niv <= 0.01f ? !E::touche(x, .5f, z, r, 1.3f) : (E::box_sous(x, niv, z) >= 0 && !E::touche(x, niv + .5f, z, r, 1.3f))); }
        int nc = 0; std::vector<int> taille; std::vector<int> pile;
        for (int s = 0; s < n * n; s++) {
            if (!ok[s] || comp[s] >= 0) continue; int t = 0; pile = {s}; comp[s] = nc;
            while (!pile.empty()) { int c0 = pile.back(); pile.pop_back(); t++; int ci_ = c0 / n, cj = c0 % n;
                for (int d = 0; d < 4; d++) { int ni = ci_ + (d == 0) - (d == 1), nj = cj + (d == 2) - (d == 3); if (ni < 0 || nj < 0 || ni >= n || nj >= n) continue; int k = ni * n + nj; if (ok[k] && comp[k] < 0) { comp[k] = nc; pile.push_back(k); } } }
            taille.push_back(t); nc++;
        }
        principale = 0; for (int k = 1; k < nc; k++) if (taille[k] > taille[principale]) principale = k;
        pret = true;
    }
    int cell_de(float x, float z, int rayon_max = 6, int comp_req = -1) const {            // cellule praticable la plus proche
        int i = std::clamp((int)std::floor((x - org) / cell), 0, n - 1), j = std::clamp((int)std::floor((z - org) / cell), 0, n - 1);
        if (ok[i * n + j] && (comp_req < 0 || comp[i * n + j] == comp_req)) return i * n + j;
        for (int rr = 1; rr <= rayon_max; rr++) { int best = -1; float bd = 1e9f;
            for (int di = -rr; di <= rr; di++) for (int dj = -rr; dj <= rr; dj++) { if (std::max(std::abs(di), std::abs(dj)) != rr) continue; int ni = i + di, nj = j + dj; if (ni < 0 || nj < 0 || ni >= n || nj >= n || !ok[ni * n + nj] || (comp_req >= 0 && comp[ni * n + nj] != comp_req)) continue; float d = (float)(di * di + dj * dj); if (d < bd) { bd = d; best = ni * n + nj; } }
            if (best >= 0) return best; }
        return -1;
    }
    V3 centre(int k) const { return {org + (k / n + .5f) * cell, 0, org + (k % n + .5f) * cell}; }
};
static std::map<int, Grille> GRILLES;
static Grille& grille(bool gros, float niv = 0) {
    int cle = (int)std::lround(niv * 10) * 2 + (gros ? 1 : 0); Grille& g = GRILLES[cle];
    if (!g.pret) { if (gros) g.construire(4, 2.3f, niv); else g.construire(2, .75f, niv); } return g;
}
static bool trait_libre(V3 a, V3 b, float r) {            // trois rayons (axe + côtés) : le corps passe-t-il en ligne droite ?
    V3 d = {b.x - a.x, 0, b.z - a.z}; float l = len(d); if (l < .1f) return true; d = d * (1 / l); V3 p = {-d.z, 0, d.x};
    for (float off : {0.0f, -r * .8f, r * .8f}) for (float hh : {.48f, 1.5f}) { V3 o = {a.x + p.x * off, a.y + hh, a.z + p.z * off}; if (E::ray_world(o, d, l) < l - .2f) return false; }
    return true;
}
static std::vector<V3> astar(V3 a, V3 b, bool gros, float niv) {
    Grille& g = grille(gros, niv); int s = g.cell_de(a.x, a.z); if (s < 0) return {}; int t = g.cell_de(b.x, b.z, 8, g.comp[s]); if (t < 0) return {};
    int N = g.n * g.n; std::vector<float> cost(N, 1e30f); std::vector<int> parent(N, -1); std::vector<char> clos(N, 0);
    using PQ = std::pair<float, int>; std::priority_queue<PQ, std::vector<PQ>, std::greater<PQ>> pq; cost[s] = 0; pq.push({0, s});
    auto h = [&](int k) { float dx = std::abs(k / g.n - t / g.n), dz = std::abs(k % g.n - t % g.n); return (dx + dz + (1.4142f - 2) * std::min(dx, dz)) * g.cell; };
    int guard = 0;
    while (!pq.empty() && guard++ < 60000) {
        int c = pq.top().second; pq.pop(); if (clos[c]) continue; clos[c] = 1; if (c == t) break;
        int ci_ = c / g.n, cj = c % g.n;
        for (int di = -1; di <= 1; di++) for (int dj = -1; dj <= 1; dj++) {
            if (!di && !dj) continue; int ni = ci_ + di, nj = cj + dj; if (ni < 0 || nj < 0 || ni >= g.n || nj >= g.n) continue; int k = ni * g.n + nj; if (!g.ok[k] || clos[k]) continue;
            if (di && dj && (!g.ok[(ci_ + di) * g.n + cj] || !g.ok[ci_ * g.n + nj])) continue;       // pas de coupe-coin
            float nc = cost[c] + (di && dj ? 1.4142f : 1.0f) * g.cell; if (nc < cost[k]) { cost[k] = nc; parent[k] = c; pq.push({nc + h(k), k}); }
        }
    }
    if (!clos[t]) return {};
    std::vector<V3> brut; for (int k = t; k >= 0; k = parent[k]) { V3 cc = g.centre(k); cc.y = niv; brut.push_back(cc); } std::reverse(brut.begin(), brut.end());
    std::vector<V3> out; V3 cur = {a.x, niv, a.z}; size_t i = 0; float rr = gros ? 2.0f : .6f;
    while (i < brut.size()) {
        size_t j = std::min(brut.size() - 1, i + 30); while (j > i && !trait_libre(cur, brut[j], rr)) j--;
        out.push_back(brut[j]); cur = brut[j]; i = j + 1;
    }
    return out;
}
std::vector<V3> vers_sol_gen(V3 a, V3 b, bool gros, float niv = 0) {
    float rr = gros ? 2.0f : .6f; V3 a0 = {a.x, niv, a.z}, b0 = {b.x, niv, b.z};
    if (trait_libre(a0, b0, rr)) return {b0};
    auto p = astar(a0, b0, gros, niv); if (!p.empty()) { p.push_back(b0); return p; }
    if (niv > .01f) return {b0};
    V3 pa = proj_route(a0), pb = proj_route(b0); std::vector<V3> r = {pa}; auto c = chemin(pa, pb); r.insert(r.end(), c.begin(), c.end()); r.push_back(pb); r.push_back(b0); return r;   // dernier recours
}
std::vector<V3> vers_sol(V3 a, V3 b) { return vers_sol_gen(a, b, false); }
std::vector<V3> chemin_toit(V3 a, V3 b, float niv) { return vers_sol_gen(a, b, false, niv); }
std::vector<V3> vers_sol_gros(V3 a, V3 b) { return vers_sol_gen(a, b, true); }
bool sol_joignable(float x, float z) { Grille& g = grille(false); int k = g.cell_de(x, z, 3); return k >= 0 && g.comp[k] == g.principale; }
static bool toit_joignable(const struct AccesToit& a, float x, float z) { Grille& g = grille(false, a.y); int s = g.cell_de(a.toit.x, a.toit.z, 3), t = g.cell_de(x, z, 2); return s >= 0 && t >= 0 && g.comp[s] == g.comp[t]; }
static bool meme_toit(const AccesToit& a, float x, float z, float y) {
    int i = E::box_sous(a.toit.x, a.y, a.toit.z), j = E::box_sous(x, y, z);
    if (i >= 0 && j >= 0) return i == j;
    return E::libre({a.toit.x, a.y, a.toit.z}, {x, y, z});
}
static const AccesToit* meilleur_acces(float x, float z, float y) {
    const AccesToit* best = nullptr; float bd = 1e9f;
    for (auto& a : E::acces_toit) if (std::fabs(a.y - y) < 1.0f && meme_toit(a, x, z, y)) { float d = hyp(a.toit.x - x, a.toit.z - z); if (d < bd) { bd = d; best = &a; } }
    return best;
}
std::vector<V3> route(V3 depart, V3 but, float but_y) {
    const AccesToit* dest_acces = (but_y > 2.5f && !E::acces_toit.empty()) ? meilleur_acces(but.x, but.z, but_y) : nullptr;
    if (dest_acces && !toit_joignable(*dest_acces, but.x, but.z)) dest_acces = nullptr;
    if (dest_acces && std::fabs(depart.y - dest_acces->y) < 1.0f && meme_toit(*dest_acces, depart.x, depart.z, depart.y)) return chemin_toit(depart, but, dest_acces->y);   // déjà sur le bon toit
    if (depart.y > 2.5f) {                                       // sur un toit / une rampe : il faut d'abord redescendre par l'escalier
        const AccesToit* cur = nullptr; float bd2 = 1e9f;
        for (auto& a : E::acces_toit) if (std::fabs(a.y - depart.y) < 1.0f && meme_toit(a, depart.x, depart.z, depart.y)) { cur = &a; break; }
        if (!cur) for (auto& a : E::acces_toit) if (std::fabs(a.y - depart.y) < 1.0f) { float d = hyp(a.toit.x - depart.x, a.toit.z - depart.z); if (d < bd2 && d < 70) { bd2 = d; cur = &a; } }
        if (cur) { const AccesToit& a = *cur;
            std::vector<V3> r = chemin_toit(depart, a.toit, a.y);
            for (int k = (int)a.pts.size() - 2; k >= 0; k--) r.push_back(a.pts[k]);
            r.push_back({a.base.x, .05f, a.base.z});
            auto reste = route({a.base.x, 0, a.base.z}, but, but_y); r.insert(r.end(), reste.begin(), reste.end()); return r;
        }
    }
    if (depart.y > .6f && but_y < depart.y - .5f) {            // à mi-escalier, cible plus bas : on redescend les marches
        const AccesToit* st = nullptr; float bd = 6.0f;
        for (auto& a : E::acces_toit) { if (a.y < depart.y - .5f) continue; float d = hyp(a.base.x - depart.x, a.base.z - depart.z); for (auto& p : a.pts) d = std::min(d, hyp(p.x - depart.x, p.z - depart.z)); if (d < bd) { bd = d; st = &a; } }
        if (st) {
            std::vector<V3> r; for (int k = (int)st->pts.size() - 1; k >= 0; k--) if (st->pts[k].y < depart.y - .6f) r.push_back(st->pts[k]);
            r.push_back({st->base.x, .05f, st->base.z});
            auto reste = route({st->base.x, 0, st->base.z}, but, but_y); r.insert(r.end(), reste.begin(), reste.end()); return r;
        }
    }
    if (dest_acces) {
        const AccesToit* best = dest_acces; std::vector<V3> r;
        if (depart.y > .8f) {                                    // déjà dans l'escalier : on reprend devant soi
            size_t k = 0; while (k < best->pts.size() && best->pts[k].y < depart.y - .6f) k++;
            if (k >= best->pts.size()) k = best->pts.size() - 1;
            for (; k < best->pts.size(); k++) r.push_back(best->pts[k]);
        } else { r = vers_sol(depart, best->base); if (!r.empty()) r.back() = {best->base.x, .05f, best->base.z}; r.insert(r.end(), best->pts.begin(), best->pts.end()); }
        auto sur_toit = chemin_toit(best->toit, but, best->y); r.insert(r.end(), sur_toit.begin(), sur_toit.end()); return r;
    }
    if (but_y > 2.5f && depart.y <= .8f) {                            // cible à mi-hauteur d'un escalier : on monte jusqu'à elle
        const AccesToit* st = nullptr; float bd = 6.0f;
        for (auto& a : E::acces_toit) { if (a.y < but_y - .5f) continue; float d = hyp(a.base.x - but.x, a.base.z - but.z); for (auto& p : a.pts) d = std::min(d, hyp(p.x - but.x, p.z - but.z)); if (d < bd) { bd = d; st = &a; } }
        if (st) {
            std::vector<V3> r = vers_sol(depart, st->base); if (!r.empty()) r.back() = {st->base.x, .05f, st->base.z};
            for (auto& p : st->pts) if (p.y <= but_y + .6f) r.push_back(p);
            r.push_back(but); return r;
        }
    }
    if (but_y > 2.5f) return vers_sol(depart, {but.x, 0, but.z});      // toit inaccessible : on va au pied
    return vers_sol(depart, but);
}
bool joignable(float x, float z, float y) { if (y <= 2.5f) return sol_joignable(x, z); const AccesToit* a = meilleur_acces(x, z, y); return a && toit_joignable(*a, x, z); }
}

// ====================================================================== craft
static void cmsg(Pilote* p, const std::string& t) { if (p == G::joueur) G::msg(t); }
bool craft_soin(Pilote* p) { auto& d = p->craft; d.vert++; if (d.kits_soin >= 3) { cmsg(p, "Kits de soin au maximum."); return false; } d.kits_soin++; cmsg(p, "Kit de soin (" + std::to_string(d.kits_soin) + "/3) - touche H"); return true; }
bool craft_armure(Pilote* p) { auto& d = p->craft; d.bleu++; if (d.kits_armure >= 3) { cmsg(p, "Kits d'armure au maximum."); return false; } d.kits_armure++; cmsg(p, "Kit d'armure (" + std::to_string(d.kits_armure) + "/3) - touche B"); return true; }
static bool soin_utiliser(Pilote* p) { auto& d = p->craft; if (d.kits_soin <= 0 || !p->vivant || p->hp >= p->hp_max - 1) return false; d.kits_soin--; p->hp = std::min(p->hp_max, p->hp + 45); cmsg(p, "Soin +45"); return true; }
static bool armure_utiliser(Pilote* p) { auto& d = p->craft; if (d.kits_armure <= 0 || !p->vivant || p->armure >= 50) return false; d.kits_armure--; p->armure = 50; cmsg(p, "Armure +50"); return true; }
bool craft_soin_utiliser(Pilote* p) { return soin_utiliser(p); }
bool craft_armure_utiliser(Pilote* p) { return armure_utiliser(p); }
void craft_auto(Pilote* p) { if (p == G::joueur || !p->vivant) return; if (p->hp < p->hp_max * .5f) soin_utiliser(p); else if (p->armure <= 0) armure_utiliser(p); }
void donner_arme_classe(Pilote* p) {
    auto a = arme_classe(p->classe); ArmeP c = p->armes[0];
    p->armes.clear(); p->armes.push_back(c); p->armes.push_back(a);
    if (p->idx == 1) p->sur_changement_arme(); else p->choisir_arme(1);
}
bool craft_on_loot(Pilote* p, LootType t, bool& gere) {
    gere = true; auto& d = p->craft;
    if (t == L_ARME) {
        d.rouge++; int n = d.rouge;
        if (n >= 3 && d.niveau_melee == 0) { d.niveau_melee = 1; p->mult_melee = 1.25f; for (auto& a : p->armes) if (a->melee) { a->degats *= 1.25f; break; } cmsg(p, "Amélioration mêlée : +25 % de dégâts !"); }
        if (n >= 5 && !d.arme_classe) { donner_arme_classe(p); d.arme_classe = true; cmsg(p, "Arme de classe débloquée : " + p->armes[1]->nom); }
        if (n >= 10 && !d.augmentation && d.arme_classe) {
            auto a = p->armes[1]; a->degats *= 1.35f; a->cadence *= .83f; a->chargeur = (int)std::lround(a->chargeur * 1.5f); a->munitions = a->chargeur;
            d.augmentation = true; p->sur_changement_arme(); cmsg(p, "Arme à feu améliorée !");
        }
        return true;
    }
    if (t == L_GRENADE) {
        d.orange++;
        if (p->nb_tactique < 4) { p->nb_tactique++; cmsg(p, "+1 " + p->tactique + " (" + std::to_string(p->nb_tactique) + "/4)"); return true; }
        if (p->armes.size() > 1 && p->armes[1] != p->armes[0] && p->armes[1]->reserve >= 0) { p->armes[1]->ajouter_munitions(30); cmsg(p, "Grenades au maximum : +30 munitions"); return true; }
        cmsg(p, "Stock de grenades plein."); return false;
    }
    if (t == L_ARMURE) return craft_armure(p);
    if (t == L_SOIN) return craft_soin(p);
    gere = false; return false;
}

// ====================================================================== Pilote
void Pilote::appliquer_classe(const Classe* c) {
    classe = c; craft = CraftData(); mult_melee = 1;
    hp_max = hp = c->hp; armure = 0; tactique = c->tactique; nb_tactique = c->nb; vitesse_mult = c->vitesse;
    auto k = Armes::couteau();
    if (arme_depart) { armes = {k, arme_classe(c)}; idx = 1; craft.arme_classe = true; } else { armes = {k, k}; idx = 0; }
    sur_changement_arme();
}
void Pilote::nouvelle_partie(const Classe* c, V3 p) { score = score_final = kills = 0; elimine = false; t_elim = -1; titan = nullptr; titan_cd = 0; appliquer_classe(c); reapparaitre(p); }
void Pilote::reapparaitre(V3 p) {
    appliquer_classe(classe); vivant = true; attente = false; mode = 0; pos = {p.x, 0, p.z}; vy = 0; invuln = 3; since_hit = 99; cool_tact = 0; apres_respawn();
}
void Pilote::damage(float n, Cible* src, bool) {
    if (!vivant || elimine || invuln > 0 || !G::partie->actif()) return;
    if (src && src != this && G::allies(src, this)) return;
    since_hit = 0;
    if (armure > 0) { float a = std::min(armure, n * .6f); armure -= a; n -= a; }
    hp -= n; sur_degats(src);
    if (hp <= 0) G::partie->mort_pilote(this, src);
}
void Pilote::tick(float dt) {
    invuln = std::max(0.0f, invuln - dt); since_hit += dt; cool_tact -= dt; if (titan_cd > 0) titan_cd -= dt;
    if (mode == 0 && since_hit > 4 && hp < hp_max) hp = std::min(hp_max, hp + 6 * dt);
    for (auto& a : armes) a->update(dt);
}
bool Pilote::lancer_tactique(V3 origin, V3 vitesse) {
    if (nb_tactique <= 0 || cool_tact > 0) return false;
    nb_tactique--; cool_tact = 1; G::grenades.push_back(new Grenade{this, origin, vitesse, tactique == "frag"}); return true;
}
bool Pilote::titan_actif() const { return titan && titan->vivant; }
ArmeP Pilote::arme_pour_titan() {
    if (armes.size() > 1 && !armes[1]->melee) return armes[1];
    return arme_classe(classe);
}
bool Pilote::appeler_titan() {
    bool j = this == G::joueur;
    if (mode == 1 || titan_actif()) return false;
    if (titan_cd > 0) { if (j) G::msg("Titan pret dans " + std::to_string((int)std::ceil(titan_cd)) + "s"); return false; }
    if (score < COUT_TITAN) { if (j) G::msg("Il faut " + std::to_string(COUT_TITAN) + " points pour appeler un Titan"); return false; }
    V3 base = norm({avant().x, 0, avant().z});
    for (int ang : {0, 35, -35, 70, -70, 110, -110, 180}) {
        float a = rad((float)ang); V3 fw = {base.x * std::cos(a) + base.z * std::sin(a), 0, -base.x * std::sin(a) + base.z * std::cos(a)};
        for (int d = 14; d < 60; d += 4) {
            V3 spot = pos + fw * (float)d;
            if (std::fabs(spot.x) < DEMI_MAP - 6 && std::fabs(spot.z) < DEMI_MAP - 6 && !E::touche(spot.x, 0, spot.z, 3.5f, 6)) {
                score -= COUT_TITAN; titan = new Titan(this, spot); if (j) G::msg("TITANFALL ! (-" + std::to_string(COUT_TITAN) + " pts)"); return true;
            }
        }
    }
    return false;
}
bool Pilote::ramasser(Loot* l) {
    std::string texte;
    if (l->type == L_SOIN) { if (hp >= hp_max - 1) return false; hp = std::min(hp_max, hp + 45); texte = "Medikit +45 PV"; }
    else if (l->type == L_ARMURE) { if (armure >= 50) return false; armure = 50; texte = "Armure +50"; }
    else if (l->type == L_MUNITIONS) { for (auto& a : armes) a->ajouter_munitions(a->chargeur * 2); texte = "Munitions"; }
    else if (l->type == L_GRENADE) { if (nb_tactique >= 4) return false; nb_tactique++; texte = "+1 " + tactique; }
    if (this == G::joueur && !texte.empty()) G::msg(texte);
    return true;
}

// ====================================================================== Player
bool craft_soin_utiliser(Pilote* p); bool craft_armure_utiliser(Pilote* p);
#define craft_soin_utiliser_ext craft_soin_utiliser
#define craft_armure_utiliser_ext craft_armure_utiliser
Reglages REG;
static bool A_avant() { return T::tenu(T::AVANT); } static bool A_arriere() { return T::tenu(T::ARRIERE); }
static bool A_gauche() { return T::tenu(T::GAUCHE); } static bool A_droite() { return T::tenu(T::DROITE); }
static bool A_sprint() { return T::tenu(T::SPRINT); } static bool A_saut_tenu() { return T::tenu(T::SAUT); }
static bool A_viser() { return T::tenu(T::VISER); } static bool A_tir_tenu() { return T::tenu(T::TIR); }

Player::Player() : Pilote("Toi", {0, .5f, 1, 1}) { fov = REG.fov; }
ArmeP Player::arme_active() { if (mode == 1 && arme_titan) return arme_titan; return armes.empty() ? nullptr : armes[idx]; }
void Player::sur_changement_arme() { auto a = arme_active(); if (a) gun = a->vue; }
void Player::sur_degats(Cible*) { hud_degats(); }
void Player::avant_mort() { pivot_y = .4f; zoom = false; fin_grappin(); }
void Player::apres_respawn() {
    vel = {0, 0, 0}; vy = 0; grounded = true; jumps = 1; wall_t = 0; has_wall = false; wallrunning = false; slide_t = 0; grimpe = false; pitch = 0; arme_titan = nullptr; pivot_y = 1.65f; fin_grappin(); sur_changement_arme();
}
void Player::tick(float dt) { Pilote::tick(dt); if (arme_titan) arme_titan->update(dt); }
void Player::damage(float n, Cible* src, bool tete) {
    if (mode == 1) {
        if (!vivant || invuln > 0 || !G::partie->actif()) return;
        if (src && src != this && G::allies(src, this)) return;
        since_hit = 0; titan_hp -= n * .8f; sur_degats(src); if (titan_hp <= 0) ejecter(true, src); return;
    }
    Pilote::damage(n, src, tete);
}
void Player::embarquer() {
    Titan* t = titan; mode = 1; titan_hp = t->hp; vy = 0; arme_titan = Armes::pour_titan(arme_pour_titan()); pos = {t->pos.x, 0, t->pos.z};
    t->embarquer(); zoom = false; sur_changement_arme(); G::msg("Titan : [E] pour ejecter");
}
void Player::ejecter(bool force, Cible* tueur) {
    Titan* t = titan; V3 p0 = pos; mode = 0; vy = 16; grounded = false; jumps = 1;
    if (t && t->vivant) {
        if (force) { t->pos = {p0.x, 0, p0.z}; t->detruire(true, tueur); }
        else { V3 fw = norm({avant().x, 0, avant().z}); V3 p = p0 + fw * 4; if (E::touche(p.x, 0, p.z, 1.6f, 5.5f)) p = p0; t->debarquer(p, titan_hp); }
    }
    sur_changement_arme();
}
void Player::regarder() {
    auto a = arme_active(); float ratio = (zoom && a && a->zoom > 0) ? clampf(a->zoom / REG.fov, .25f, 1) : 1;
    float sens = REG.sensibilite * ratio * (1.78f / (float)GetScreenWidth()); Vector2 d = GetMouseDelta();
    yaw -= d.x * sens; pitch = clampf(pitch - d.y * sens * (REG.inverser_y ? -1.0f : 1.0f), -89, 89);
}
bool Player::tente_grimper() {
    if (mode == 1 || grimpe || !vivant) return false;
    V3 fw = {avant().x, 0, avant().z}; if (len(fw) < .1f) return false; fw = norm(fw);
    float qx = pos.x + fw.x * (r + .35f), qz = pos.z + fw.z * (r + .35f), top = E::hits(qx, pos.y + .5f, qz, .15f, 2.2f);
    if (top < HNONE / 2 || top > pos.y + 2.7f) return false;
    float tx = pos.x + fw.x * (r + .85f), tz = pos.z + fw.z * (r + .85f), sol = E::hits(tx, top - .3f, tz, r * .8f, .1f);
    if (sol < HNONE / 2 || std::fabs(sol - top) > .05f) return false;
    if (E::touche(tx, top + .02f, tz, r, h - .02f)) return false;
    grimpe = true; g0 = pos; g1 = {tx, top, tz}; gp = 0; slide_t = 0; return true;
}
void Player::lancer_grappin() {
    if (grappin || grappin_cd > 0) return;
    V3 o = oeil(), d = fwd3(); float t = E::ray_world(o, d, 50);
    if (t < 50) { grappin = true; g_pt = o + d * t; grappin_t = 1.2f; slide_t = 0; }
}
void Player::fin_grappin(bool boost) { grappin = false; if (boost) { grappin_cd = 3; vy = std::max(vy, 6.0f); } }
static V3 point_vise(Player* p, float portee = 220, bool* ok = nullptr) {
    V3 o = p->oeil(), d = p->fwd3(); float t = E::ray_world(o, d, portee);
    if (t >= portee - 1e-3f && d.y >= -.02f) { if (ok) *ok = false; return {}; }
    V3 q = o + d * t; float lim = DEMI_MAP - 6; if (ok) *ok = true;
    return {clampf(q.x, -lim, lim), 0, clampf(q.z, -lim, lim)};
}
bool Pilote::donner_ordre(const std::string& ordre) {
    Titan* t = titan; bool j = this == G::joueur;
    if (!t || !t->vivant) { if (j) G::msg("Pas de Titan à commander"); return false; }
    if (mode == 1 || t->state != 1) { if (j && t->state == 0) G::msg("Le Titan est encore en chute"); return false; }
    V3 pt{}; bool has = false;
    if (ordre == "aller") { bool ok; pt = point_vise(G::joueur, 220, &ok); if (!ok) { if (j) G::msg("Vise le sol pour désigner un point"); return false; } has = true; }
    t->ordonner(ordre, has ? &pt : nullptr);
    static std::map<std::string, std::string> N = {{"suivre", "SUIVRE"}, {"garder", "GARDER LA POSITION"}, {"aller", "ALLER AU POINT"}, {"attaquer", "ATTAQUER"}};
    if (j) G::msg("Titan : " + N[ordre]); return true;
}
void Player::frapper() {
    ArmeP a;
    if (G::melee_est_epee(this)) { if (!melee_arme || melee_arme->nom != "Épée") melee_arme = Armes::epee(); a = melee_arme; }
    else { for (auto& x : armes) if (x->melee) { a = x; break; } if (!a) a = Armes::couteau(); }
    if (!vivant || !a->pret()) return;
    a->cool = a->cadence; melee_arme = a; melee_t = .3f; int t; G::melee_coup(this, oeil(), fwd3(), a.get(), t);
}
void Player::tirer() {
    auto a = arme_active(); if (!a) return;
    if (a->melee) { frapper(); return; }
    V3 o = oeil(), d = fwd3(), R = right3(), F = d, U = cross3(R, F);
    V3 debut = o + R * .3f - U * .2f + F * .5f;
    if (a->tirer(this, o, d, &debut, zoom)) {
        gun.z -= a->recul; pitch += a->recul * 6; flash_t = .045f; FX::secousse(a->recul * 2.5f);
        if (mode == 0 && a->munitions == 0) a->recharger();
    }
}
void Player::entrees() {
    Partie* P = G::partie;
    if (!P->actif() || !vivant || elimine) return;
    bool titanm = mode == 1;
    if (T::presse(T::SAUT)) {
        if (slide_t > 0) { float v = std::min(24.0f, slide_v * 1.06f); vel = {slide_dir.x * v, 0, slide_dir.z * v}; slide_t = 0; }
        if (grappin) fin_grappin();
        if (grounded) { if (!tente_grimper()) { vy = titanm ? 7.0f : 9.0f; grounded = false; } }
        else if (wallrunning) { vy = 9; if (has_wall) { vel = vel + wall_n * 9; vel = vel + V3{avant().x, 0, avant().z} * 3; has_wall = false; wallrunning = false; } }
        else if (!titanm) { if (!tente_grimper() && jumps > 0) { vy = 8; jumps--; } }
    }
    if (T::presse(T::RECHARGE) && !titanm) arme()->recharger();
    if (T::presse(T::ARME1) && !titanm) choisir_arme(0);
    if (T::presse(T::ARME2) && !titanm) choisir_arme(1);
    if (GetMouseWheelMove() != 0 && !titanm) choisir_arme(1 - idx);
    if (T::presse(T::TITAN) && !titanm) appeler_titan();
    if (T::presse(T::INTERAGIR)) {
        if (titanm) ejecter();
        else if (titan_actif() && titan->state == 1 && dist(pos, titan->pos) < 8) embarquer();
    }
    if (T::presse(T::TACTIQUE) && !titanm) { V3 R = right3(), F = fwd3(), U = cross3(R, F); lancer_tactique(oeil() + F * .6f, F * 18 + V3{0, 3, 0}); }
    if (T::presse(T::GRAPPIN) && !titanm) lancer_grappin();
    if (T::presse(T::GLISSE) && grounded && !titanm && slide_t <= 0) {
        V3 v = {vel.x, 0, vel.z};
        if (len(v) > 6) { slide_t = 2.2f; slide_dir = norm(v); slide_v = std::min(24.0f, std::max(len(v) * 1.12f, 13.0f)); }
    }
    auto a = arme_active();
    if (T::presse(T::TIR) && a && !a->autom) tirer();
    if (T::presse(T::MELEE)) frapper();
    if (!titanm) {
        if (T::presse(T::ORDRE_SUIVRE)) donner_ordre("suivre");
        if (T::presse(T::ORDRE_GARDER)) donner_ordre("garder");
        if (T::presse(T::ORDRE_ALLER)) donner_ordre("aller");
        if (T::presse(T::ORDRE_ATTAQUER)) donner_ordre("attaquer");
    }
    if (T::presse(T::SOIN)) craft_soin_utiliser_ext(this);
    if (T::presse(T::ARMURE)) craft_armure_utiliser_ext(this);
}
void Player::vol_spectateur(float dt) {
    regarder(); float f = (float)A_avant() - (float)A_arriere(), s = (float)A_droite() - (float)A_gauche();
    V3 d = fwd3() * f + right3() * s; pos = pos + d * ((A_sprint() ? 25.0f : 14.0f) * dt); if (pos.y < .5f) pos.y = .5f; pivot_y = 1.65f;
}
void Player::animer_grimpe(float dt, V3 wish) {
    gp += dt / .42f; float p = std::min(1.0f, gp), up = std::min(1.0f, p / .6f), fw = std::max(0.0f, (p - .4f) / .6f);
    pos = {lerpf(g0.x, g1.x, fw), lerpf(g0.y, g1.y, up), lerpf(g0.z, g1.z, fw)}; vel = {0, 0, 0}; vy = 0;
    if (p >= 1) { grimpe = false; grounded = true; jumps = 1; wall_t = 0; vel = {wish.x * 3, 0, wish.z * 3}; }
}
void Player::appliquer_vitesse(float dt, V3 wish, float speed, bool sprint) {
    if (slide_t > 0) return;
    bool sol = grounded; V3 v = {vel.x, 0, vel.z}; float mag = len(v);
    if (len(wish) > 0) {
        V3 dirv = mag > .1f ? v * (1 / mag) : wish;
        if (mag > .1f && dot(dirv, wish) < -.3f) { float k = std::min(1.0f, (sol ? 14.0f : 3.5f) * dt); vel.x = lerpf(vel.x, wish.x * speed, k); vel.z = lerpf(vel.z, wish.z * speed, k); return; }
        V3 d = norm(lerp3(dirv, wish, std::min(1.0f, (sol ? 14.0f : 2.6f) * dt)));
        if (mag > speed) { float taux = sol ? (sprint ? 1.0f : 2.0f) : (wallrunning ? 1.0f : .3f); mag = speed + (mag - speed) * std::exp(-taux * dt); }
        else mag = lerpf(mag, speed, std::min(1.0f, (sol ? 14.0f : (wallrunning ? 8.0f : 3.5f)) * dt));
        vel.x = d.x * mag; vel.z = d.z * mag;
    } else { float k = std::min(1.0f, (sol ? 10.0f : 1.0f) * dt); vel.x = lerpf(vel.x, 0, k); vel.z = lerpf(vel.z, 0, k); }
}
void Player::mouvement(float dt, float f, V3 wish, bool titan_) {
    bool sprint = A_sprint();
    float speed = titan_ ? (sprint ? 10.0f : 6.0f) : (sprint ? 12.0f : 8.0f) * vitesse_mult;
    if (wallrunning) speed *= 1.25f; if (zoom) speed *= .7f;
    appliquer_vitesse(dt, wish, speed, sprint);
    if (grappin) {
        V3 d = g_pt - (pos + V3{0, 1, 0}); float l = len(d); grappin_t -= dt;
        if (l < 2.5f || grappin_t <= 0) fin_grappin(true); else { V3 dn = d * (1 / l); vel.x = dn.x * 28; vel.z = dn.z * 28; vy = dn.y * 28; }
    }
    if (slide_t > 0) {
        slide_t -= dt; slide_v = std::max(0.0f, slide_v - 4.5f * dt);
        if (len(wish) > 0) slide_dir = norm(lerp3(slide_dir, wish, std::min(1.0f, 1.6f * dt)));
        vel.x = slide_dir.x * slide_v; vel.z = slide_dir.z * slide_v;
        if (!grounded || slide_v < 6.5f) slide_t = 0;
    }
    if (has_wall && !grounded && A_saut_tenu() && f > 0 && !titan_) { if (tente_grimper()) return; }
    bool stick = has_wall && !grounded && f > 0 && wall_t < 3 && !titan_ && !grappin;
    wallrunning = stick;
    if (stick) { wall_t += dt; vy = std::max(vy - 6 * dt, -1.5f); vel = vel - wall_n * 4; jumps = 1; }
    else if (!grappin) vy -= 22 * dt;
    has_wall = false; float st = .5f;
    float nx = pos.x + vel.x * dt;
    if (!E::touche(nx, pos.y + st, pos.z, r, h - st)) pos.x = nx; else { has_wall = true; wall_n = {-(vel.x >= 0 ? 1.0f : -1.0f), 0, 0}; vel.x = 0; }
    float nz = pos.z + vel.z * dt;
    if (!E::touche(pos.x, pos.y + st, nz, r, h - st)) pos.z = nz; else { has_wall = true; wall_n = {0, 0, -(vel.z >= 0 ? 1.0f : -1.0f)}; vel.z = 0; }
    float ny = pos.y + vy * dt, top = E::hits(pos.x, ny, pos.z, r, h);
    if (top < HNONE / 2 && ny > 0) { pos.y = ny; grounded = false; }
    else { if (vy < 0) { pos.y = top > HNONE / 2 ? top : 0; grounded = true; jumps = 1; wall_t = 0; } vy = 0; }
    float lim = DEMI_MAP - 2; pos.x = clampf(pos.x, -lim, lim); pos.z = clampf(pos.z, -lim, lim);
}
void Player::update(float dt) {
    Partie* P = G::partie;
    if (!P->en_cours || P->pause) return;
    if (elimine) { vol_spectateur(dt); return; }
    if (!vivant || P->fini) return;
    tick(dt);
    if (melee_t > 0) melee_t -= dt;
    grappin_cd -= dt; bool titanm = mode == 1; r = titanm ? 1.2f : .4f; h = titanm ? 5.6f : 1.8f;
    auto a = arme_active(); regarder();
    pivot_y = lerpf(pivot_y, titanm ? 5.2f : (slide_t > 0 ? .9f : 1.65f), std::min(1.0f, 8 * dt));
    float f = (float)A_avant() - (float)A_arriere(), s = (float)A_droite() - (float)A_gauche();
    V3 wish = fwd3() * f + right3() * s; wish = {wish.x, 0, wish.z}; if (len(wish) > 0) wish = norm(wish);
    if (grimpe) animer_grimpe(dt, wish); else mouvement(dt, f, wish, titanm);
    float tilt = (wallrunning && has_wall) ? 9 * dot(wall_n, right3()) : 0; roll = lerpf(roll, tilt, std::min(1.0f, 8 * dt));
    zoom = a && a->zoom > 0 && A_viser() && !grimpe; lunette = zoom && a->zoom < 30;
    float vit = hyp(vel.x, vel.z);
    if (zoom) fov = lerpf(fov, a->zoom, std::min(1.0f, 10 * dt)); else fov = lerpf(fov, REG.fov + vit * REG.fov_vitesse, std::min(1.0f, 4 * dt));
    if (grounded && vit > 1) bob += dt * (6 + vit * .5f);
    float amp = zoom ? 0 : std::min(1.0f, vit / 8); V3 cible = a->vue; float k = std::min(1.0f, 16 * dt);
    if (zoom) { extern V3 vm_ads(const std::string&, bool); cible = vm_ads(a->modele, a->titan); }
    gun.x = lerpf(gun.x, cible.x + std::sin(bob) * .012f * amp, k); gun.y = lerpf(gun.y, cible.y + std::fabs(std::cos(bob)) * .012f * amp, k); gun.z = lerpf(gun.z, cible.z, std::min(1.0f, 15 * dt));
    if (flash_t > 0) flash_t -= dt;
    FX::shake *= std::max(0.0f, 1 - 9 * dt); shake_x = rnd(-1, 1) * FX::shake * .12f; shake_y = rnd(-1, 1) * FX::shake * .12f;
    if (A_tir_tenu() && a->autom && a->pret()) tirer();
}

// ====================================================================== PiloteIA
struct BotArme { V3 sc; Col col; };
static BotArme bot_arme(const std::string& m) {
    if (m == "Kriss Vector") return {{.1f, .12f, .55f}, hsv(180, .8f, .3f)}; if (m == "DRS-50") return {{.09f, .1f, 1.1f}, hsv(200, .05f, .9f)};
    if (m == "Pompe") return {{.1f, .12f, .85f}, hsv(25, .7f, .3f)}; if (m == "Pistolet") return {{.06f, .08f, .3f}, gris(.85f)};
    if (m == "Couteau") return {{.05f, .05f, .42f}, hsv(210, .08f, .92f)}; if (m == "Épée") return {{.06f, .06f, 1.2f}, hsv(210, .08f, .92f)};
    return {{.1f, .12f, .75f}, hsv(85, .38f, .27f)};
}
PiloteIA::PiloteIA(const std::string& n, Col c) : Pilote(n, c) { humano_construire(m, c, hsv(0, 0, .18f), hsv(25, .4f, .8f), true); m.a_bras[1] = -75; }
void PiloteIA::recolorer(Col c) { couleur = c; humano_construire(m, c, hsv(0, 0, .18f), hsv(25, .4f, .8f), true); m.a_bras[1] = -75; }
void PiloteIA::apres_respawn() { cible = nullptr; a_dest = false; bloque = 0; sur_changement_arme(); }
void PiloteIA::avant_mort() {}
void PiloteIA::sur_degats(Cible* src) {
    flash = .08f; Pilote* p = G::pilote_de(src);
    if (p && p != this && p->vivant) { cible = p; react = .3f; }
}
void PiloteIA::penser() {
    V3 oe = oeil(); std::vector<std::pair<float, Cible*>> c;
    for (Cible* t : G::toutes_cibles()) {
        if (t == this || t == titan || G::allies(this, t)) continue;
        float d = hyp(t->pos.x - pos.x, t->pos.z - pos.z), lim, w;
        if (t->kind == 1) { lim = 26; w = 1; } else if (t->kind == 2) { lim = 55; w = 1.3f; } else { lim = 65; w = .6f; }
        if (d < lim) c.push_back({d * w, t});
    }
    std::sort(c.begin(), c.end(), [](auto& a, auto& b) { return a.first < b.first; });
    Cible* prev = cible; cible = nullptr; vu = false;
    for (size_t i = 0; i < c.size() && i < 3; i++) {
        Cible* t = c[i].second; if (t == evite && evite_t > 0) continue;
        if (E::visible(oe, {t->pos.x, t->pos.y + t->h * .6f, t->pos.z})) { if (prev != t && react <= 0) react = rnd(.25f, .6f); cible = t; vu = true; memoire_t = 2.5f; break; }
    }
    if (!cible && prev && prev->vivant && memoire_t > 0 && !(prev == evite && evite_t > 0) && !G::allies(this, prev)) cible = prev;   // on garde la piste quelques secondes
    if (!cible) choisir_destination(); else actions_combat();
}
void PiloteIA::choisir_destination() {
    if (a_dest && bloque < 1.2f && !route.empty()) return;
    bloque = 0; V3 obj;
    if (G::partie && G::partie->objectif_ia(this, obj)) {
        if (hyp(obj.x - pos.x, obj.z - pos.z) < G::partie->rayon_zone() * .6f) { a_dest = false; route.clear(); return; }
        dest = obj; a_dest = true; route = Nav::route(pos, dest, dest.y); return;
    }
    int besoin = -1;
    if (armes.size() < 2 || armes[1]->melee) besoin = L_ARME;
    if (besoin < 0 && hp < hp_max * .55f) besoin = L_SOIN;
    else if (besoin < 0 && arme()->reserve >= 0 && !arme()->melee && arme()->reserve + arme()->munitions < arme()->chargeur) besoin = L_MUNITIONS;
    Loot* best = nullptr; float bd = besoin >= 0 ? 220.0f : 130.0f;
    for (Loot* l : E::loots) {
        if (!l->actif || (besoin >= 0 && l->type != besoin)) continue;
        if (!Nav::joignable(l->pos.x, l->pos.z, l->pos.y)) continue;
        float d = hyp(l->pos.x - pos.x, l->pos.z - pos.z); if (d < bd) { best = l; bd = d; }
    }
    V3 d;
    if (best && (besoin >= 0 || rnd01() < .75)) d = best->pos; else d = E::point_libre(1.0f);
    dest = d; a_dest = true; route = Nav::route(pos, d, d.y);
}
void PiloteIA::actions_combat() {
    Cible* c = cible; float d = hyp(c->pos.x - pos.x, c->pos.z - pos.z); bool est_pilote = c->kind == 0;
    if (est_pilote && d < 55 && score >= COUT_TITAN && !titan_actif() && titan_cd <= 0 && rnd01() < .35) appeler_titan();
    if (nb_tactique > 0 && 8 < d && d < 28 && rnd01() < .15) {
        V3 dn = norm({c->pos.x - pos.x, 0, c->pos.z - pos.z}); float v = std::min(26.0f, d * 1.6f); lancer_tactique(oeil(), dn * v + V3{0, 6, 0});
    }
}
void PiloteIA::bouger(float dt) {
    if (cible) {                                                  // garde-fou : on ne se rapproche pas d'une cible qu'on devrait rejoindre -> on l'ignore un moment
        float di = hyp(cible->pos.x - pos.x, cible->pos.z - pos.z); bool approche = di > arme()->pref * 1.25f;
        if (approche) { cw_t += dt; if (cw_d > 1e8f) cw_d = di; if (cw_t >= 3.5f) { if (di > cw_d - 1.0f) { evite = cible; evite_t = 5; cible = nullptr; vu = false; route.clear(); a_dest = false; } cw_t = 0; cw_d = di; } }
        else { cw_t = 0; cw_d = 1e9f; }
    } else { cw_t = 0; cw_d = 1e9f; }
    Cible* c = cible; V3 mv{0, 0, 0}; bool face = false; V3 fdir{0, 0, 1};
    if (c) {
        V3 d = {c->pos.x - pos.x, 0, c->pos.z - pos.z}; float di = len(d);
        if (di > .01f) { V3 dn = d * (1 / di); face = true; fdir = dn; float pref = arme()->pref; float fwd = di > pref * 1.25f ? 1.0f : (di < pref * .55f ? -.7f : 0.0f);
            mv = dn * fwd + V3{dn.z, 0, -dn.x} * (strafe * .7f); }
    } else if (!route.empty()) {
        V3 wp = route[0], d = {wp.x - pos.x, 0, wp.z - pos.z}; float di = len(d);
        if (di < (wp.y > .03f ? .6f : 1.3f)) { route.erase(route.begin()); if (route.empty()) a_dest = false; } else { mv = d * (1 / di); face = true; fdir = mv; }
    }
    marche = false;
    if (len(mv) > 0) {
        bool moved = G::marcher(this, mv.x, mv.z, 6.5f * vitesse_mult, dt); bloque = moved ? 0 : bloque + dt; marche = moved;
        if (bloque > 1.2f || G::watchdog(this, dt, !c && (a_dest || !route.empty()), hyp(dest.x - pos.x, dest.z - pos.z))) { a_dest = false; route.clear(); strafe = -strafe; detour_t = 0; bloque = 0; }
    } else G::marcher(this, 0, 0, 0, dt);
    if (face) yaw = deg(std::atan2(fdir.x, fdir.z));
}
void PiloteIA::tirer_sur_cible(float) {
    Arme* a = arme();
    if (!a->melee && a->munitions <= 0 && a->reserve == 0 && armes.size() > 1) { idx = 1 - idx; return; }
    if (a->munitions <= 0 && !a->melee) a->recharger();
    Cible* c = cible; if (!c || !vu || react > 0) return;
    V3 oe = oeil(), cp = {c->pos.x, c->pos.y + c->h * .6f, c->pos.z}; float d = dist(oe, cp);
    if (d > a->portee * .85f || !a->pret() || (a->munitions <= 0 && !a->melee)) return;
    float err = .03f + d * .0012f; float jx = rnd(-1, 1), jy = rnd(-.6f, .6f), jz = rnd(-1, 1); V3 jit = V3{jx, jy, jz} * (d * err);
    V3 fw = norm({cp.x - oe.x, 0, cp.z - oe.z}); V3 debut = oe + fw * .6f; a->tirer(this, oe, cp - oe + jit, &debut);
}
void PiloteIA::update(float dt) {
    Partie* P = G::partie; if (!P->actif() || !vivant) return;
    tick(dt); react -= dt; flash -= dt; strafe_t -= dt; memoire_t -= dt; evite_t -= dt;
    if (strafe_t <= 0) { strafe_t = rnd(1.2f, 2.5f); strafe = (float)RG.choice<int>({-1, 1}); }
    regen_mun += dt; if (regen_mun > 25) { regen_mun = 0; for (auto& a : armes) a->ajouter_munitions(a->chargeur); }
    pense -= dt; if (pense <= 0) { pense = rnd(.3f, .5f); penser(); }
    if (cible && !cible->vivant) { cible = nullptr; vu = false; }
    bouger(dt);
    if (marche) phase += dt * 10; float amp = marche ? 38.0f : 0.0f;
    float k = std::min(1.0f, 12 * dt);
    m.a_jambe[0] = lerpf(m.a_jambe[0], std::sin(phase) * amp, k); m.a_jambe[1] = lerpf(m.a_jambe[1], -std::sin(phase) * amp, k);
    m.a_bras[0] = lerpf(m.a_bras[0], std::sin(phase) * amp * .6f - 30, k);
    tirer_sur_cible(dt);
}
void PiloteIA::afficher() {
    if (!vivant) return;
    humano_dessiner(m, pos, yaw, 1, flash > 0 ? 1.6f : 0);
    BotArme b = bot_arme(arme()->modele);
    Matrix root = matTRS(pos, yaw, 1);
    Matrix mm = MatrixMultiply(MatrixMultiply(MatrixScale(b.sc.x, b.sc.y, b.sc.z), MatrixTranslate(.3f, 1.05f, .45f)), root);
    Piece& f = m.fusil; f.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = toC(b.col); ::dessiner(f, mm, 0, 0); f.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
}

// ====================================================================== Zombies
std::vector<int> tailles_groupes(int total, int tmin, int tmax) {
    int kmin = (total + tmax - 1) / tmax, kmax = total / tmin; int k = RG.randint(kmin, kmax);
    std::vector<int> t(k, tmin); int reste = total - tmin * k;
    while (reste > 0) { int i = (int)RG.randbelow(k); if (t[i] < tmax) { t[i]++; reste--; } }
    return t;
}
Zombie::Zombie(ZType ty, V3 p) : type(ty) {
    kind = 1; DEGATS = 7; DETECTION = 32; DELAI = 1.0f; ANIM = 5; TAILLE = 1; HP = 40; MULTHP = 1; VIT = 2; MULTVIT = 1;
    ctete = hsv(85, .3f, .5f); cyeux = {.9f, .8f, .2f, 1};
    if (ty == ZJOUR) { MULTHP = 1.5f; MULTVIT = .8f; }
    else if (ty == ZSPECIAL) { MULTHP = 2; MULTVIT = 1.5f; DEGATS = 12; DELAI = .9f; ANIM = 7; ecoute = true; ctete = hsv(30, .5f, .5f); cyeux = {1, .45f, .1f, 1}; }
    else { HP = 70; VIT = 4.6f; DEGATS = 14; DETECTION = 75; DELAI = .7f; ANIM = 9; ctete = hsv(8, .5f, .42f); cyeux = {1, .1f, .05f, 1};
        if (ty == ZRAPACE) { MULTHP = 1.7f; ecoute = true; ctete = hsv(285, .55f, .4f); cyeux = {.8f, .2f, 1, 1}; }
        if (ty == ZGEANT) { MULTHP = 2.2f; MULTVIT = .8f; DEGATS = 38; DETECTION = 90; DELAI = 1.2f; ANIM = 6; TAILLE = 1.8f; geant = true; ctete = hsv(0, .65f, .3f); cyeux = {1, .55f, 0, 1}; } }
    pos = {p.x, 0, p.z}; r = .45f * TAILLE; h = 1.9f * TAILLE; hp = hp_max = HP * MULTHP;
    var = rnd(.85f, 1.15f); pense = rnd(0, .4f); anim = rnd(0, 6);
    float th = (float)RG.choice<int>({0, 25, 50, 90, 150, 200, 240, 300}); float vp = (float)RG.choice<int>({75, 90, 105});
    humano_construire(mm, hsv(th, .25f, .3f), hsv(220, .2f, .2f), hsv(vp, .3f, .45f), false);
    mm.couleur_tete = ctete; mm.couleur_yeux = cyeux; groupe = nullptr;
}
void Zombie::alerter(V3 p, bool propage) {
    a_alerte = true; alerte_pos = p; alerte_t = 12;
    if (propage && groupe) for (Zombie* g : groupe->m) if (g != this && g->vivant) g->alerter(p, false);
}
Cible* Zombie::trouver_cible() {
    Cible* best = nullptr; float bd = DETECTION + bonus_detection;
    for (Pilote* p : G::pilotes) if (p->vivant && !p->elimine) { float d = hyp(p->pos.x - pos.x, p->pos.z - pos.z); if (d < bd) { best = p; bd = d; } }
    if (!best && groupe) for (Zombie* g : groupe->m) if (g != this && g->vivant && g->cible && g->cible->vivant) return g->cible;
    return best;
}
bool Zombie::avancer(float dx, float dz, float dt) { return G::marcher(this, dx, dz, vitesse(), dt); }
void Zombie::enquete(float dt) {
    V3 p = alerte_pos; alerte_t -= dt;
    if (alerte_t <= 0 || hyp(p.x - pos.x, p.z - pos.z) < 2.5f) { a_alerte = false; alerte_t = 0; route.clear(); return; }
    rt -= dt; if (bloque > .8f && rt <= 0) { rt = 2.5f; bloque = 0; route = Nav::route(pos, p, p.y); }
    aller_vers(!route.empty() ? route[0] : p, dt);
}
void Zombie::aller_vers(V3 but, float dt) {
    V3 d = {but.x - pos.x, 0, but.z - pos.z}; float di = len(d);
    if (!route.empty() && di < (but.y > .03f ? .6f : 1.4f)) route.erase(route.begin());
    else if (di > .01f) { V3 dn = d * (1 / di); yaw = deg(std::atan2(dn.x, dn.z)); bool ok = avancer(dn.x, dn.z, dt); bloque = ok ? 0 : bloque + dt; }
}
void Zombie::tick(float dt, bool pr) {
    if (pr) { anim += dt * ANIM; float s = std::sin(anim) * 35; mm.a_jambe[0] = s; mm.a_jambe[1] = -s; float lunge = atk > .5f ? 14.0f : 0.0f;
        mm.a_bras[0] = -80 + std::sin(anim * .7f) * 8 + lunge; mm.a_bras[1] = -80 + std::sin(anim * .7f + 2) * 8 + lunge; mm.tete_rz = 8 + std::sin(anim * .5f) * 6; }
    proche = pr; if (flash > 0) flash -= dt; atk -= dt; pense -= dt;
    if (pense <= 0) { pense = rnd(.25f, .45f); cible = trouver_cible(); }
    Cible* c = cible; if (c && (!c->vivant || (c->kind == 0 && static_cast<Pilote*>(c)->elimine))) { c = cible = nullptr; }
    if (c) {
        a_alerte = false; alerte_t = 0; bool haut = c->pos.y - pos.y > 2.5f; rt -= dt;
        bool but_bouge = !route_valide || hyp(c->pos.x - route_but.x, c->pos.z - route_but.z) > 8 || std::fabs(c->pos.y - route_but.y) > 2.5f;
        bool coince = G::watchdog(this, dt, hyp(c->pos.x - pos.x, c->pos.z - pos.z) > 3.0f, hyp(c->pos.x - pos.x, c->pos.z - pos.z));
        if (((haut && (route.empty() || but_bouge)) || bloque > .8f || coince) && rt <= 0) { rt = 2.5f; bloque = 0; route = Nav::route(pos, c->pos, c->pos.y); route_but = c->pos; route_valide = true; if (coince) detour_t = 1.5f; }
        V3 but = !route.empty() ? route[0] : c->pos; float di = hyp(but.x - pos.x, but.z - pos.z), dc = hyp(c->pos.x - pos.x, c->pos.z - pos.z);
        if (di > .01f && (dc > 1.2f * TAILLE || !route.empty())) aller_vers(but, dt); else if (di > .01f) G::marcher(this, 0, 0, 0, dt);
        if (dc < 1.7f * TAILLE && std::fabs(c->pos.y - pos.y) < 2.2f * TAILLE && atk <= 0) { atk = DELAI; c->damage(DEGATS, this); }
    } else if (a_alerte) enquete(dt); else errer(dt);
}
void Zombie::errer(float dt) {
    route.clear(); Zombie* chef = (groupe && !groupe->m.empty()) ? groupe->m[0] : this;
    if (chef != this && chef->vivant) {
        float dx = chef->pos.x - pos.x, dz = chef->pos.z - pos.z, di = hyp(dx, dz);
        if (di > 5) { aller_vers(chef->pos, dt * .6f); return; }
        if (di < 2) { avancer(-dx, -dz, dt * .3f); return; }
        G::marcher(this, 0, 0, 0, dt); return;
    }
    dest_t -= dt;
    if (!a_dest || dest_t <= 0) { float a = rnd(0, 6.28f); dest = {std::sin(a), 0, std::cos(a)}; dest_t = rnd(2, 5); a_dest = true; }
    if (avancer(dest.x, dest.z, dt * .5f)) yaw = deg(std::atan2(dest.x, dest.z)); else a_dest = false;
}
void Zombie::damage(float n, Cible* src, bool) {
    if (!vivant) return;
    hp -= n; flash = .08f; FX::sang(pos + V3{0, 1.3f * TAILLE, 0}, 3);
    if (hp <= 0) {
        vivant = false;
        G::zombies.erase(std::remove(G::zombies.begin(), G::zombies.end(), this), G::zombies.end());
        if (groupe) groupe->m.erase(std::remove(groupe->m.begin(), groupe->m.end(), this), groupe->m.end());
        FX::sang(pos + V3{0, 1.2f * TAILLE, 0}, 10);
        G::partie->mort_zombie(this, src);
    }
}
std::vector<Zombie*> creer_groupe(ZType t, int n, V3 centre, float bonus) {
    Groupe* g = new Groupe(); std::vector<Zombie*> res;
    for (int i = 0; i < n; i++) {
        float x = centre.x, z = centre.z; bool place = false;
        for (int k = 0; k < 8; k++) {
            float px = centre.x + rnd(-3, 3), pz = centre.z + rnd(-3, 3);
            if (!E::touche(px, 0, pz, .6f, 2) && std::fabs(px) < DEMI_MAP - 3 && std::fabs(pz) < DEMI_MAP - 3 && Nav::sol_joignable(px, pz)) { x = px; z = pz; place = true; break; }
        }
        if (!place) { for (int k = 0; k < 40 && !place; k++) { float px = centre.x + rnd(-12, 12), pz = centre.z + rnd(-12, 12); if (!E::touche(px, 0, pz, .6f, 2) && std::fabs(px) < DEMI_MAP - 3 && std::fabs(pz) < DEMI_MAP - 3 && Nav::sol_joignable(px, pz)) { x = px; z = pz; place = true; } } }
        if (!place) continue;
        Zombie* zb = new Zombie(t, {x, 0, z}); zb->groupe = g; zb->bonus_detection = bonus; g->m.push_back(zb); res.push_back(zb); G::zombies.push_back(zb);
    }
    return res;
}
void generer_groupes(ZType t, int total, int tmin, int tmax, const std::function<V3()>& pos_fn) {
    for (int n : tailles_groupes(total, tmin, tmax)) creer_groupe(t, n, pos_fn());
}
std::vector<CompoZ> composition(bool nuit) {
    if (nuit) return {{ZNUIT, 45, 3, 4}, {ZRAPACE, 30, 2, 3}, {ZGEANT, 5, 1, 1}};
    return {{ZJOUR, 40, 3, 4}, {ZSPECIAL, 8, 2, 3}};
}
void gestion_zombies_update(float dt) {
    static int n = 0; Partie* P = G::partie; if (!P || !P->actif()) return; n++;
    std::vector<std::pair<float, float>> refs; for (auto p : G::pilotes) if (p->vivant && !p->elimine) refs.push_back({p->pos.x, p->pos.z});
    if (refs.empty()) refs.push_back({0, 0});
    std::vector<Zombie*> zs = G::zombies;
    for (size_t i = 0; i < zs.size(); i++) {
        Zombie* z = zs[i]; if (!z->vivant) continue; float d2 = 1e18f;
        for (auto& r : refs) d2 = std::min(d2, (z->pos.x - r.first) * (z->pos.x - r.first) + (z->pos.z - r.second) * (z->pos.z - r.second));
        int pas = d2 < 1600 ? 1 : (d2 < 4900 ? 2 : (d2 < 12100 ? 4 : 8));
        if ((n + (int)i) % pas == 0) z->tick(dt * pas, pas <= 2);
    }
}

// ====================================================================== Titan
static ArmeP titan_nouvelle_arme(Titan* t) { auto a = Armes::pour_titan(t->owner->arme_pour_titan()); a->cadence *= 1.8f; return a; }
Titan::Titan(Pilote* o, V3 sp) : owner(o), spot(sp) {
    kind = 2; pos = {sp.x, 70, sp.z}; r = 1.6f; h = 6; ia.arme = titan_nouvelle_arme(this); ia.epee = Armes::epee(); G::titans.push_back(this);
}
void Titan::ordonner(const std::string& o, const V3* p) { ordre = o; a_point = p != nullptr; if (p) point = *p; ia.route.clear(); ia.bloque = 0; ia.a_but = false; }
void Titan::embarquer() { state = 2; }
void Titan::debarquer(V3 p, float h_) { hp = h_; pos = {p.x, 0, p.z}; state = 1; ordre = "suivre"; }
void Titan::damage(float n, Cible* src, bool) { if (state != 1 || !vivant) return; if (src && src != this && G::allies(src, this)) return; hp -= n * .7f; if (hp <= 0) detruire(true, src); }
void Titan::detruire(bool explose, Cible* tueur) {
    if (!vivant) return; vivant = false; G::titans.erase(std::remove(G::titans.begin(), G::titans.end(), this), G::titans.end());
    if (tueur && G::partie) G::partie->mort_titan(this, tueur);
    if (explose) { FX::explosion(pos + V3{0, 3, 0}, 16, {1, .55f, .05f, 1}); G::explode(pos, 10, 250, this, owner); }
    if (owner->titan == this) { owner->titan = nullptr; owner->titan_cd = 25; }
}
static bool t_bouger(Titan* t, float dx, float dz, float v, float dt) {
    float nx = t->pos.x + dx * v * dt, nz = t->pos.z + dz * v * dt; bool ok = false;
    if (!E::touche(nx, .5f, t->pos.z, 1.6f, 5.5f)) { t->pos.x = nx; ok = true; } if (!E::touche(t->pos.x, .5f, nz, 1.6f, 5.5f)) { t->pos.z = nz; ok = true; } return ok;
}
static bool t_marcher(Titan* t, V3 but, float v, float dt) {
    TitanAuto& ia = t->ia;
    if (!ia.a_but || hyp(but.x - ia.but_prec.x, but.z - ia.but_prec.z) > 6) { ia.route.clear(); ia.bloque = 0; ia.a_but = true; ia.but_prec = {but.x, 0, but.z}; }
    if (hyp(but.x - t->pos.x, but.z - t->pos.z) < 2.5f) return true;
    ia.rt -= dt; if (ia.bloque > .8f && ia.rt <= 0) { ia.rt = 3; ia.bloque = 0; ia.route = Nav::vers_sol_gros(t->pos, but); }
    V3 wp = !ia.route.empty() ? ia.route[0] : but, d = {wp.x - t->pos.x, 0, wp.z - t->pos.z}; float di = len(d);
    if (!ia.route.empty() && di < 3) { ia.route.erase(ia.route.begin()); return false; }
    if (di > .01f) { V3 dn = d * (1 / di); t->yaw = deg(std::atan2(dn.x, dn.z)); ia.bloque = t_bouger(t, dn.x, dn.z, v, dt) ? 0 : ia.bloque + dt; }
    return false;
}
void Titan::update(float dt) {
    if (!G::partie->actif()) return;
    if (t_epee > 0) { t_epee -= dt; epee_a = lerpf(-75, 75, clampf(1 - t_epee / .45f, 0, 1) * 1.8f); }
    if (state == 0) {
        vy += 25 * dt; pos.y -= vy * dt;
        if (pos.y <= 0) { pos.y = 0; state = 1; FX::secousse(1.0f); FX::explosion(pos + V3{0, .5f, 0}, 16, {1, 1, 1, 1}); G::explode(pos, 9, 300, this, owner); }
        return;
    }
    if (state != 1) return;
    Pilote* o = owner; ia.arme->update(dt); ia.epee->update(dt); ia.pense -= dt;
    if (ia.pense <= 0) {
        ia.pense = .4f; if (ia.arme->modele != o->arme_pour_titan()->modele) ia.arme = titan_nouvelle_arme(this);
        Cible* best = nullptr; float bs = 1e9f; bool ag = ordre == "attaquer";
        for (Cible* c : G::toutes_cibles()) {
            if (c == this || c == o || G::allies(o, c)) continue; float d = hyp(c->pos.x - pos.x, c->pos.z - pos.z);
            float lim = ag ? 90.0f : (c->kind != 1 ? 55.0f : 40.0f); if (d > lim) continue;
            float score = d * (c->kind == 0 ? .7f : 1.0f);
            if (score < bs && (ag || E::visible(tete(), {c->pos.x, c->pos.y + c->h * .55f, c->pos.z}))) { best = c; bs = score; }
        }
        ia.cible = best;
    }
    Cible* c = ia.cible; if (c && !c->vivant) c = ia.cible = nullptr;
    float dc = c ? hyp(c->pos.x - pos.x, c->pos.z - pos.z) - c->r : 999;
    const float PF = 5.5f;
    if (ordre == "aller") { if (t_marcher(this, point, 6, dt)) ordonner("garder"); }
    else if (ordre == "attaquer") {
        if (c && dc > PF - 1.5f && (dc > 22 || !E::visible(tete(), {c->pos.x, c->pos.y + c->h * .55f, c->pos.z}))) t_marcher(this, {c->pos.x, 0, c->pos.z}, 6, dt);
        else if (!c && o->vivant && !o->elimine && hyp(o->pos.x - pos.x, o->pos.z - pos.z) > 14) t_marcher(this, {o->pos.x, 0, o->pos.z}, 6, dt);
    } else if (ordre == "suivre") {
        if (c) { if (dc > 32) t_marcher(this, {c->pos.x, 0, c->pos.z}, 5, dt); }
        else if (o->vivant && !o->elimine && hyp(o->pos.x - pos.x, o->pos.z - pos.z) > 14) t_marcher(this, {o->pos.x, 0, o->pos.z}, 6, dt);
    }
    if (!c) return;
    V3 cp = {c->pos.x, c->pos.y + c->h * .55f, c->pos.z};
    if (ordre != "aller" || dc < 14) { V3 d = {c->pos.x - pos.x, 0, c->pos.z - pos.z}; if (len(d) > .1f) yaw = deg(std::atan2(d.x, d.z)); }
    if (dc <= PF) {
        if (ia.epee->pret()) { ia.epee->cool = ia.epee->cadence; balancer(); V3 dn = {c->pos.x - pos.x, 0, c->pos.z - pos.z}; int tt; G::melee_coup(this, {pos.x, pos.y + 3, pos.z}, dn, ia.epee.get(), tt); }
    } else if (ia.arme->pret()) {
        V3 org = tete();
        if (E::visible(org, cp)) {
            float di = dist(org, cp); float a = rnd(-1, 1), b = rnd(-.5f, .5f), cc = rnd(-1, 1); V3 jit = V3{a, b, cc} * (di * .03f);
            V3 dn = norm({cp.x - org.x, 0, cp.z - org.z}); V3 debut = org + dn * 2; ia.arme->tirer(this, org, cp - org + jit, &debut);
        }
    }
}
static Piece* titan_piece = nullptr, *epee_piece = nullptr;
void titan_dessiner(Titan* t) {
    if (!titan_piece) {
        Col c = hsv(210, .15f, .32f), c2 = hsv(210, .1f, .2f), ora = hsv(20, .9f, .9f);
        std::vector<Cube> parts = {{{1.3f, 3, 1.4f}, {-.95f, 1.5f, 0}, c}, {{1.3f, 3, 1.4f}, {.95f, 1.5f, 0}, c}, {{1.5f, .6f, 2.1f}, {-.95f, .3f, .3f}, c2}, {{1.5f, .6f, 2.1f}, {.95f, .3f, .3f}, c2},
            {{2.8f, 1.2f, 1.8f}, {0, 3.4f, 0}, c2}, {{3.6f, 2.8f, 2.3f}, {0, 5.2f, 0}, c}, {{1.5f, 1, 2.6f}, {-2.4f, 6.2f, 0}, c2}, {{1.5f, 1, 2.6f}, {2.4f, 6.2f, 0}, c2},
            {{1.1f, 1, 1.1f}, {0, 7, .2f}, c2}, {{.95f, .28f, .3f}, {0, 7.05f, .8f}, ora}, {{.9f, .9f, 3.4f}, {2.4f, 4.8f, 1.4f}, hsv(210, .1f, .15f)},
            {{1, 2.4f, 1}, {-2.5f, 4.6f, .3f}, c}, {{1.6f, 2.2f, .3f}, {-2.5f, 4.4f, 1}, ora}, {{1, .35f, 1}, {0, 5.2f, -1.3f}, ora}};
        titan_piece = new Piece(baker(parts));
        epee_piece = new Piece(baker({{{.3f, .22f, 6.2f}, {0, 0, 3.6f}, hsv(200, .1f, .95f)}, {{1.6f, .3f, .4f}, {0, 0, .9f}, ora}}));
    }
    if (t->state == 2) return;
    dessiner(*titan_piece, matTRS(t->pos, t->yaw, 1), 0, 0);
    if (t->t_epee > 0) { Matrix m = MatrixMultiply(MatrixRotateY(rad(t->epee_a)), MatrixTranslate(0, 5.6f, .2f)); dessiner(*epee_piece, MatrixMultiply(m, matTRS(t->pos, t->yaw, 1)), 0, 0); }
    if (t->state == 0) {
        rlDisableDepthMask(); DrawCube({t->pos.x, t->pos.y + 5, t->pos.z}, 1.6f, 8, 1.6f, {255, 140, 25, 150});
        DrawCylinder({t->spot.x, .06f, t->spot.z}, 3.5f, 3.5f, .02f, 20, {255, 100, 0, 150}); rlEnableDepthMask();
    } else if (t->ordre == "aller" && t->a_point) DrawCylinder({t->point.x, .2f, t->point.z}, 2.5f, 2.5f, .02f, 20, {255, 150, 25, 140});
}
