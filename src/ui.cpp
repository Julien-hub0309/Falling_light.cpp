// FALLING LIGHT - interface : HUD, minimap, menus (mode / classe / options), armes en vue subjective.
#include "partie.hpp"
#include "touches.hpp"
#include "rlgl.h"


// ------------------------------------------------------------------ état du HUD
static float hurt_a = 0, hitm = 0, ban_t = 0, acc_t = 0; static std::string ban_txt; static bool hud_est_nuit = false;
static std::vector<std::pair<std::string, float>> feed;
void hud_msg(const std::string& t, float ttl) { feed.push_back({t, ttl}); if (feed.size() > 6) feed.erase(feed.begin()); }
void hud_degats() { hurt_a = .45f; }
void hud_touche(bool tete) { hitm = tete ? .25f : .12f; }
void hud_banniere(const std::string& t, float d) { ban_txt = t; ban_t = d; }
void hud_nuit(bool n) { hud_est_nuit = n; }

// ------------------------------------------------------------------ pages / curseur
enum Page { PG_ACCUEIL, PG_MODE, PG_CLASSE, PG_JEU };
Page page = PG_ACCUEIL; static std::string sel_mode = "classique"; static bool sel_nuit = false; bool options_ouvert = false; bool curseur_libre = true; int ignorer_souris = 0;
void ui_menu_revenir() { page = PG_MODE; }
void ui_menu_fermer() { page = PG_JEU; }
void ui_curseur(bool libre) { if (libre && !curseur_libre) EnableCursor(); if (!libre && curseur_libre) { DisableCursor(); ignorer_souris = 2; } curseur_libre = libre; }

