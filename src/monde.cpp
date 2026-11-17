// FALLING LIGHT - physique du monde + génération de la carte (port fidèle de partie/etat.py, map/*.py)
#include "etat.hpp"
#include <set>
#include <array>

namespace E {
std::vector<Box> boxes; std::vector<AccesToit> acces_toit; std::vector<V3> spawns; std::vector<FumeeData> fumees; std::vector<Loot*> loots;
static constexpr int GN = 56, GO = 28;
static std::vector<int> grille[GN][GN];
static std::vector<int> stamp; static int stampN = 0;
static int ci(float v) { return (int)std::floor(v / CELL); }
static std::vector<int>& cell(int i, int j) { return grille[std::clamp(i + GO, 0, GN - 1)][std::clamp(j + GO, 0, GN - 1)]; }

void init_grille() { boxes.clear(); for (auto& r : grille) for (auto& c : r) c.clear(); acces_toit.clear(); }
void ajouter_box(float x0, float x1, float z0, float z1, float y0, float y1) {
    int id = (int)boxes.size(); boxes.push_back({x0, x1, z0, z1, y0, y1});
    for (int i = ci(x0); i <= ci(x1); i++) for (int j = ci(z0); j <= ci(z1); j++) cell(i, j).push_back(id);
}
float hits(float x, float y, float z, float r, float h) {
    float best = HNONE; float xm = x - r, xp = x + r, zm = z - r, zp = z + r, yh = y + h;
    for (int i = ci(xm); i <= ci(xp); i++) for (int j = ci(zm); j <= ci(zp); j++)
        for (int id : cell(i, j)) {
            const Box& b = boxes[id];
            if (xp > b.x0 && xm < b.x1 && zp > b.z0 && zm < b.z1 && y < b.y1 && yh > b.y0 && b.y1 > best) best = b.y1;
        }
    return best;
}
static float rayBox(V3 o, V3 d, const Box& b) {
    float tmin = 0, tmax = 1e9f;
    float oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z}, lo[3] = {b.x0, b.y0, b.z0}, hi[3] = {b.x1, b.y1, b.z1};
    for (int k = 0; k < 3; k++) {
        if (std::fabs(dd[k]) < 1e-9f) { if (oo[k] < lo[k] || oo[k] > hi[k]) return -1; }
        else {
            float t1 = (lo[k] - oo[k]) / dd[k], t2 = (hi[k] - oo[k]) / dd[k]; if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1); tmax = std::min(tmax, t2); if (tmin > tmax) return -1;
        }
    }
    return tmin;
}
float ray_world(V3 o, V3 d, float portee) {
    float best = portee;
    if (d.y < -1e-6f) { float t = -o.y / d.y; if (t > 0 && t < best) best = t; }
    const float inf = 1e18f;
    int i = ci(o.x), j = ci(o.z), si = d.x > 0 ? 1 : -1, sj = d.z > 0 ? 1 : -1;
    float tdx = std::fabs(d.x) > 1e-9f ? CELL / std::fabs(d.x) : inf, tdz = std::fabs(d.z) > 1e-9f ? CELL / std::fabs(d.z) : inf;
    float tmx = std::fabs(d.x) > 1e-9f ? (((i + (si > 0 ? 1 : 0)) * CELL - o.x) / d.x) : inf;
    float tmz = std::fabs(d.z) > 1e-9f ? (((j + (sj > 0 ? 1 : 0)) * CELL - o.z) / d.z) : inf;
    if ((int)stamp.size() < (int)boxes.size()) stamp.resize(boxes.size(), 0);
    stampN++; float t = 0;
    for (int guard = 0; t <= best && guard < 200; guard++) {
        for (int id : cell(i, j)) {
            if (stamp[id] == stampN) continue; stamp[id] = stampN;
            float tt = rayBox(o, d, boxes[id]); if (tt >= 0 && tt < best) best = tt;
        }
        if (tmx >= inf && tmz >= inf) break;
        if (tmx < tmz) { t = tmx; i += si; tmx += tdx; } else { t = tmz; j += sj; tmz += tdz; }
    }
    return best;
}
float ray_cyl(V3 o, V3 d, float cx, float cz, float cy, float r, float h) {
    float ox = o.x - cx, oz = o.z - cz, a = d.x * d.x + d.z * d.z;
    if (a < 1e-8f) return -1;
    float b = 2 * (ox * d.x + oz * d.z), c = ox * ox + oz * oz - r * r, disc = b * b - 4 * a * c;
    if (disc < 0) return -1;
    float s = std::sqrt(disc), t = (-b - s) / (2 * a);
    if (t < 0) { t = (-b + s) / (2 * a); if (t < 0) return -1; }
    float y = o.y + d.y * t;
    return (y >= cy && y <= cy + h) ? t : -1;
}
static float distSeg(V3 a, V3 b, V3 c) {
    V3 ab = b - a; float l2 = dot(ab, ab); if (l2 < 1e-9f) return len(c - a);
    float t = clampf(dot(c - a, ab) / l2, 0, 1); return len(c - (a + ab * t));
}
bool visible(V3 a, V3 b) {
    V3 d = b - a; float l = len(d); if (l < .1f) return true;
    d = d * (1.0f / l);
    if (ray_world(a, d, l) < l - .3f) return false;
    for (auto& f : fumees) if (distSeg(a, b, f.pos) < f.rayon * .9f) return false;
    return true;
}
bool libre(V3 a, V3 b, float hauteur) {
    V3 d = {b.x - a.x, 0, b.z - a.z}; float l = len(d); if (l < .1f) return true; d = d * (1.0f / l);
    for (float hh : {hauteur, 1.5f}) { V3 o = {a.x, a.y + hh, a.z}; if (ray_world(o, d, l) < l - .3f) return false; }
    return true;
}
int box_sous(float x, float y, float z) {
    for (int id : cell(ci(x), ci(z))) { const Box& b = boxes[id]; if (std::fabs(b.y1 - y) < .6f && x > b.x0 && x < b.x1 && z > b.z0 && z < b.z1) return id; }
    return -1;
}
float hauteur_sol(float x, float z, float r, float ymax) {
    float best = 0; float xm = x - r, xp = x + r, zm = z - r, zp = z + r;
    for (int i = ci(xm); i <= ci(xp); i++) for (int j = ci(zm); j <= ci(zp); j++)
        for (int id : cell(i, j)) {
            const Box& b = boxes[id]; float y1 = b.y1;
            if (best < y1 && y1 <= ymax && xp > b.x0 && xm < b.x1 && zp > b.z0 && zm < b.z1) best = y1;
        }
    return best;
}
V3 point_libre(float rayon, const std::vector<V3>& loin, float dmin, int essais) {
    for (int k = 0; k < essais; k++) {
        float x = rnd(-DEMI_MAP + 6, DEMI_MAP - 6), z = rnd(-DEMI_MAP + 6, DEMI_MAP - 6);
        if (touche(x, 0, z, rayon, 3)) continue;
        bool ok = true; for (auto& p : loin) if (hyp(x - p.x, z - p.z) < dmin) { ok = false; break; }
        if (ok) return {x, 0, z};
    }
    return {0, 0, 0};
}
Loot* nouveau_loot(V3 pos, int type, bool temporaire) {
    Loot* l = new Loot(); l->pos = pos;
    if (type < 0) type = RG.choices({5, 5, 3, 2, 3});             // soin, munitions, armure, grenade, arme
    l->type = (LootType)type; l->temporaire = temporaire; l->t = rnd(0, 6);
    loots.push_back(l); return l;
}

