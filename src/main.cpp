// FALLING LIGHT - C++ / raylib : boucle principale.
#include "partie.hpp"
#include "touches.hpp"
#include "rlgl.h"

enum Page { PG_ACCUEIL, PG_MODE, PG_CLASSE, PG_JEU };
extern Page page; extern bool options_ouvert, curseur_libre; extern int ignorer_souris;
bool ui_menus(); void ui_options(); void hud_dessiner(); void hud_update(float); void hud_init_carte(); void viewmodel_dessiner(Player*);
void charger_config(); void sauver_config(); void ui_curseur(bool); void ui_menu_fermer();
void ui_auto(const std::string&, bool, int); void ui_echap();
#ifdef FL_DIAG
int diag_run(const std::string&, int);
#endif
static bool quitter = false; void quitter_jeu() { quitter = true; }
static float temps_loot = 0;

static void dessiner_loots() {
    if (!G::partie || !G::partie->loot_actif) return;
    V3 cp = G::joueur->oeil();
    static const Color COL[5] = {LIME, YELLOW, SKYBLUE, ORANGE, RED};
    for (Loot* l : E::loots) {
        if (!l->actif) continue; if (hyp(l->pos.x - cp.x, l->pos.z - cp.z) > 130) continue;
        Color c = COL[l->type]; float y = l->pos.y + .7f + std::sin(l->t * 2) * .12f;
        rlPushMatrix(); rlTranslatef(l->pos.x, y, l->pos.z); rlRotatef(l->t * 90, 0, 1, 0); DrawCube({0, 0, 0}, .5f, .5f, .5f, c); rlPopMatrix();
    }
    rlDisableDepthMask(); for (Loot* l : E::loots) { if (!l->actif || hyp(l->pos.x - cp.x, l->pos.z - cp.z) > 130) continue; Color c = COL[l->type]; c.a = 90; DrawCube({l->pos.x, l->pos.y + 3.5f, l->pos.z}, .1f, 7, .1f, c); } rlEnableDepthMask();
}
static bool dans_vue(V3 p, V3 cp, V3 fw, float maxd, float marge = 4) {
    V3 d = p - cp; float l = len(d); if (l > maxd) return false; if (l < marge + 2) return true; return dot(d, fw) / l > .25f;
}
int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "FALLING LIGHT"); SetExitKey(KEY_NULL); SetTargetFPS(0);
    rlSetClipPlanes(.12, 420);
    R.init(); charger_config();
    E::construire_map(); hud_init_carte();
    G::joueur = new Player(); G::pilotes.push_back(G::joueur);
    static const char* NOMS[19] = {"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel", "India", "Juliet", "Kilo", "Lima", "Mike", "November", "Oscar", "Papa", "Quebec", "Romeo", "Sierra"};
    static const Col BOTC[4] = {{1, 0, 0, 1}, {1, .5f, 0, 1}, {.6f, .2f, .9f, 1}, {.2f, .85f, .2f, 1}};
    std::vector<PiloteIA*> ias; for (int i = 0; i < 19; i++) { Col c = i < 4 ? BOTC[i] : hsv(i * 47.0f, .8f, 1); auto* ia = new PiloteIA(NOMS[i], c); ias.push_back(ia); G::pilotes.push_back(ia); }
    for (auto p : G::pilotes) p->armes = {Armes::couteau(), Armes::couteau()};
    G::partie = new Partie(); G::partie->appliquer_ciel(0);
    int frame = 0; const char* AUTO = getenv("FL_AUTO"); int shot = getenv("FL_SHOT") ? atoi(getenv("FL_SHOT")) : 0, fin = getenv("FL_END") ? atoi(getenv("FL_END")) : 0;
    if (AUTO) { std::string a = AUTO; size_t p1 = a.find(':'), p2 = a.find(':', p1 + 1); ui_auto(a.substr(0, p1), a.substr(p2 + 1) == "nuit", atoi(a.substr(p1 + 1, p2 - p1 - 1).c_str())); }
#ifdef FL_DIAG
    if (getenv("FL_DIAG")) { int r = diag_run(getenv("FL_DIAG"), atoi(getenv("FL_FRAMES"))); CloseWindow(); return r; }