static int SW() { return GetScreenWidth(); } static int SH() { return GetScreenHeight(); }
static float UX(float x) { return SW() / 2.0f + x * SH(); } static float UY(float y) { return SH() / 2.0f - y * SH(); }
static void txt(const std::string& s, float x, float y, float sc, Color c, int anc = 0) {
    int fs = std::max(8, (int)(sc * SH() / 36.0f)); int lines = 1; for (char ch : s) if (ch == '\n') lines++;
    float py = UY(y); std::string cur; std::vector<std::string> ls; for (char ch : s) { if (ch == '\n') { ls.push_back(cur); cur.clear(); } else cur += ch; } ls.push_back(cur);
    if (anc == 1) py -= lines * fs / 2.0f;
    for (size_t i = 0; i < ls.size(); i++) { int w = MeasureText(ls[i].c_str(), fs); float px = UX(x); if (anc == 1) px -= w / 2.0f; else if (anc == 2) px -= w; DrawText(ls[i].c_str(), (int)px, (int)(py + i * fs * 1.15f), fs, c); }
}
static bool bouton(float cx, float cy, float w, float h, const std::string& t, Color base, bool sel = false) {
    Rectangle r = {UX(cx - w / 2), UY(cy + h / 2), w * SH(), h * SH()}; Vector2 m = GetMousePosition(); bool sur = CheckCollisionPointRec(m, r);
    Color c = sel ? Color{30, 144, 255, 255} : base; if (sur) { c.r = std::min(255, c.r + 35); c.g = std::min(255, c.g + 35); c.b = std::min(255, c.b + 35); }
    DrawRectangleRec(r, c); DrawRectangleLinesEx(r, 1, {255, 255, 255, 60});
    int fs = std::max(10, (int)(h * SH() * .38f)); int nl = 0; for (char ch : t) if (ch == '\n') nl++;
    { size_t d = 0; int large = 0; while (d <= t.size()) { size_t f = t.find('\n', d); if (f == std::string::npos) f = t.size(); large = std::max(large, MeasureText(t.substr(d, f - d).c_str(), fs)); d = f + 1; }
      if (large > w * SH() * .92f) fs = std::max(8, (int)(fs * (w * SH() * .92f) / large)); }       // le texte ne déborde jamais du bouton
    int tw = MeasureText(t.c_str(), fs);
    if (nl == 0) DrawText(t.c_str(), (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + r.height / 2 - fs / 2), fs, WHITE);
    else { std::string a = t.substr(0, t.find('\n')), b = t.substr(t.find('\n') + 1); DrawText(a.c_str(), (int)(r.x + r.width / 2 - MeasureText(a.c_str(), fs) / 2), (int)(r.y + r.height / 2 - fs * 1.1f), fs, WHITE); DrawText(b.c_str(), (int)(r.x + r.width / 2 - MeasureText(b.c_str(), fs) / 2), (int)(r.y + r.height / 2 + fs * .1f), fs, WHITE); }
    return sur && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
static void fond_menu() { DrawRectangleGradientV(0, 0, SW(), SH(), {10, 14, 30, 255}, {40, 55, 85, 255}); }
static const Color GRISB = {80, 80, 80, 255}, ORANGEC = {255, 150, 30, 255}, VERTC = {40, 170, 70, 255}, CYANC = {30, 190, 220, 255};
void sauver_config(); void quitter_jeu();
static void lancer(const Classe* c) {
    Partie* old = G::partie; G::partie = creer_partie(sel_mode, sel_nuit); delete old; G::partie->demarrer(c);
}
static void page_accueil() {
    fond_menu(); txt("FALLING LIGHT", 0, .3f, 3.2f, ORANGEC, 1);
    if (bouton(0, .07f, .5f, .1f, "SOLO", VERTC)) page = PG_MODE; txt("Affrontez les IA et les zombies", 0, .07f - .062f, .9f, LIGHTGRAY, 1);
    if (bouton(0, -.12f, .5f, .07f, "OPTIONS", GRISB)) options_ouvert = true;
    if (bouton(0, -.27f, .5f, .07f, "QUITTER", GRISB)) quitter_jeu();
    txt("Version C++ / raylib  -  Echap : options et touches", 0, -.47f, .8f, GRAY, 1);
}
static void page_mode() {
    fond_menu(); txt("FALLING LIGHT", 0, .43f, 3.2f, ORANGEC, 1); txt("Choisis un mode de jeu (clic ou touches 1-5)", 0, .34f, 1.3f, WHITE, 1);
    struct M { const char* c; const char* n; const char* d; };
    static const M MODES[5] = {{"classique", "CLASSIQUE", "Survis 10 min\nZombie tué : +1 pt\nTitans et pilotes rivaux"}, {"controle", "POINT DE\nCONTROLE", "Reste 30 s sur le point\npour +10 pts\nZombie tué : +1 pt"},
        {"parcours", "PARCOURS", "Couteau seulement\nPasse tous les points\ndans le temps imparti"}, {"mortequipe", "MATCH A MORT\nPAR EQUIPE", "2 équipes de 10 pilotes\nBleu contre orange\nPremier camp à 150 pts"},
        {"melee", "MELEE\nGENERALE", "10 pilotes, chacun\npour soi, sans loot\nPremier à 150 pts"}};
    for (int i = 0; i < 5; i++) { float x = -.72f + i * .36f; if (bouton(x, .2f, .34f, .14f, MODES[i].n, GRISB, sel_mode == MODES[i].c)) sel_mode = MODES[i].c; txt(MODES[i].d, x - .16f, .1f, .72f, LIGHTGRAY); }
    txt("Moment de la partie (J / N)", 0, -.08f, 1.3f, WHITE, 1);
    if (bouton(-.34f, -.17f, .4f, .09f, "JOUR", GRISB, !sel_nuit)) sel_nuit = false;
    if (bouton(.34f, -.17f, .4f, .09f, "NUIT", GRISB, sel_nuit)) sel_nuit = true;
    txt("Zombies lents et plutôt faibles\nLes tirs attirent les spéciaux\n40 zombies en groupes de 3-4", -.64f, -.23f, .8f, LIGHTGRAY);
    txt("Zombies forts et rapides, 5 géants\nLes tirs attirent les rapaces\n30 rapaces en groupes de 2-3", .04f, -.23f, .8f, LIGHTGRAY);
    bool go = bouton(0, -.4f, .5f, .08f, "CONTINUER  (Entrée)", VERTC);
    if (bouton(.62f, -.4f, .35f, .06f, "OPTIONS", GRISB)) options_ouvert = true;
    if (bouton(-.62f, -.4f, .35f, .06f, "< ACCUEIL", GRISB)) page = PG_ACCUEIL;
    static const char* IDS[5] = {"classique", "controle", "parcours", "mortequipe", "melee"};
    for (int i = 0; i < 5; i++) if (IsKeyPressed(KEY_ONE + i)) sel_mode = IDS[i];
    if (IsKeyPressed(KEY_J)) sel_nuit = false; if (IsKeyPressed(KEY_N)) sel_nuit = true;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) go = true;
    if (go) { if (sel_mode == "parcours") lancer(nullptr); else page = PG_CLASSE; }
}
static void page_classe() {
    fond_menu(); txt("FALLING LIGHT", 0, .40f, 3.2f, ORANGEC, 1); txt("Choisis ta classe (clic ou touches 1-4)", 0, .30f, 1.4f, WHITE, 1);
    for (int i = 0; i < 4; i++) { float x = -.66f + i * .44f; std::string n = std::to_string(i + 1) + "\n" + CLASSES[i].nom; if (bouton(x, .1f, .4f, .25f, n, GRISB)) lancer(&CLASSES[i]); txt(CLASSES[i].desc, x - .18f, -.04f, .9f, LIGHTGRAY); }
    for (int i = 0; i < 4; i++) if (IsKeyPressed(KEY_ONE + i)) lancer(&CLASSES[i]);
    txt((sel_mode == "mortequipe" || sel_mode == "melee") ? "Départ avec ton arme de classe et ton couteau" : "Départ avec couteau uniquement", 0, -.25f, 1, YELLOW, 1);
    if (bouton(0, -.32f, .35f, .06f, "OPTIONS", GRISB)) options_ouvert = true;
    if (bouton(-.62f, -.4f, .35f, .06f, "< CHANGER DE MODE", GRISB)) page = PG_MODE;
}
static bool touches_ouvert = false; static int capt_a = -1, capt_s = 0, capt_delai = 0;
static void page_touches() {
    DrawRectangle(0, 0, SW(), SH(), {0, 0, 0, 215}); txt("TOUCHES", 0, .46f, 2.4f, WHITE, 1);
    txt("Clique sur une case, puis appuie sur la touche ou le bouton voulu.   Echap : annuler   Suppr : effacer la case", 0, .40f, .9f, LIGHTGRAY, 1);
    txt("Les noms suivent la disposition QWERTY du clavier : sur AZERTY, la touche Z s'affiche W, Q s'affiche A, A s'affiche Q.", 0, .365f, .8f, GRAY, 1);
    if (capt_a >= 0) {
        if (capt_delai > 0) capt_delai--;
        else { int c = T::capter(); if (c >= 0) { T::affecter((T::Action)capt_a, capt_s, c == KEY_DELETE ? 0 : c); capt_a = -1; } else if (c == -2) capt_a = -1; }
    }
    const int PAR_COL = 12;
    for (int i = 0; i < T::NB; i++) {
        int col = i / PAR_COL, row = i % PAR_COL; float x0 = col == 0 ? -.86f : .06f, y = .30f - row * .062f;
        txt(T::libelle((T::Action)i), x0, y + .015f, .95f, LIGHTGRAY);
        for (int s = 0; s < 2; s++) {
            bool cap = capt_a == i && capt_s == s; std::string t = cap ? "..." : T::nom_code(T::code((T::Action)i, s));
            if (bouton(x0 + .50f + s * .18f, y, .17f, .05f, t, cap ? Color{200, 120, 20, 255} : GRISB) && capt_a < 0) { capt_a = i; capt_s = s; capt_delai = 2; }
        }
    }
    if (bouton(-.25f, -.47f, .35f, .06f, "REINITIALISER", GRISB)) { T::defauts(); capt_a = -1; }
    if (bouton(.25f, -.47f, .35f, .06f, "RETOUR", VERTC)) { touches_ouvert = false; capt_a = -1; sauver_config(); }
}
void ui_echap() {
    if (capt_a >= 0) { capt_a = -1; return; }
    if (touches_ouvert) { touches_ouvert = false; sauver_config(); return; }
    options_ouvert = !options_ouvert; if (!options_ouvert) sauver_config();
}
static void options_menu() {
    DrawRectangle(0, 0, SW(), SH(), {0, 0, 0, 170}); txt("OPTIONS", 0, .34f, 2.4f, WHITE, 1);
    float y = .2f;
    auto ligne = [&](const std::string& nom, std::string val, std::function<void(int)> chg) { txt(nom, -.38f, y + .012f, 1.1f, LIGHTGRAY); if (bouton(.12f, y, .06f, .05f, "-", GRISB)) chg(-1); txt(val, .27f, y + .012f, 1.1f, WHITE, 1); if (bouton(.42f, y, .06f, .05f, "+", GRISB)) chg(1); y -= .085f; };
    ligne("Sensibilité souris", std::to_string((int)REG.sensibilite), [](int d) { REG.sensibilite = clampf(REG.sensibilite + d * 5, 10, 120); });
    ligne("Champ de vision", std::to_string((int)REG.fov), [](int d) { REG.fov = clampf(REG.fov + d * 5, 80, 120); });
    ligne("Vitesse FOV", std::to_string((int)(REG.fov_vitesse * 10) / 10.0f).substr(0, 3), [](int d) { REG.fov_vitesse = clampf(REG.fov_vitesse + d * .2f, 0, 1.6f); });
    ligne("Souris Y inversée", REG.inverser_y ? "OUI" : "NON", [](int) { REG.inverser_y = !REG.inverser_y; });
    ligne("Graphismes", R.qualite, [](int d) { static const char* Q[3] = {"bas", "moyen", "haut"}; int i = R.qualite == "bas" ? 0 : (R.qualite == "moyen" ? 1 : 2); i = std::clamp(i + d, 0, 2); R.qualite = Q[i]; });
    ligne("Plein écran", IsWindowFullscreen() ? "OUI" : "NON", [](int) { ToggleFullscreen(); });
    if (bouton(0, -.2f, .4f, .07f, "CHANGER LES TOUCHES", ORANGEC)) touches_ouvert = true;
    if (bouton(0, -.34f, .4f, .07f, "REPRENDRE", VERTC)) { options_ouvert = false; sauver_config(); }
    if (page == PG_JEU && G::partie && G::partie->en_cours) { if (bouton(-.3f, -.43f, .4f, .06f, "MENU PRINCIPAL", GRISB)) { options_ouvert = false; G::partie->ouvrir_menu(); } }
    if (bouton(.3f, -.43f, .4f, .06f, "QUITTER", GRISB)) quitter_jeu();
}