// ====================================================================== carte (map/elements.py + map/map.py)
static PyRandom* R_ = nullptr;
static const float ROUTES[5] = {-120, -60, 0, 60, 120};
static const float CENTRES[6] = {-150, -90, -30, 30, 90, 150};
static Batch& B(const char* n) { return W.b.at(n); }
static void init_batches() {
    W.clear();
    W.batch("beton", "facade", 13.2f, 13.2f); W.batch("brique", "brique", 13.2f, 13.2f); W.batch("verre", "verre", 13.2f, 13.2f);
    W.batch("toit", "toit", 4, 4); W.batch("metal", "metal", 2, 2); W.batch("bois", "bois", 2, 2); W.batch("sol", "sol", 8, 8);
    W.batch("route", "route", 12, 16); W.batch("mur", "mur", 16, 16); W.batch("trottoir", "trottoir", 6, 6); W.batch("herbe", "herbe", 6, 6);
    W.batch("blanc", "", 4, 4); W.batch("lumiere", "", 4, 4, true);
}
static void bloc(Batch& bat, float x0, float x1, float y0, float y1, float z0, float z1, Col col, Batch* haut = nullptr, const Col* ch = nullptr, bool coll = true) {
    bat.box(x0, x1, y0, y1, z0, z1, col, haut, false, ch);
    if (coll) ajouter_box(x0, x1, z0, z1, y0, y1);
}
struct Facade {
    char side; float x0, z0, w, d, L;
    Facade(char s, float x0_, float z0_, float w_, float d_) : side(s), x0(x0_), z0(z0_), w(w_), d(d_), L((s == 'N' || s == 'S') ? w_ : d_) {}
    void xz(float u, float v, float& x, float& z) const {
        if (side == 'S') { x = x0 + u; z = z0 - v; } else if (side == 'N') { x = x0 + u; z = z0 + d + v; }
        else if (side == 'W') { x = x0 - v; z = z0 + u; } else { x = x0 + w + v; z = z0 + u; }
    }
    void box(Batch& bat, float ua, float ub, float va, float vb, float y0, float y1, Col col, Batch* haut = nullptr, bool coll = true) const {
        float xa, za, xb, zb; xz(ua, va, xa, za); xz(ub, vb, xb, zb);
        bloc(bat, std::min(xa, xb), std::max(xa, xb), y0, y1, std::min(za, zb), std::max(za, zb), col, haut, nullptr, coll);
    }
    V3 pt(float u, float v, float y = 0) const { float x, z; xz(u, v, x, z); return {x, y, z}; }
};
static void parapet(float x0, float z0, float w, float d, float h) {
    Col col = gris(.5f); float t = .35f, ph = .45f; Batch& toit = B("toit");
    bloc(toit, x0, x0 + w, h, h + ph, z0, z0 + t, col); bloc(toit, x0, x0 + w, h, h + ph, z0 + d - t, z0 + d, col);
    bloc(toit, x0, x0 + t, h, h + ph, z0 + t, z0 + d - t, col); bloc(toit, x0 + w - t, x0 + w, h, h + ph, z0 + t, z0 + d - t, col);
}
static float immeuble(float x0, float z0, float w, float d, int e, const std::string& style, PyRandom& rng, float y_base = 0) {
    float h = y_base + e * FL; Batch& bat = B(style.c_str()); Col tint;
    if (style == "beton") { float hh = (float)rng.choice<int>({30, 40, 200, 215, 0}); float s = rng.uniform(.03f, .12f); float v = rng.uniform(.82f, 1); tint = hsv(hh, s, v); }
    else tint = gris(rng.uniform(.88f, 1));
    bloc(bat, x0, x0 + w, y_base, h - .4f, z0, z0 + d, tint, &B("toit"), nullptr, false);
    ajouter_box(x0, x0 + w, z0, z0 + d, y_base, h);
    bloc(B("toit"), x0 - .3f, x0 + w + .3f, h - .4f, h, z0 - .3f, z0 + d + .3f, gris(.62f), nullptr, nullptr, false);
    if (y_base == 0) bloc(B("toit"), x0 - .12f, x0 + w + .12f, 0, .9f, z0 - .12f, z0 + d + .12f, gris(.32f), nullptr, nullptr, true);
    parapet(x0, z0, w, d, h);
    return h;
}
static V3 escalier(const Facade& fac, float h, float u0, int sens = 1, float prof = .85f, float larg = 2.2f, float y_depart = 0, bool enreg = true) {
    int n = (int)std::ceil((h - y_depart) / MARCHE); float mh = (h - y_depart) / n;
    Col col = gris(.55f); Batch &toit = B("toit"), &bat = B("toit"); std::vector<V3> pts; V3 base;
    if (n * prof + 3 <= fac.L) {
        for (int i = 0; i < n; i++) {
            float a = u0 + sens * i * prof, b = u0 + sens * (i + 1) * prof;
            fac.box(bat, std::min(a, b), std::max(a, b), 0, larg, y_depart ? y_depart : 0, y_depart + mh * (i + 1), col, &toit);
        }
        base = fac.pt(u0 - sens * 1.8f, larg / 2);
        float ue = u0 + sens * (n - .5f) * prof;
        pts = {fac.pt(ue, larg / 2, h), fac.pt(ue, -2.2f, h)};
    } else {
        int n1 = (int)std::ceil(n / 2.0), n2 = n - n1;
        for (int i = 0; i < n1; i++) { float a = u0 + sens * i * prof, b = u0 + sens * (i + 1) * prof; fac.box(bat, std::min(a, b), std::max(a, b), 0, larg, 0, y_depart + mh * (i + 1), col, &toit); }
        float l0 = u0 + sens * n1 * prof, l1 = l0 + sens * 2.4f;
        fac.box(bat, std::min(l0, l1), std::max(l0, l1), 0, 2 * larg, 0, y_depart + mh * n1, col, &toit);
        for (int i = 0; i < n2; i++) { float a = l0 - sens * i * prof, b = l0 - sens * (i + 1) * prof; fac.box(bat, std::min(a, b), std::max(a, b), larg, 2 * larg, 0, y_depart + mh * (n1 + i + 1), col, &toit); }
        float ue = l0 - sens * (n2 - .5f) * prof;
        fac.box(bat, ue - .9f, ue + .9f, 0, larg, h - .3f, h, col, &toit);
        base = fac.pt(u0 - sens * 1.8f, larg / 2);
        pts = {fac.pt(u0 + sens * (n1 - .5f) * prof, larg / 2, y_depart + mh * n1), fac.pt((l0 + l1) / 2, larg / 2, y_depart + mh * n1),
               fac.pt((l0 + l1) / 2, 1.5f * larg, y_depart + mh * n1), fac.pt(ue, 1.5f * larg, h), fac.pt(ue, larg / 2, h), fac.pt(ue, -2.2f, h)};
    }
    if (enreg && y_depart == 0) acces_toit.push_back({base, pts, pts.back(), h});
    return base;
}
static float rng_teinte() { return (float)RG.choice<int>({22, 35, 200, 140, 0}); }
static float balcons(const Facade& fac, float h, float u0, int sens = 1, float y_depart = 0, float lg = 1.5f) {
    Col col = hsv(rng_teinte(), .5f, .55f);
    int n = (int)std::ceil((h - y_depart) / 2.4f); float mh = (h - y_depart) / n;
    float dispo = sens > 0 ? (fac.L - u0 - 1) : (u0 - 1);
    lg = std::min(lg, std::max(1.2f, dispo / n));
    for (int k = 0; k < n; k++) {
        float a = u0 + sens * k * lg, b = u0 + sens * (k + 1) * lg, top = y_depart + mh * (k + 1);
        fac.box(B("metal"), std::min(a, b), std::max(a, b), 0, 2.0f, top - .25f, top, col, &B("metal"));
        if (k % 3 == 2) fac.box(B("metal"), std::min(a, b) + .1f, std::min(a, b) + .25f, 1.8f, 1.95f, y_depart, top - .25f, gris(.2f), nullptr, false);
    }
    return n * lg;
}
static void pont(float x0, float z0, float x1, float z1, float h, float large = 3.0f) {
    Col c = hsv(40, .6f, .5f);
    if (std::fabs(x1 - x0) > std::fabs(z1 - z0)) {
        bloc(B("metal"), std::min(x0, x1), std::max(x0, x1), h - .35f, h, z0 - large / 2, z0 + large / 2, gris(.35f), &B("metal"));
        for (int sz : {-1, 1}) bloc(B("metal"), std::min(x0, x1), std::max(x0, x1), h, h + .45f, z0 + sz * large / 2 - .08f, z0 + sz * large / 2 + .08f, c);
    } else {
        bloc(B("metal"), x0 - large / 2, x0 + large / 2, h - .35f, h, std::min(z0, z1), std::max(z0, z1), gris(.35f), &B("metal"));
        for (int sx : {-1, 1}) bloc(B("metal"), x0 + sx * large / 2 - .08f, x0 + sx * large / 2 + .08f, h, h + .45f, std::min(z0, z1), std::max(z0, z1), c);
    }
}
static std::vector<V3> mobilier_toit(float x0, float z0, float w, float d, float h, PyRandom& rng, int nb = -1, float marge = 3.0f, std::vector<std::array<float, 4>> exclure = {}) {
    std::vector<std::array<float, 4>> occ = exclure;
    auto place = [&](float sx, float sz, float& ox, float& oz) -> bool {
        for (int k = 0; k < 40; k++) {
            if (w - 2 * marge - sx < 0 || d - 2 * marge - sz < 0) return false;
            float x = rng.uniform(x0 + marge + sx / 2, x0 + w - marge - sx / 2), z = rng.uniform(z0 + marge + sz / 2, z0 + d - marge - sz / 2);
            std::array<float, 4> r = {x - sx / 2 - 1, x + sx / 2 + 1, z - sz / 2 - 1, z + sz / 2 + 1}; bool ok = true;
            for (auto& o : occ) if (r[0] < o[1] && r[1] > o[0] && r[2] < o[3] && r[3] > o[2]) { ok = false; break; }
            if (ok) { occ.push_back(r); ox = x; oz = z; return true; }
        }
        return false;
    };
    if (nb < 0) nb = std::max(2, (int)(w * d / 90));
    static const std::vector<int> TY = {0, 0, 1, 2, 3, 4, 5, 6};
    for (int q = 0; q < nb; q++) {
        int t = rng.choice(TY); float px, pz;
        if (t == 0) { if (place(2.4f, 1.6f, px, pz)) { float g = rng.uniform(.5f, .7f); bloc(B("metal"), px - 1.2f, px + 1.2f, h, h + 1.3f, pz - .8f, pz + .8f, gris(g), &B("metal")); } }
        else if (t == 1) { if (place(4.5f, 4.5f, px, pz)) { Col ch = gris(.4f); bloc(B("toit"), px - 2.25f, px + 2.25f, h, h + 2.9f, pz - 2.25f, pz + 2.25f, gris(.6f), nullptr, &ch); bloc(B("blanc"), px - .6f, px + .6f, h, h + 2.0f, pz - 2.27f, pz - 2.2f, gris(.1f), nullptr, nullptr, false); } }
        else if (t == 2) { if (place(3, 3, px, pz)) {
            for (int sx : {-1, 1}) for (int sz : {-1, 1}) bloc(B("metal"), px + sx * 1.3f - .1f, px + sx * 1.3f + .1f, h, h + 2.1f, pz + sz * 1.3f - .1f, pz + sz * 1.3f + .1f, gris(.25f), nullptr, nullptr, false);
            bloc(B("bois"), px - 1.5f, px + 1.5f, h + 2.1f, h + 4.3f, pz - 1.5f, pz + 1.5f, BLANCF); } }
        else if (t == 3) { if (place(7, 1, px, pz)) {
            for (float sx : {-2.5f, 2.5f}) bloc(B("metal"), px + sx - .1f, px + sx + .1f, h, h + 2.2f, pz - .1f, pz + .1f, gris(.25f), nullptr, nullptr, false);
            float hh = (float)rng.choice<int>({0, 50, 200, 120}); bloc(B("blanc"), px - 3.5f, px + 3.5f, h + 2.0f, h + 5.5f, pz - .15f, pz + .15f, hsv(hh, .75f, .75f)); } }
        else if (t == 4) { if (place(5, 1.2f, px, pz)) bloc(B("bois"), px - 2.5f, px + 2.5f, h, h + 1.0f, pz - .6f, pz + .6f, hsv(45, .4f, .7f)); }
        else if (t == 5) { if (place(3, 3, px, pz)) {
            int k = rng.randint(2, 4);
            for (int i = 0; i < k; i++) { float ox = rng.uniform(-1, 1), oz = rng.uniform(-1, 1); float v = rng.uniform(.5f, .8f);
                bloc(B("bois"), px + ox - .7f, px + ox + .7f, h, h + 1.4f, pz + oz - .7f, pz + oz + .7f, hsv(30, .5f, v)); } } }
        else { if (place(1, 1, px, pz)) {
            bloc(B("metal"), px - .08f, px + .08f, h, h + 7, pz - .08f, pz + .08f, gris(.4f), nullptr, nullptr, false);
            bloc(B("lumiere"), px - .2f, px + .2f, h + 7, h + 7.4f, pz - .2f, pz + .2f, {1, .1f, .1f, 1}, nullptr, nullptr, false); } }
    }
    std::vector<V3> libres;
    for (int k = 0; k < 12; k++) {
        float x = rng.uniform(x0 + 2, x0 + w - 2), z = rng.uniform(z0 + 2, z0 + d - 2); bool ok = true;
        for (auto& o : occ) if (o[0] < x && x < o[1] && o[2] < z && z < o[3]) { ok = false; break; }
        if (ok) libres.push_back({x, h, z});
    }
    return libres;
}
static bool vehicule(float x, float z, bool along_z, PyRandom& rng, const char* genre_ = nullptr) {
    static const std::vector<int> GE = {0, 0, 0, 1, 2, 3};
    int g = 0; if (genre_) { std::string s = genre_; g = s == "voiture" ? 0 : (s == "camionnette" ? 1 : (s == "bus" ? 2 : 3)); } else g = rng.choice(GE);
    static const float DIM[4][3] = {{4.4f, 1.9f, 1.5f}, {5.4f, 2.2f, 2.4f}, {11, 2.7f, 3.2f}, {8, 2.5f, 3.4f}};
    float L = DIM[g][0], Wd = DIM[g][1], H = DIM[g][2];
    float sx = along_z ? Wd : L, sz = along_z ? L : Wd;
    if (touche(x, 0, z, std::max(sx, sz) / 2, 2)) return false;
    float hh = (float)rng.choice<int>({0, 25, 55, 210, 120, 0}); float s = rng.uniform(.2f, .65f); float v = rng.uniform(.3f, .6f);
    Col col = hsv(hh, s, v), cab = gris(.12f); Batch& m = B("metal");
    bloc(m, x - sx / 2, x + sx / 2, .35f, H * .62f, z - sz / 2, z + sz / 2, col, &m);
    if (g <= 1) {
        float cx = along_z ? x : x - L * .06f, cz = along_z ? z - L * .06f : z;
        float csx = along_z ? Wd * .85f : L * .5f, csz = along_z ? L * .5f : Wd * .85f;
        bloc(m, cx - csx / 2, cx + csx / 2, H * .62f, H, cz - csz / 2, cz + csz / 2, cab, &m, nullptr, false);
    } else {
        bloc(m, x - sx / 2 + .1f, x + sx / 2 - .1f, H * .62f, H, z - sz / 2 + .1f, z + sz / 2 - .1f, col, &m, nullptr, false);
        bloc(B("blanc"), x - sx / 2 - .01f, x + sx / 2 + .01f, H * .66f, H * .9f, z - sz / 2 - .01f, z + sz / 2 + .01f, cab, nullptr, nullptr, false);
    }
    for (float fx : {-.36f, .36f}) for (float fz : {-.5f, .5f}) {
        float wx = along_z ? (fx * 1.1f) * Wd : fz * L * .85f, wz = along_z ? fz * L * .85f : (fx * 1.1f) * Wd;
        bloc(m, x + wx - (along_z ? .15f : .4f), x + wx + (along_z ? .15f : .4f), 0, .7f, z + wz - (along_z ? .4f : .15f), z + wz + (along_z ? .4f : .15f), gris(.05f), nullptr, nullptr, false);
    }
    ajouter_box(x - sx / 2, x + sx / 2, z - sz / 2, z + sz / 2, 0, H);
    return true;
}
static void lampadaire(float x, float z, int cote, char axe) {
    Col l = {1, .85f, .5f, 1};
    bloc(B("metal"), x - .09f, x + .09f, 0, 7, z - .09f, z + .09f, gris(.18f), nullptr, nullptr, true);     // poteau plein : on ne le traverse plus
    if (axe == 'x') { bloc(B("metal"), x, x + cote * 1.5f, 6.9f, 7.05f, z - .06f, z + .06f, gris(.18f), nullptr, nullptr, false);
        bloc(B("lumiere"), x + cote * 1.0f, x + cote * 1.6f, 6.78f, 6.9f, z - .2f, z + .2f, l, nullptr, nullptr, false); }
    else { bloc(B("metal"), x - .06f, x + .06f, 6.9f, 7.05f, z, z + cote * 1.5f, gris(.18f), nullptr, nullptr, false);
        bloc(B("lumiere"), x - .2f, x + .2f, 6.78f, 6.9f, z + cote * 1.0f, z + cote * 1.6f, l, nullptr, nullptr, false); }
}
static void feu_tricolore(float x, float z) {
    bloc(B("metal"), x - .08f, x + .08f, 0, 4.5f, z - .08f, z + .08f, gris(.15f), nullptr, nullptr, true);
    Col c = RG.randbelow(2) == 0 ? Col{1, .1f, .1f, 1} : Col{.1f, 1, .2f, 1};
    bloc(B("lumiere"), x - .18f, x + .18f, 3.9f, 4.6f, z - .1f, z + .1f, c, nullptr, nullptr, false);
}
static void arbre_mort(float x, float z, PyRandom& rng) {
    float hh0 = rng.uniform(3, 4.5f);
    bloc(B("bois"), x - .25f, x + .25f, 0, hh0, z - .25f, z + .25f, hsv(25, .4f, .4f));
    for (int k = 0; k < 3; k++) {
        float ox = rng.uniform(-.9f, .9f), oz = rng.uniform(-.9f, .9f), hh = rng.uniform(2.2f, 3.6f);
        bloc(B("bois"), x + ox / 2 - .08f, x + ox / 2 + .08f, hh, hh + .12f, z + oz / 2 - .08f, z + oz / 2 + .08f, hsv(25, .4f, .35f), nullptr, nullptr, false);
        bloc(B("bois"), x + ox - .06f, x + ox + .06f, hh, hh + 1.1f, z + oz - .06f, z + oz + .06f, hsv(25, .4f, .35f), nullptr, nullptr, false);
    }
}
static void poubelle(float x, float z, PyRandom& rng) {
    float hh = (float)rng.choice<int>({120, 200});
    bloc(B("metal"), x - 1, x + 1, 0, 1.4f, z - .6f, z + .6f, hsv(hh, .5f, .35f), &B("metal"));
}
static void barriere(float x, float z, float lg, bool along_z, float h = 1.2f, float y0 = 0) {
    float sx = along_z ? .9f : lg, sz = along_z ? lg : .9f;
    bloc(B("toit"), x - sx / 2, x + sx / 2, y0, y0 + h, z - sz / 2, z + sz / 2, gris(.6f));
}
static void gravats(float x, float z, PyRandom& rng, int n = 6, float rayon = 3) {
    for (int k = 0; k < n; k++) {
        float ox = rng.uniform(-rayon, rayon), oz = rng.uniform(-rayon, rayon), sx = rng.uniform(.8f, 2.2f), sz = rng.uniform(.8f, 2.2f), hh = rng.uniform(.4f, 1.5f);
        bool route = false; for (float rr : ROUTES) if (std::fabs(x + ox - rr) < 8 + sx || std::fabs(z + oz - rr) < 8 + sz) route = true;
        if (!route && !touche(x + ox, 0, z + oz, std::max(sx, sz) / 2, 1)) {
            bool toit = rng.random() < .6; float tt = (float)rng.choice<int>({0, 20, 0}); float v = rng.uniform(.4f, .65f);
            bloc(toit ? B("toit") : B("brique"), x + ox - sx / 2, x + ox + sx / 2, 0, hh, z + oz - sz / 2, z + oz + sz / 2, hsv(tt, .2f, v));
        }
    }
}

