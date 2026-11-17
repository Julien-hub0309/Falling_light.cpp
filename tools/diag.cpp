// Outil de diagnostic (hors build normal) : simule une partie sans affichage et détecte
// les entités incrustées dans la géométrie ou coincées.
#include "partie.hpp"
void ui_auto(const std::string&, bool, int);
struct Suivi { V3 p0{0, 0, 0}; float t = 0; int coince = 0; };
static std::map<Cible*, Suivi> suivi; static Cible* trace_c = nullptr; static Zombie* trace_z = nullptr; static int trace_n = 0;
static void boite_proche(V3 p) {
    for (auto& b : E::boxes) if (p.x > b.x0 - 2.5f && p.x < b.x1 + 2.5f && p.z > b.z0 - 2.5f && p.z < b.z1 + 2.5f && b.y1 > p.y - .5f && b.y0 < p.y + 2)
        printf("      box x[%.1f %.1f] z[%.1f %.1f] y[%.2f %.2f]\n", b.x0, b.x1, b.z0, b.z1, b.y0, b.y1);
}
int diag_run(const std::string& cfg, int frames) {
    size_t p1 = cfg.find(':'); ui_auto(cfg.substr(0, p1), cfg.substr(p1 + 1) == "nuit", 0);
    Partie* P = G::partie; Player* J = G::joueur; float dt = 1.0f / 30; int incruste = 0, coince = 0, tombe = 0;
    std::map<std::string, int> deja;
    for (int f = 0; f < frames; f++) {
        J->invuln = 999; J->hp = J->hp_max; if (auto pp = dynamic_cast<PartieParcours*>(P)) pp->reste = 999; if (P->fini) break; P->update(dt);
        for (auto p : G::pilotes) if (auto ia = dynamic_cast<PiloteIA*>(p)) { if (!getenv("FL_NOCHEAT")) ia->hp = std::max(ia->hp, 60.0f); ia->update(dt); }
        for (auto t : std::vector<Titan*>(G::titans)) t->update(dt);
        gestion_zombies_update(dt); G::update_grenades(dt);
        if (trace_c && f % 15 == 0 && trace_n < 40) { trace_n++; Cible* c = trace_c; if (auto ia = dynamic_cast<PiloteIA*>(c)) printf("B %.1fs pos(%.1f %.2f %.1f) viv=%d dest(%.1f %.1f %.1f) a_dest=%d route=%zu wp=(%.1f %.2f %.1f) bloque=%.2f detour=%.2f cible=%d\n", P->t, ia->pos.x, ia->pos.y, ia->pos.z, ia->vivant, ia->dest.x, ia->dest.y, ia->dest.z, ia->a_dest, ia->route.size(), ia->route.empty() ? 0.f : ia->route[0].x, ia->route.empty() ? 0.f : ia->route[0].y, ia->route.empty() ? 0.f : ia->route[0].z, ia->bloque, ia->detour_t, ia->cible != nullptr); }
        if (trace_z && f % 15 == 0 && trace_n < 24) { Zombie* z = trace_z; trace_n++; printf("T %.1fs pos(%.1f %.2f %.1f) viv=%d cible=%s (%.1f %.1f) route=%zu wp=(%.1f %.1f) bloque=%.2f detour=%.2f rt=%.2f proche=%d\n", P->t, z->pos.x, z->pos.y, z->pos.z, z->vivant, z->cible ? (z->cible->kind ? "z" : "P") : "-", z->cible ? z->cible->pos.x : 0.f, z->cible ? z->cible->pos.z : 0.f, z->route.size(), z->route.empty() ? 0.f : z->route[0].x, z->route.empty() ? 0.f : z->route[0].z, z->bloque, z->detour_t, z->rt, z->proche); }
        if (f % 30) continue;
        std::vector<Cible*> tous; for (auto p : G::pilotes) if (p != J && p->vivant) tous.push_back(p); for (auto z : G::zombies) tous.push_back(z);
        for (Cible* c : tous) {
            V3 p = c->pos; char k[64]; snprintf(k, sizeof k, "%d,%d", (int)(p.x / 2), (int)(p.z / 2));
            if (E::touche(p.x, p.y + .25f, p.z, c->r * .55f, c->h - .5f) && !deja.count(std::string("i") + k)) {
                deja[std::string("i") + k] = 1; incruste++; printf("INCRUSTE %s kind=%d at (%.1f %.1f %.1f) t=%.0fs\n", c->kind == 0 ? "bot" : "zombie", c->kind, p.x, p.y, p.z, P->t); boite_proche(p);
            }
            if (p.y < -.5f || std::fabs(p.x) > DEMI_MAP + 1 || std::fabs(p.z) > DEMI_MAP + 1) { tombe++; printf("HORS-LIMITES kind=%d (%.1f %.1f %.1f)\n", c->kind, p.x, p.y, p.z); }
            Suivi& s = suivi[c]; s.t += 1;
            if (s.t >= 8) {
                float d = hyp(p.x - s.p0.x, p.z - s.p0.z); bool veut = false; V3 dest{0, 0, 0};
                if (auto ia = dynamic_cast<PiloteIA*>(c)) { veut = !ia->cible && (ia->a_dest || !ia->route.empty()); dest = ia->dest; }
                else if (auto z = dynamic_cast<Zombie*>(c)) { veut = z->cible && hyp(z->cible->pos.x - p.x, z->cible->pos.z - p.z) > 4; if (veut) dest = z->cible->pos; }
                if (!(veut && d < 1.2f)) s.coince = 0; else s.coince++; if (s.coince >= 2 && !deja.count(std::string("s") + k)) { deja[std::string("s") + k] = 1; coince++; if (!trace_c && c->kind == 0) { trace_c = c; trace_n = 0; } if (auto zz = dynamic_cast<Zombie*>(c)) if (!trace_z && zz->pos.y < .1f) { trace_z = zz; } printf("COINCE %s at (%.1f %.1f %.1f) -> dest (%.1f %.1f %.1f)\n", c->kind == 0 ? "bot" : "zombie", p.x, p.y, p.z, dest.x, dest.y, dest.z); boite_proche(p); if (c->kind == 0) { for (auto q : G::pilotes) if (auto ia = dynamic_cast<PiloteIA*>(q)) if (ia->vivant) printf("      IA %-8s pos(%.1f %.1f %.1f) a_dest=%d dest(%.1f %.1f) route=%zu cible=%d bloque=%.1f\n", ia->nom.c_str(), ia->pos.x, ia->pos.y, ia->pos.z, ia->a_dest, ia->dest.x, ia->dest.z, ia->route.size(), ia->cible ? ia->cible->kind + 1 : 0, ia->bloque); } if (auto z = dynamic_cast<Zombie*>(c)) printf("      zombie: route=%zu bloque=%.2f rt=%.2f y=%.2f wp=(%.1f %.1f)\n", z->route.size(), z->bloque, z->rt, z->pos.y, z->route.empty() ? 0.f : z->route[0].x, z->route.empty() ? 0.f : z->route[0].z); }
                s.p0 = p; s.t = 0;
            }
        }
    }
    if (auto dm = dynamic_cast<PartieDM*>(P)) {
        printf("DM t=%.0fs fini=%d resultat=[%s]\n", P->t, (int)P->fini, P->resultat.c_str());
        if (dm->par_equipe) printf("DM equipes bleu=%d orange=%d\n", dm->total_equipe(1), dm->total_equipe(2));
        printf("DM stats : pilotes tues=%d (%d pts)  titans=%d (%d pts)  zombies=%d (%d pts)\n", dm->stat_pilotes, dm->stat_pilotes * PTS_PILOTE, dm->stat_titans, dm->stat_titans * PTS_TITAN, dm->stat_zombies, dm->stat_zombies * PTS_ZOMBIE);
        int reserve_min = 99999, infinies = 0, total = 0; for (auto q : dm->pilotes_actifs()) { if (q->armes.size() > 1 && !q->armes[1]->melee) { total++; if (q->armes[1]->reserve < 0) infinies++; else reserve_min = std::min(reserve_min, q->armes[1]->reserve); } }
        printf("DM munitions : %d/%d armes de classe en reserve infinie (reserve_min=%d)\n", infinies, total, reserve_min);
        int vivants = 0, kills = 0; for (auto q : dm->pilotes_actifs()) { if (q->vivant) vivants++; kills += q->kills; }
        printf("DM pilotes=%zu vivants=%d kills_total=%d\n", dm->pilotes_actifs().size(), vivants, kills);
        for (auto q : dm->classement()) printf("   %-9s eq=%d pts=%3d kills=%2d classe=%s\n", q->nom.c_str(), q->equipe, q->pts, q->kills, q->classe ? q->classe->nom : "?");
    }
    printf("RESULTAT %s : incrustes=%d coinces=%d hors-limites=%d (bots vivants %d, zombies %zu)\n", cfg.c_str(), incruste, coince, tombe, (int)std::count_if(G::pilotes.begin(), G::pilotes.end(), [](Pilote* p) { return p->vivant; }), G::zombies.size());
    return 0;
}