// ------------------------------------------------------------------ minimap + HUD
struct RectMM { float x0, x1, z0, z1; }; static std::vector<RectMM> bats;
void hud_init_carte() { bats.clear(); for (auto& b : E::boxes) if ((b.x1 - b.x0) > 14 && (b.z1 - b.z0) > 14 && b.y1 > 5 && b.x1 - b.x0 < 60 && b.y0 == 0) bats.push_back({b.x0, b.x1, b.z0, b.z1}); }
static const float MM_T = .36f, MMX = -.70f, MMY = -.14f;
static Vector2 mmpos(float x, float z) { float k = MM_T / (2 * DEMI_MAP); return {UX(MMX - x * k), UY(MMY + z * k)}; }   // x inversé : repère raylib
static void tri(Vector2 c, float ang_deg, float s, Color col) {
    float a = rad(ang_deg); Vector2 d = {std::sin(a), -std::cos(a)}, p = {-d.y, d.x};
    Vector2 A = {c.x + d.x * s, c.y + d.y * s}, B = {c.x - d.x * s * .8f + p.x * s * .7f, c.y - d.y * s * .8f + p.y * s * .7f}, C = {c.x - d.x * s * .8f - p.x * s * .7f, c.y - d.y * s * .8f - p.y * s * .7f};
    DrawTriangle(A, C, B, col); DrawTriangle(A, B, C, col);
}
static void minimap(Player* p, bool aobj, V3 obj) {
    float S = MM_T * SH(); Vector2 c = {UX(MMX), UY(MMY)}; DrawRectangle((int)(c.x - S / 2), (int)(c.y - S / 2), (int)S, (int)S, {0, 0, 0, 140});
    float k = MM_T / (2 * DEMI_MAP) * SH();
    for (auto& b : bats) DrawRectangle((int)(c.x - b.x1 * k), (int)(c.y - b.z1 * k), std::max(1, (int)((b.x1 - b.x0) * k)), std::max(1, (int)((b.z1 - b.z0) * k)), {153, 166, 179, 150});
    if (aobj) { Vector2 q = mmpos(obj.x, obj.z); DrawCircleV(q, SH() * .009f, SKYBLUE); }
    for (auto o : G::pilotes) if (o != p && o->vivant && !o->elimine && hyp(o->pos.x - p->pos.x, o->pos.z - p->pos.z) < 80) DrawRectangleV(mmpos(o->pos.x, o->pos.z), {SH() * .011f, SH() * .011f}, toC(o->couleur));
    int n = 0; for (auto z : G::zombies) { if (n >= 40) break; if ((z->pos.x - p->pos.x) * (z->pos.x - p->pos.x) + (z->pos.z - p->pos.z) * (z->pos.z - p->pos.z) < 3600) { Vector2 q = mmpos(z->pos.x, z->pos.z); DrawRectangleV(q, {SH() * .007f, SH() * .007f}, z->geant ? RED : LIME); n++; } }
    if (p->titan && p->titan->vivant && p->titan->state == 1) DrawRectangleV(mmpos(p->titan->pos.x, p->titan->pos.z), {SH() * .012f, SH() * .012f}, ORANGE);
    tri(mmpos(p->pos.x, p->pos.z), -p->yaw + 180 + 180, SH() * .014f, WHITE);
}
static std::string fmt2(int v) { char b[8]; snprintf(b, sizeof b, "%02d", v); return b; }
void hud_update(float dt) {
    hurt_a = std::max(0.0f, hurt_a - dt * .8f); hitm -= dt;
    if (ban_t > 0) { ban_t -= dt; if (ban_t <= 0) ban_txt.clear(); }
    for (auto& m : feed) m.second -= dt; feed.erase(std::remove_if(feed.begin(), feed.end(), [](auto& m) { return m.second <= 0; }), feed.end());
}
void hud_dessiner() {
    Partie* P = G::partie; Player* p = G::joueur; if (!P || !p) return; int W_ = SW(), H_ = SH();
    if (hud_est_nuit) DrawRectangle(0, 0, W_, H_, {0, 0, 15, 30});
    bool bas = p->vivant && p->hp < p->hp_max * .3f && p->mode == 0; float pulse = bas ? .18f + .12f * std::fabs(std::sin((float)GetTime() * 5)) : 0;
    float ha = std::max(hurt_a, pulse); if (ha > 0) DrawRectangle(0, 0, W_, H_, {255, 0, 0, (unsigned char)(ha * 255)});
    bool visee = p->zoom, lunette = visee && p->lunette; Color cc = hitm > 0 ? RED : WHITE;
    if (lunette) {
        float R_ = H_ * .45f; Vector2 c = {W_ / 2.0f, H_ / 2.0f};
        DrawRing(c, R_, std::sqrt((float)W_ * W_ + (float)H_ * H_), 0, 360, 80, BLACK);
        DrawLine((int)(c.x - R_), (int)c.y, (int)(c.x + R_), (int)c.y, BLACK); DrawLine((int)c.x, (int)(c.y - R_), (int)c.x, (int)(c.y + R_), BLACK);
    } else if (!visee) {
        int cx = W_ / 2, cy = H_ / 2, g = (int)(H_ * .012f), l = (int)(H_ * .012f);
        DrawLine(cx - g - l, cy, cx - g, cy, cc); DrawLine(cx + g, cy, cx + g + l, cy, cc); DrawLine(cx, cy - g - l, cx, cy - g, cc); DrawLine(cx, cy + g, cx, cy + g + l, cc); DrawPixel(cx, cy, cc);
    }
    if (ban_t > 0 && !ban_txt.empty()) txt(ban_txt, 0, .27f, 2.6f, ORANGEC, 1);
    std::string fe; for (auto& m : feed) fe += m.first + "\n"; if (!fe.empty()) txt(fe, .88f, .16f, 1.0f, YELLOW, 2);
    if (!P->en_cours) return;
    float reste = P->temps_restant(); txt(fmt2((int)(reste / 60)) + ":" + fmt2((int)reste % 60), 0, .46f, 2, WHITE, 1); txt(P->phase_texte(), 0, .41f, 1.1f, WHITE, 1);
    txt(P->lignes_score(p), -.88f, .47f, 1.1f, WHITE); txt(P->info_mode(), 0, .315f, 1.1f, CYANC, 1);
    std::string etat;
    if (P->pause) etat = "PAUSE"; else if (p->elimine && !P->fini) etat = "ELIMINE - spectateur\n[" + T::nom(T::REJOUER) + "] relancer une partie"; else if (P->fini) etat = P->resultat + "\n[" + T::nom(T::REJOUER) + "] rejouer";
    else if (!p->vivant) etat = P->respawn_payant() ? "MORT - réapparition... (-" + std::to_string(COUT_RESPAWN) + " pts)" : std::string("MORT - réapparition dans quelques secondes...");
    if (!etat.empty()) txt(etat, 0, .12f, 2, WHITE, 1);
    V3 obj; bool aobj = P->objectif(obj) && !P->fini; float fr; Col cb;
    if (aobj) { float ang = deg(std::atan2(obj.x - p->pos.x, obj.z - p->pos.z)) - p->yaw; tri({UX(0), UY(.225f)}, -ang, SH() * .03f, SKYBLUE); }
    if (P->barre(fr, cb) && !P->fini) { DrawRectangle((int)UX(-.16f), (int)UY(.285f), (int)(.32f * H_), (int)(.02f * H_), {0, 0, 0, 140}); DrawRectangle((int)UX(-.16f), (int)UY(.282f), (int)(.32f * H_ * clampf(fr, 0, 1)), (int)(.014f * H_), toC(cb)); }
    ArmeP a = p->arme_active(); std::string tt; Color tc = ORANGE;
    if (p->mode == 1) { tt = "[" + T::nom(T::INTERAGIR) + "] EJECTION   [" + T::nom(T::MELEE) + "] EPEE"; txt("TITAN " + std::to_string((int)p->titan_hp), -.86f, -.405f, 1, WHITE);
        DrawRectangle((int)UX(-.86f), (int)UY(-.44f + .013f), (int)(.32f * H_), (int)(.026f * H_), {0, 0, 0, 140}); DrawRectangle((int)UX(-.86f), (int)UY(-.44f + .009f), (int)(.32f * H_ * clampf(p->titan_hp / 400, 0, 1)), (int)(.018f * H_), ORANGE); }
    else {
        Titan* t = p->titan;
        if (!t || !t->vivant) { if (p->titan_cd > 0) tt = "TITAN PRET DANS " + std::to_string((int)p->titan_cd + 1) + "s"; else if (p->score >= COUT_TITAN) tt = "[" + T::nom(T::TITAN) + "] APPELER LE TITAN (-" + std::to_string(COUT_TITAN) + " pts)"; else tt = "TITAN : " + std::to_string(COUT_TITAN) + " pts requis"; }
        else if (t->state == 0) tt = "TITANFALL EN COURS...";
        else { static std::map<std::string, std::string> O = {{"suivre", "SUIVRE"}, {"garder", "GARDER"}, {"aller", "ALLER AU POINT"}, {"attaquer", "ATTAQUER"}};
            if (dist(p->pos, t->pos) < 8) tt = "[" + T::nom(T::INTERAGIR) + "] EMBARQUER   -   ordre : " + O[t->ordre]; else tt = "TITAN : " + O[t->ordre] + "   [" + T::nom(T::ORDRE_SUIVRE) + "] suivre  [" + T::nom(T::ORDRE_GARDER) + "] garder  [" + T::nom(T::ORDRE_ALLER) + "] aller  [" + T::nom(T::ORDRE_ATTAQUER) + "] attaquer"; }
        float f = clampf(p->hp / std::max(1.0f, p->hp_max), 0, 1);
        txt("PV " + std::to_string(std::max(0, (int)p->hp)) + "   ARMURE " + std::to_string((int)p->armure), -.86f, -.405f, 1, WHITE);
        DrawRectangle((int)UX(-.86f), (int)UY(-.44f + .013f), (int)(.32f * H_), (int)(.026f * H_), {0, 0, 0, 140});
        DrawRectangle((int)UX(-.86f), (int)UY(-.44f + .009f), (int)(.32f * H_ * f), (int)(.018f * H_), toC({1 - f, .4f + .6f * f, .1f, 1}));
        DrawRectangle((int)UX(-.86f), (int)UY(-.468f + .006f), (int)(.32f * H_ * std::min(1.0f, p->armure / 50)), (int)(.012f * H_), {0, 150, 255, 255});
    }
    txt(tt, 0, .35f, 1.2f, tc, 1);
    if (a) {
        txt(a->nom, .88f, -.455f, 1.0f, LIGHTGRAY, 2);
        if (!a->melee) txt(a->rech > 0 ? "RECHARGE" : (std::to_string(a->munitions) + " / " + (a->reserve < 0 ? "INF" : std::to_string(a->reserve))), .88f, -.40f, 2.2f, WHITE, 2);
    }
    if (p->mode == 0 && p->nb_tactique) txt(std::to_string(p->nb_tactique) + "x " + p->tactique, .88f, -.345f, 1.0f, LIGHTGRAY, 2);
    minimap(p, aobj, obj);
    txt("Echap : options et touches", 0, -.485f, .7f, GRAY, 1);
}