static void loot_toit(const std::vector<V3>& pos, int n) {
    static const std::vector<int> LT = {L_ARME, L_ARME, L_ARMURE, L_ARMURE, L_SOIN, L_MUNITIONS, L_GRENADE};
    for (int i = 0; i < n && i < (int)pos.size(); i++) nouveau_loot(pos[i], RG.choice(LT));
}
static void toit_complet(float x0, float z0, float w, float d, float h, PyRandom& rng, int n_loot = 3, std::vector<std::array<float, 4>> excl = {}) {
    loot_toit(mobilier_toit(x0, z0, w, d, h, rng, -1, 3.0f, excl), n_loot);
}
static std::vector<char> cotes(PyRandom& rng, std::vector<char> evite = {}) {
    std::vector<char> c = {'N', 'S', 'E', 'W'}; rng.shuffle(c); std::vector<char> r;
    for (char x : c) if (std::find(evite.begin(), evite.end(), x) == evite.end()) r.push_back(x);
    for (char x : c) if (std::find(evite.begin(), evite.end(), x) != evite.end()) r.push_back(x);
    return r;
}
static void acces_immeuble(float x0, float z0, float w, float d, float h, PyRandom& rng, char ce, char cc, float prof = .85f) {
    Facade f(ce, x0, z0, w, d);
    if (rng.random() < .5) escalier(f, h, 1.8f, 1, prof); else escalier(f, h, f.L - 1.8f, -1, prof);
    Facade f2(cc, x0, z0, w, d);
    if (rng.random() < .5) balcons(f2, h, 1.5f, 1); else balcons(f2, h, f2.L - 1.5f, -1);
}
static const char* STY[] = {"beton", "brique", "verre"};
static float bloc_immeuble(float cx, float cz, int e, PyRandom& rng, std::vector<char> evite) {
    float x0 = cx - 20, z0 = cz - 20;
    std::string st = rng.choice<std::string>({"beton", "beton", "brique", "verre"});
    float h = immeuble(x0, z0, 40, 40, e, st, rng);
    auto c = cotes(rng, evite);
    acces_immeuble(x0, z0, 40, 40, h, rng, c[0], c[1]);
    toit_complet(x0, z0, 40, 40, h, rng, 2);
    return h;
}
static void bloc_lotissement(float cx, float cz, PyRandom& rng) {
    struct Lot { float lx, lz; int e; }; Lot lots[2][2];
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
        float lx = i == 0 ? cx - 21 : cx + 3, lz = j == 0 ? cz - 21 : cz + 3; int e = rng.choice<int>({2, 3, 4}); lots[i][j] = {lx, lz, e};
    }
    if (rng.random() < .6) lots[1][0].e = lots[0][0].e;
    if (rng.random() < .6) lots[1][1].e = lots[0][1].e;
    if (rng.random() < .6) lots[0][1].e = lots[0][0].e;
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {      // ordre du dict Python : (0,0),(0,1),(1,0),(1,1)
        Lot& L = lots[i][j];
        std::string st = rng.choice<std::string>({"beton", "brique", "beton"});
        float h = immeuble(L.lx, L.lz, 18, 18, L.e, st, rng);
        char ex = i == 0 ? 'W' : 'E', ez = j == 0 ? 'S' : 'N'; char a, b;
        if (rng.random() < .5) { a = ex; b = ez; } else { a = ez; b = ex; }
        acces_immeuble(L.lx, L.lz, 18, 18, h, rng, a, b, .8f);
        toit_complet(L.lx, L.lz, 18, 18, h, rng, 1);
    }
    for (int j : {0, 1}) { Lot &A = lots[0][j], &Bq = lots[1][j]; if (A.e == Bq.e) pont(A.lx + 17.6f, A.lz + 9, Bq.lx + .4f, A.lz + 9, A.e * FL); }
    for (int i : {0, 1}) { Lot &A = lots[i][0], &Bq = lots[i][1]; if (A.e == Bq.e) pont(A.lx + 9, A.lz + 17.6f, A.lx + 9, Bq.lz + .4f, A.e * FL); }
}
static void bloc_tiers(float cx, float cz, PyRandom& rng, bool tour = false) {
    float x0 = cx - 20, z0 = cz - 20;
    std::string s1 = rng.choice<std::string>({"beton", "brique"});
    float h1 = immeuble(x0, z0, 40, 40, 3, s1, rng);
    auto c = cotes(rng);
    Facade f(c[0], x0, z0, 40, 40); escalier(f, h1, 1.8f, 1);
    float tw = tour ? 30 : 24; int e2 = tour ? 15 : rng.choice<int>({6, 8, 10});
    float tx0 = cx - tw / 2 + (tour ? 0 : rng.uniform(-3, 3));
    float tz0 = cz - tw / 2 + (tour ? 0 : rng.uniform(-3, 3));
    float h2 = immeuble(tx0, tz0, tw, tw, e2, "verre", rng, h1);
    Facade ft(c[1], tx0, tz0, tw, tw); balcons(ft, h2, 1.5f, 1, h1);
    Facade ft2(c[2], tx0, tz0, tw, tw); balcons(ft2, h2, ft2.L - 1.5f, -1, h1);
    toit_complet(x0, z0, 40, 40, h1, rng, 2, {{tx0 - 3, tx0 + tw + 3, tz0 - 3, tz0 + tw + 3}});
    toit_complet(tx0, tz0, tw, tw, h2, rng, tour ? 4 : 2);
    if (tour) {
        B("blanc").plan(cx - 7, cx + 7, cz - 7, cz + 7, h2 + .03f, gris(.3f));
        Col jn = {1, 1, 0, 1};
        B("blanc").plan(cx - 4, cx - 3, cz - 4, cz + 4, h2 + .05f, jn); B("blanc").plan(cx + 3, cx + 4, cz - 4, cz + 4, h2 + .05f, jn);
        B("blanc").plan(cx - 3, cx + 3, cz - .5f, cz + .5f, h2 + .05f, jn);
        for (int sx : {-1, 1}) for (int sz : {-1, 1})
            bloc(B("lumiere"), cx + sx * 14 - .3f, cx + sx * 14 + .3f, h2 + .45f, h2 + 1.0f, cz + sz * 14 - .3f, cz + sz * 14 + .3f, {1, .15f, .1f, 1}, nullptr, nullptr, false);
        bloc(B("metal"), cx - .15f, cx + .15f, h2, h2 + 16, cz + 10, cz + 10.3f, gris(.35f), nullptr, nullptr, false);
    }
}
static void bloc_parc(float cx, float cz, PyRandom& rng) {
    B("herbe").plan(cx - 21, cx + 21, cz - 21, cz + 21, .045f, BLANCF, 6, 6);
    B("trottoir").plan(cx - 1.6f, cx + 1.6f, cz - 21, cz + 21, .05f, BLANCF, 6, 6);
    B("trottoir").plan(cx - 21, cx + 21, cz - 1.6f, cz + 1.6f, .052f, BLANCF, 6, 6);
    bloc(B("toit"), cx - 4.5f, cx + 4.5f, 0, .7f, cz - 4.5f, cz + 4.5f, gris(.6f));
    B("blanc").plan(cx - 3.8f, cx + 3.8f, cz - 3.8f, cz + 3.8f, .62f, hsv(200, .5f, .45f));
    bloc(B("toit"), cx - .7f, cx + .7f, .7f, 4.5f, cz - .7f, cz + .7f, gris(.5f));
    for (int k = 0; k < 18; k++) { float x = cx + rng.uniform(-20, 20), z = cz + rng.uniform(-20, 20);
        if (std::fabs(x - cx) > 2.5f && std::fabs(z - cz) > 2.5f && !touche(x, 0, z, 1, 3)) arbre_mort(x, z, rng); }
    for (int k = 0; k < 7; k++) { float x = cx + rng.uniform(-18, 18), z = cz + rng.uniform(-18, 18);
        if (!touche(x, 0, z, 1.5f, 1)) bloc(B("bois"), x - 1, x + 1, 0, .5f, z - .35f, z + .35f, BLANCF); }
    float px = cx + 11, pz = cz + 11;
    float h = immeuble(px, pz, 8, 8, 1, "brique", rng);
    balcons(Facade('W', px, pz, 8, 8), h, 1.2f, 1); balcons(Facade('S', px, pz, 8, 8), h, 6.8f, -1);
    loot_toit({{px + 4, h, pz + 4}}, 1);
    for (int k = 0; k < 6; k++) { float bx = cx + rng.uniform(-17, 17), bz = cz + rng.uniform(-17, 17); barriere(bx, bz, 4, rng.random() < .5); }
}
static float ossature(float x0, float z0, float w, float d, int niveaux, PyRandom& rng, bool garde = true, Col couleur = {-1, 0, 0, 1}) {
    float x1 = x0 + w, z1 = z0 + d; Batch& toit = B("toit"); float ep = .5f; Col col = couleur.r < 0 ? gris(.55f) : couleur;
    auto rampe = [&](int k) -> std::array<float, 4> { bool sw = k % 2 == 1; return sw ? std::array<float, 4>{x0 + 2, x0 + 9.4f, z0 + 2, z0 + 6} : std::array<float, 4>{x1 - 9.4f, x1 - 2, z1 - 6, z1 - 2}; };
    std::vector<std::array<float, 4>> rects; for (int k = 1; k <= niveaux; k++) rects.push_back(rampe(k));
    for (int i = 1; i < (int)(w / 8); i++) for (int j = 1; j < (int)(d / 8); j++) {
        float px = x0 + 4 + 8 * (i - 1), pz = z0 + 4 + 8 * (j - 1); bool ok = true;
        for (auto& r : rects) if (r[0] - 1.5f < px && px < r[1] + 1.5f && r[2] - 1.5f < pz && pz < r[3] + 1.5f) { ok = false; break; }
        if (ok && px < x1 - 2 && pz < z1 - 2) bloc(toit, px - .5f, px + .5f, 0, niveaux * FL, pz - .5f, pz + .5f, gris(.5f));
    }
    for (int k = 1; k <= niveaux; k++) {
        float top = k * FL; auto r0 = rects[k - 1];
        float hx0 = r0[0] - .4f, hx1 = r0[1] + .1f, hz0 = r0[2] - .4f, hz1 = r0[3] + .4f;
        float R4[4][4] = {{x0, hx0, z0, z1}, {hx1, x1, z0, z1}, {hx0, hx1, z0, hz0}, {hx0, hx1, hz1, z1}};
        for (auto& q : R4) if (q[1] - q[0] > .1f && q[3] - q[2] > .1f) bloc(toit, q[0], q[1], top - ep, top, q[2], q[3], col);
        int n = 8; float mh = FL / n;
        for (int i = 0; i < n; i++) {
            float a, b;
            if (k % 2 == 1) { a = r0[0] + i * .9f; b = r0[0] + (i + 1) * .9f + (i == n - 1 ? .2f : 0); }
            else { a = r0[1] - (i + 1) * .9f - (i == n - 1 ? .2f : 0); b = r0[1] - i * .9f; }
            bloc(toit, a, b, top - FL, top - FL + mh * (i + 1), r0[2], r0[3], gris(.5f));
        }
        if (garde) {
            float t = .4f, ph = k == niveaux ? .8f : .6f;
            bloc(toit, x0, x1, top, top + ph, z0, z0 + t, gris(.5f)); bloc(toit, x0, x1, top, top + ph, z1 - t, z1, gris(.5f));
            bloc(toit, x0, x0 + t, top, top + ph, z0 + t, z1 - t, gris(.5f)); bloc(toit, x1 - t, x1, top, top + ph, z0 + t, z1 - t, gris(.5f));
        }
        for (int q = 0; q < 3; q++) { float bx = rng.uniform(x0 + 14, x1 - 14), bz = rng.uniform(z0 + 14, z1 - 14); bool az = rng.random() < .5; barriere(bx, bz, 4, az, 1.2f, top); }
        std::vector<V3> lp; for (int q = 0; q < 3; q++) { float a = rng.uniform(x0 + 11, x1 - 11), b = rng.uniform(z0 + 11, z1 - 11); lp.push_back({a, top, b}); }
        loot_toit(lp, 1);
    }
    V3 base = {x0 + .8f, 0, z0 + 4}; std::vector<V3> pts;
    for (int k = 1; k <= niveaux; k++) {
        auto r = rects[k - 1]; float top = k * FL, zm = (r[2] + r[3]) / 2;
        if (k % 2 == 1) { pts.push_back({r[0] + .1f, 0, zm}); pts.push_back({r[1] - .3f, top, zm}); }
        else { pts.push_back({r[1] - .1f, top - FL, zm}); pts.push_back({r[0] + .3f, top, zm}); }
        if (k < niveaux) {
            if (k % 2 == 1) { pts.push_back({x0 + 10, top, z0 + 8}); pts.push_back({x1 - 1, top, z0 + 8}); pts.push_back({x1 - 1, top, z1 - 4}); }
            else { pts.push_back({x1 - 10, top, z1 - 8}); pts.push_back({x0 + 1, top, z1 - 8}); pts.push_back({x0 + 1, top, z0 + 4}); }
        } else { float fin = k % 2 == 1 ? r[1] + 1.8f : r[0] - 1.8f; pts.push_back({fin, top, zm}); }
        acces_toit.push_back({base, pts, pts.back(), top});
    }
    return niveaux * FL;
}
static void bloc_parking(float cx, float cz, PyRandom& rng) {
    ossature(cx - 20, cz - 20, 40, 40, 2, rng);
    for (int k = 0; k < 5; k++) { float a = cx + rng.uniform(-17, 17), b = cz + rng.uniform(-17, 17); bool az = rng.random() < .5; vehicule(a, b, az, rng, "voiture"); }
}
static void bloc_chantier(float cx, float cz, PyRandom& rng) {
    ossature(cx - 17, cz - 17, 32, 32, 3, rng, false, gris(.45f));
    float mx = cx + 19, mz = cz + 19; Col jaune = hsv(48, .85f, .85f);
    bloc(B("metal"), mx - .8f, mx + .8f, 0, 42, mz - .8f, mz + .8f, jaune);
    bloc(B("metal"), mx - 32, mx + 6, 40, 41.4f, mz - .7f, mz + .7f, jaune, nullptr, nullptr, false);
    bloc(B("metal"), mx + 6, mx + 11, 38.8f, 40, mz - 1, mz + 1, gris(.3f), nullptr, nullptr, false);
    bloc(B("metal"), mx - 24 - .05f, mx - 24 + .05f, 30, 40, mz - .05f, mz + .05f, gris(.2f), nullptr, nullptr, false);
    for (int k = 0; k < 4; k++) { float z = cz - 15 + k * 7;
        for (int lvl = 0; lvl < 2; lvl++) { float hh = (float)rng.choice<int>({0, 25, 110, 210});
            bloc(B("metal"), cx + 18.1f, cx + 20.5f, lvl * 2.6f, (lvl + 1) * 2.6f, z - 3, z + 3, hsv(hh, .6f, .6f), &B("metal")); } }
    for (int k = 0; k < 8; k++) { float a = cx + rng.uniform(-19, 19), b = cz + rng.uniform(-19, 19); gravats(a, b, rng, 2, 1.5f); }
}
static void bloc_ruines(float cx, float cz, PyRandom& rng) {
    const int OFF[4][2] = {{-11, -11}, {11, -11}, {-11, 11}, {11, 11}};
    for (auto& o : OFF) {
        if (rng.random() < .25) continue;
        float x0 = cx + o[0] - 8, z0 = cz + o[1] - 8, w = 16, d = 16, t = .6f;
        for (char side : {'N', 'S', 'E', 'W'}) {
            Facade f(side, x0, z0, w, d); float u = 0;
            while (u < f.L - .5f) {
                float seg = std::min(rng.uniform(3, 6), f.L - u);
                if (rng.random() < .7) { float hh = (float)rng.choice<double>({3.3, 6.6, 6.6}); f.box(B("brique"), u, u + seg, -t, 0, 0, hh, gris(.9f), &B("toit")); }
                u += seg + rng.uniform(1.8f, 3.5f);
            }
        }
        bloc(B("toit"), x0 + .6f, x0 + w - .6f, 3.0f, 3.3f, z0 + .6f, z0 + d / 2, gris(.5f));
        for (int i = 0; i < 8; i++) { float a = x0 + 1 + i * .9f; bloc(B("toit"), a, a + .9f, 0, .41f * (i + 1), z0 + d / 2 + 1.5f, z0 + d / 2 + 3.5f, gris(.5f)); }
        bloc(B("toit"), x0 + 8.2f, x0 + 9.6f, 0, 3.3f, z0 + d / 2 + 1.5f, z0 + d / 2 + 3.5f, gris(.5f));
        gravats(x0 + w / 2, z0 + d / 2, rng, 5, 7);
        static const std::vector<int> LT = {L_ARME, L_ARME, L_ARMURE, L_ARMURE, L_SOIN, L_MUNITIONS, L_GRENADE};
        nouveau_loot({x0 + 4, 3.3f, z0 + 3}, RG.choice(LT));
    }
    gravats(cx, cz, rng, 10, 18);
}
static void sol_et_routes() {
    float A = DEMI_MAP;
    B("sol").plan(-A - 70, A + 70, -A - 70, A + 70, 0, BLANCF, 8, 8);
    for (float r : ROUTES) { B("route").plan(r - 6, r + 6, -A, A, .03f, BLANCF, 12, 16); B("route").plan(-A, A, r - 6, r + 6, .032f, BLANCF, 12, 16, true); }
    for (float ax : ROUTES) for (float az : ROUTES) {
        B("blanc").plan(ax - 6, ax + 6, az - 6, az + 6, .05f, gris(.2f));
        for (float k : {-4.5f, -3.f, -1.5f, 0.f, 1.5f, 3.f, 4.5f}) for (int s : {-1, 1}) {
            if (s * 9.4f + ax < A) B("blanc").plan(std::min(ax + s * 6.8f, ax + s * 9.4f), std::max(ax + s * 6.8f, ax + s * 9.4f), az + k - .3f, az + k + .3f, .06f, gris(.8f));
            if (s * 9.4f + az < A) B("blanc").plan(ax + k - .3f, ax + k + .3f, std::min(az + s * 6.8f, az + s * 9.4f), std::max(az + s * 6.8f, az + s * 9.4f), .06f, gris(.8f));
        }
    }
    for (float cx : CENTRES) for (float cz : CENTRES) B("trottoir").plan(cx - 24, cx + 24, cz - 24, cz + 24, .04f, BLANCF, 6, 6);
}
static void enceinte(PyRandom& rng) {
    float A = DEMI_MAP;
    float W4[4][4] = {{0, A + 1, 2 * A + 4, 2}, {0, -A - 1, 2 * A + 4, 2}, {A + 1, 0, 2, 2 * A + 4}, {-A - 1, 0, 2, 2 * A + 4}};
    for (auto& q : W4) bloc(B("mur"), q[0] - q[2] / 2, q[0] + q[2] / 2, 0, 45, q[1] - q[3] / 2, q[1] + q[3] / 2, gris(.78f));
    for (int k = 0; k < 70; k++) {
        float ang = rng.uniform(0, 6.283f), dd = A + rng.uniform(25, 110), x = std::cos(ang) * dd, z = std::sin(ang) * dd;
        if (std::max(std::fabs(x), std::fabs(z)) < A + 15) continue;
        float w = rng.uniform(14, 30), d = rng.uniform(14, 30), h = rng.uniform(25, 95);
        std::string st = rng.choice<std::string>({"beton", "verre", "brique"});
        bloc(B(st.c_str()), x - w / 2, x + w / 2, 0, h, z - d / 2, z + d / 2, gris(.85f), &B("toit"), nullptr, false);
    }
}
static void decor_rues(PyRandom& rng) {
    float A = DEMI_MAP;
    auto procheRoute = [&](float t) { float m = 1e9f; for (float a : ROUTES) m = std::min(m, std::fabs(t - a)); return m; };
    for (float r : ROUTES) { int k = 0; for (int t = (int)(-A + 15); t < (int)(A - 10); t += 30, k++) {
        if (procheRoute((float)t) < 9) continue; int c = k % 2 == 0 ? 1 : -1;
        lampadaire(r + 6.9f * c, (float)t, -c, 'x'); lampadaire((float)t, r + 6.9f * c, -c, 'z'); } }
    for (float ax : ROUTES) for (float az : ROUTES) { feu_tricolore(ax + 6.8f, az + 6.8f); feu_tricolore(ax - 6.8f, az - 6.8f); }
    for (int k = 0; k < 85; k++) {
        float r = rng.choice<double>({-120, -60, 0, 60, 120}); float t = rng.uniform(-A + 12, A - 12);
        if (procheRoute(t) < 11) continue;
        float lane = (float)rng.choice<int>({-1, 1}) * rng.uniform(2.6f, 4.6f);
        if (rng.random() < .5) vehicule(r + lane, t, true, rng); else vehicule(t, r + lane, false, rng);
    }
    for (int k = 0; k < 26; k++) {
        float r = rng.choice<double>({-120, -60, 0, 60, 120}); float t = rng.uniform(-A + 14, A - 14);
        if (procheRoute(t) < 12) continue;
        int s = rng.choice<int>({-1, 1});
        if (rng.random() < .5) { barriere(r + s * 3.8f, t, 4, true); barriere(r - s * 3.8f, t + 6, 4, true); }
        else { barriere(t, r + s * 3.8f, 4, false); barriere(t + 6, r - s * 3.8f, 4, false); }
    }
    for (int k = 0; k < 70; k++) {
        float r = rng.choice<double>({-120, -60, 0, 60, 120}); float t = rng.uniform(-A + 10, A - 10);
        if (procheRoute(t) < 9) continue;
        int s = rng.choice<int>({-1, 1}); float x, z;
        if (rng.random() < .5) { x = r + s * 7.4f; z = t; } else { x = t; z = r + s * 7.4f; }
        if (!touche(x, 0, z, 1.2f, 3)) { if (rng.random() < .55) arbre_mort(x, z, rng); else poubelle(x, z, rng); }
    }
    for (int k = 0; k < 26; k++) { float a = rng.uniform(-A + 12, A - 12), b = rng.uniform(-A + 12, A - 12); gravats(a, b, rng, 4, 3); }
}