#endif
    Camera3D cam{}; cam.up = {0, 1, 0}; cam.projection = CAMERA_PERSPECTIVE;
    while (!WindowShouldClose() && !quitter) {
        float dt = std::min(GetFrameTime(), .05f); Partie* P = G::partie; Player* J = G::joueur; bool jeu = page == PG_JEU && P->en_cours;
        if (IsKeyPressed(KEY_ESCAPE)) ui_echap();
        P->pause = options_ouvert && jeu; if (jeu && P->pause && !curseur_libre) ui_curseur(true); if (jeu && !options_ouvert && !P->fini && curseur_libre && !J->elimine) ui_curseur(false);
        if (jeu && (P->fini || J->elimine) && !curseur_libre && !J->elimine) ui_curseur(true);
        if (ignorer_souris > 0) { ignorer_souris--; GetMouseDelta(); }
        if (jeu && !options_ouvert) {
            if ((P->fini || J->elimine) && T::presse(T::REJOUER)) { P->ouvrir_menu(); jeu = false; }
            else {
                if (ignorer_souris == 0) J->entrees();
                P->update(dt); J->update(dt);
                for (auto ia : ias) ia->update(dt);
                for (auto t : std::vector<Titan*>(G::titans)) t->update(dt);
                gestion_zombies_update(dt); G::update_grenades(dt);
                for (Loot* l : E::loots) l->t += dt;
            }
        }
        FX::update(dt); hud_update(dt);
        // ---- caméra
        V3 F = J->fwd3(), Rr = J->right3(), U = cross3(Rr, F); float cr = std::cos(rad(J->roll)), sr = std::sin(rad(J->roll));
        V3 up = U * cr + Rr * sr; V3 o = J->oeil() + Rr * J->shake_x + U * J->shake_y;
        if (!jeu) { o = {0, 30, 0}; }
        float asp = (float)GetScreenWidth() / GetScreenHeight(); float fh = jeu ? J->fov : 100;
        cam.position = o; cam.target = o + F; cam.up = up; cam.fovy = deg(2 * std::atan(std::tan(rad(fh) / 2) / asp));
        BeginDrawing();
        bool menus = ui_menus();
        if (!menus) {
            R.dimensionner(GetScreenWidth(), GetScreenHeight());
            auto entites = [&](bool ombre) {                              // pilotes IA, zombies, Titans (la passe d'ombre les inclut tous, sans test de vue)
                for (auto ia : ias) if (ia->vivant && (ombre ? dist(ia->pos, J->pos) < 110 : dans_vue(ia->pos, o, F, 220))) { ia->afficher(); if (!ombre && P->equipes) DrawCube({ia->pos.x, ia->pos.y + 2.35f, ia->pos.z}, .3f, .3f, .3f, toC(ia->couleur)); }
                for (auto z : G::zombies) { if (!z->vivant || !(ombre ? dist(z->pos, J->pos) < 110 : dans_vue(z->pos, o, F, 200))) continue; humano_dessiner(z->mm, z->pos, z->yaw, z->TAILLE, !ombre && z->flash > 0 ? 1.6f : 0); }
                for (auto t : G::titans) if (t->vivant) titan_dessiner(t);
            };
            if (jeu && R.ombre_active()) { R.ombre_debut(J->pos); W.dessiner_ombre(J->pos, 130.0f); entites(true); R.ombre_fin(); }
            R.scene_debut();
            ClearBackground(toC(R.brume));
            R.set_camera(o);
            BeginMode3D(cam);
            R.lier_ombre();
            R.ciel(o);
            if (jeu) viewmodel_dessiner(J);
            W.dessiner(cam, F);
            entites(false);
            dessiner_loots(); G::dessiner_grenades(); FX::dessiner(); P->dessiner_visuels();
            EndMode3D();
            R.scene_fin();
            if (jeu) hud_dessiner();
        }
        ui_options();
        EndDrawing();
        frame++; if (shot && frame == shot) TakeScreenshot("capture.png"); if (fin && frame >= fin) break;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_F12)) TakeScreenshot("capture.png");
    }
    sauver_config(); CloseWindow(); return 0;
}