// ------------------------------------------------------------------ armes en vue subjective
static Col c3(float h, float s, float v) { return hsv(h, s, v); }
static const Col NOIR = gris(.08f), GANT = gris(.06f), CYANW{.2f, .95f, 1, 1}, ROUGEW{1, .08f, .08f, 1}, ORANGEW{1, .5f, .08f, 1};
static Col OR_ = hsv(45, .85f, .85f), LAITON = hsv(45, .75f, .6f), CHROME = gris(.85f), NOYER = hsv(25, .7f, .3f), ACIER = hsv(210, .08f, .92f), ACIER_S = hsv(210, .15f, .45f);
static std::vector<Cube> modele_cubes(const std::string& n) {
    auto K = [](float a, float b, float c, float x, float y, float z, Col col) { return Cube{{a, b, c}, {x, y, z}, col}; };
    if (n == "AR-15") return {K(.085f, .11f, .44f, 0, 0, .04f, c3(85, .38f, .27f)), K(.05f, .025f, .4f, 0, .07f, .05f, NOIR), K(.07f, .07f, .3f, 0, -.005f, .4f, c3(38, .42f, .52f)), K(.03f, .03f, .25f, 0, .01f, .7f, NOIR), K(.045f, .045f, .06f, 0, .01f, .84f, gris(.04f)), K(.075f, .11f, .26f, 0, -.01f, -.3f, c3(38, .42f, .5f)), K(.08f, .12f, .03f, 0, -.01f, -.44f, NOIR), K(.05f, .17f, .09f, 0, -.13f, .06f, c3(38, .42f, .45f)), K(.05f, .12f, .06f, 0, -.11f, -.1f, NOIR), K(.012f, .06f, .03f, -.03f, .1f, .1f, NOIR), K(.012f, .06f, .03f, .03f, .1f, .1f, NOIR), K(.07f, .012f, .03f, 0, .135f, .1f, NOIR), K(.011f, .011f, .01f, 0, .105f, .125f, ROUGEW)};
    if (n == "Kriss Vector") return {K(.1f, .1f, .34f, 0, 0, .04f, gris(.09f)), K(.07f, .04f, .3f, 0, .07f, .05f, gris(.15f)), K(.102f, .02f, .2f, 0, .02f, .05f, CYANW), K(.07f, .07f, .18f, 0, 0, .3f, gris(.12f)), K(.025f, .025f, .12f, 0, 0, .43f, gris(.04f)), K(.045f, .28f, .07f, 0, -.19f, .08f, gris(.09f)), K(.047f, .2f, .02f, 0, -.19f, .115f, CYANW), K(.06f, .05f, .2f, 0, .02f, -.25f, gris(.12f)), K(.045f, .09f, .05f, 0, -.09f, -.07f, NOIR), K(.04f, .08f, .04f, 0, -.07f, .28f, NOIR), K(.012f, .06f, .04f, -.03f, .1f, .1f, gris(.07f)), K(.012f, .06f, .04f, .03f, .1f, .1f, gris(.07f)), K(.07f, .012f, .04f, 0, .135f, .1f, gris(.07f)), K(.01f, .01f, .008f, 0, .1f, .125f, CYANW)};
    if (n == "DRS-50") return {K(.085f, .1f, .5f, 0, 0, .05f, c3(200, .05f, .92f)), K(.087f, .03f, .5f, 0, -.02f, .05f, c3(205, .35f, .45f)), K(.035f, .035f, .9f, 0, .01f, .8f, c3(210, .25f, .22f)), K(.06f, .06f, .12f, 0, .01f, 1.27f, NOIR), K(.055f, .055f, .32f, 0, .095f, .08f, gris(.1f)), K(.075f, .075f, .07f, 0, .095f, .27f, gris(.12f)), K(.075f, .075f, .07f, 0, .095f, -.1f, gris(.12f)), K(.05f, .05f, .01f, 0, .095f, .31f, {.3f, .6f, 1, 1}), K(.08f, .13f, .34f, 0, -.02f, -.38f, c3(200, .05f, .88f)), K(.07f, .04f, .2f, 0, .06f, -.35f, c3(205, .35f, .45f)), K(.05f, .1f, .07f, 0, -.09f, 0, NOIR), K(.05f, .1f, .06f, 0, -.1f, -.14f, NOIR), K(.012f, .13f, .012f, -.035f, -.09f, .55f, c3(210, .25f, .22f)), K(.012f, .13f, .012f, .035f, -.09f, .55f, c3(210, .25f, .22f))};
    if (n == "Pompe") return {K(.075f, .1f, .3f, 0, 0, 0, c3(210, .15f, .25f)), K(.077f, .05f, .12f, 0, 0, .02f, LAITON), K(.045f, .045f, .75f, 0, .025f, .5f, c3(210, .15f, .2f)), K(.035f, .035f, .55f, 0, -.03f, .42f, c3(210, .1f, .3f)), K(.075f, .065f, .22f, 0, -.03f, .3f, NOYER), K(.075f, .13f, .34f, 0, -.02f, -.32f, NOYER), K(.078f, .13f, .03f, 0, -.02f, -.5f, c3(25, .7f, .15f)), K(.014f, .03f, .014f, 0, .06f, .85f, OR_), K(.03f, .03f, .07f, .052f, 0, -.05f, ROUGEW), K(.03f, .03f, .07f, .052f, 0, .04f, ROUGEW), K(.03f, .03f, .07f, .052f, 0, .13f, ROUGEW)};
    if (n == "Couteau") return {K(.022f, .05f, .3f, 0, 0, .3f, ACIER), K(.02f, .03f, .18f, 0, -.02f, .36f, ACIER_S), K(.05f, .06f, .13f, 0, -.01f, .08f, gris(.12f)), K(.09f, .02f, .025f, 0, 0, .15f, gris(.2f)), K(.012f, .012f, .02f, 0, .02f, .02f, ORANGEW)};
    if (n == "Épée") return {K(.06f, .024f, 1.35f, 0, 0, .98f, ACIER), K(.02f, .03f, 1.2f, 0, .013f, .98f, {.3f, .85f, 1, 1}), K(.03f, .02f, .1f, 0, 0, 1.68f, ACIER), K(.32f, .045f, .07f, 0, 0, .3f, ORANGEW), K(.05f, .055f, .3f, 0, 0, .12f, gris(.12f)), K(.075f, .075f, .06f, 0, 0, -.06f, c3(20, .9f, .6f))};
    return {K(.05f, .065f, .27f, 0, .01f, .05f, CHROME), K(.048f, .04f, .2f, 0, -.03f, .03f, gris(.55f)), K(.045f, .13f, .07f, 0, -.09f, -.06f, gris(.2f)), K(.047f, .1f, .04f, 0, -.09f, -.06f, OR_), K(.02f, .02f, .04f, 0, .01f, .2f, OR_), K(.014f, .024f, .014f, 0, .05f, -.07f, NOIR), K(.014f, .024f, .014f, 0, .05f, .17f, NOIR)};
}
V3 vm_ads(const std::string& n, bool titan) {
    V3 a = n == "AR-15" ? V3{0, -.105f, .8f} : n == "Kriss Vector" ? V3{0, -.1f, .62f} : n == "DRS-50" ? V3{0, -.095f, .85f} : n == "Pompe" ? V3{0, -.065f, .85f} : (n == "Couteau" || n == "Épée") ? V3{0, -.05f, .5f} : V3{0, -.05f, .42f};
    return titan ? a * 2.6f : a;
}
static Piece& vm_piece(const std::string& n, bool titan) {
    static std::map<std::string, Piece> C; std::string k = n + (titan ? "T" : "");
    auto it = C.find(k); if (it != C.end()) return it->second;
    auto cubes = modele_cubes(n);
    cubes.push_back({{.07f, .07f, .14f}, {0, -.03f, .32f}, GANT}); cubes.push_back({{.07f, .09f, .1f}, {0, -.1f, -.1f}, GANT});
    bool melee = n == "Couteau" || n == "Épée";
    if (titan && !melee) { cubes.push_back({{.14f, .05f, .5f}, {0, .1f, .1f}, c3(20, .9f, .75f)}); cubes.push_back({{.09f, .014f, .4f}, {0, .045f, .05f}, ORANGEW}); cubes.push_back({{.12f, .12f, .05f}, {0, 0, .62f}, c3(20, .9f, .55f)}); }
    return C.emplace(k, baker(cubes)).first->second;
}
static Matrix basis(V3 R_, V3 U, V3 F, V3 p) { return Matrix{R_.x, U.x, F.x, p.x, R_.y, U.y, F.y, p.y, R_.z, U.z, F.z, p.z, 0, 0, 0, 1}; }
void viewmodel_dessiner(Player* p) {
    if (!p->vivant || p->elimine || p->lunette || p->grimpe) return; auto a = p->arme_active(); if (!a) return;
    bool titan = a->titan; V3 R_ = p->right3(), F = p->fwd3(), U = cross3(R_, F), o = p->oeil();
    float k = titan ? 2.6f : 1.0f; V3 pos = o + R_ * p->gun.x + U * p->gun.y + F * p->gun.z;
    Matrix base = basis(R_, U, F, pos); Matrix sc = MatrixScale(k, k, k);
    if (p->melee_t > 0) { float pr = 1 - p->melee_t / .3f, s = std::sin(pr * 3.14159f); base = MatrixMultiply(MatrixMultiply(MatrixRotateX(rad(-55 * s)), MatrixRotateY(rad(30 * s))), base); }
    ::dessiner(vm_piece(a->modele, titan), MatrixMultiply(sc, base), 2, 0);
    if (p->flash_t > 0 && !a->melee) {
        struct F_ { V3 m; float t; Col c; }; V3 mz = a->modele == "AR-15" ? V3{0, .01f, .88f} : a->modele == "Kriss Vector" ? V3{0, 0, .5f} : a->modele == "DRS-50" ? V3{0, .01f, 1.35f} : a->modele == "Pompe" ? V3{0, .02f, .9f} : V3{0, .01f, .23f};
        float t = a->modele == "AR-15" ? .24f : a->modele == "Kriss Vector" ? .2f : a->modele == "DRS-50" ? .45f : a->modele == "Pompe" ? .5f : .16f;
        Col c = a->modele == "Kriss Vector" ? Col{.4f, .95f, 1, .95f} : a->modele == "DRS-50" ? Col{.85f, .92f, 1, .95f} : a->modele == "Pompe" ? Col{1, .5f, .1f, .95f} : Col{1, .8f, .3f, .95f};
        V3 w = Vector3Transform(mz * k, base); DrawSphereEx(w, t * k * .5f, 6, 6, toC(c));
    }
}
void sauver_config() {
    FILE* f = fopen("falling_light.cfg", "w"); if (!f) return;
    fprintf(f, "sensibilite %f\nfov %f\nfov_vitesse %f\ninverser_y %d\nqualite %s\n", REG.sensibilite, REG.fov, REG.fov_vitesse, REG.inverser_y ? 1 : 0, R.qualite.c_str());
    T::ecrire(f); fclose(f);
}
void charger_config() {
    FILE* f = fopen("falling_light.cfg", "r"); if (!f) return; char l[160];
    while (fgets(l, sizeof l, f)) {
        float v; char q[16]; int i;
        if (sscanf(l, "sensibilite %f", &v) == 1) REG.sensibilite = clampf(v, 5, 200); else if (sscanf(l, "fov_vitesse %f", &v) == 1) REG.fov_vitesse = clampf(v, 0, 3);
        else if (sscanf(l, "fov %f", &v) == 1) REG.fov = clampf(v, 60, 130); else if (sscanf(l, "inverser_y %d", &i) == 1) REG.inverser_y = i != 0;
        else if (sscanf(l, "qualite %15s", q) == 1) R.qualite = q; else T::lire_ligne(l);
    }
    fclose(f);
}
bool ui_menus() {   // retourne true si une page de menu est affichée (pas de monde 3D)
    if (page == PG_JEU) return false;
    if (page == PG_ACCUEIL) page_accueil(); else if (page == PG_MODE) page_mode(); else page_classe();
    return true;
}
void ui_options() { if (options_ouvert) { if (touches_ouvert) page_touches(); else options_menu(); } }
void ui_auto(const std::string& m, bool n, int cl) { sel_mode = m; sel_nuit = n; lancer(cl < 0 ? nullptr : &CLASSES[cl]); }