void construire_map() {
    RG.seed(3); PyRandom rng(11); R_ = &rng;
    for (Loot* l : loots) delete l; loots.clear();
    init_grille(); init_batches();
    float A = DEMI_MAP; (void)A;
    sol_et_routes(); enceinte(rng);
    struct Type { std::string t; int e; std::set<char>* ev = nullptr; std::vector<char> evite; };
    Type types[6][6];
    static const std::vector<std::string> NOMS = {"immeuble", "lotissement", "tiers", "ruines", "parc"};
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) {
        int k = rng.choices({3, 4, 2, 1.2, .7}); int e = rng.choice<int>({4, 5, 6, 7, 8}); types[i][j] = {NOMS[k], e};
    }
    types[3][3] = {"tour", 0}; types[2][2] = {"parc", 0}; types[4][1] = {"parking", 0}; types[1][4] = {"chantier", 0};
    std::vector<std::array<int, 4>> ponts;
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) for (auto dd : {std::array<int, 2>{1, 0}, std::array<int, 2>{0, 1}}) {
        int di = dd[0], dj = dd[1]; int ni = i + di, nj = j + dj;
        if (ni >= 6 || nj >= 6) continue;
        Type &a = types[i][j], &b = types[ni][nj];
        if (a.t == "immeuble" && b.t == "immeuble" && rng.random() < .6) {
            b.e = a.e; ponts.push_back({i, j, ni, nj});
            char sa = di ? 'E' : 'N', sb = di ? 'W' : 'S';
            a.evite.push_back(sa); b.evite.push_back(sb);
        }
    }
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) {
        float cx = CENTRES[i], cz = CENTRES[j]; Type& d = types[i][j];
        if (d.t == "immeuble") bloc_immeuble(cx, cz, d.e, rng, d.evite);
        else if (d.t == "lotissement") bloc_lotissement(cx, cz, rng);
        else if (d.t == "tiers") bloc_tiers(cx, cz, rng);
        else if (d.t == "tour") bloc_tiers(cx, cz, rng, true);
        else if (d.t == "parc") bloc_parc(cx, cz, rng);
        else if (d.t == "parking") bloc_parking(cx, cz, rng);
        else if (d.t == "chantier") bloc_chantier(cx, cz, rng);
        else bloc_ruines(cx, cz, rng);
    }
    for (auto& p : ponts) {
        int i = p[0], j = p[1], i2 = p[2], j2 = p[3]; float h = types[i][j].e * FL;
        float ca0 = CENTRES[i], ca1 = CENTRES[j], cb0 = CENTRES[i2], cb1 = CENTRES[j2];
        float off = rng.uniform(-8, 8);
        if (i2 != i) pont(ca0 + 19.6f, ca1 + off, cb0 - 19.6f, ca1 + off, h, 3.2f);
        else pont(ca0 + off, ca1 + 19.6f, ca0 + off, cb1 - 19.6f, h, 3.2f);
    }
    decor_rues(rng);
    W.construire();
    spawns.clear(); for (auto s : {std::array<float, 2>{-120, -120}, {120, -120}, {120, 120}, {-120, 120}, {0, 0}}) spawns.push_back({s[0], 0, s[1]});
    for (int k = 0; k < 45; k++) nouveau_loot(point_libre(1.2f), -1);
}
}
