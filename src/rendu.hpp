// FALLING LIGHT - rendu : shader jour/nuit + brouillard, textures procédurales, maillages fusionnés, effets.
#pragma once
#include "common.hpp"

struct Cube { V3 sc, pos; Col col; };              // pièce d'un modèle (échelle, position, couleur de sommet)
struct Piece { Model model{}; bool ok = false; };  // maillage fusionné prêt à dessiner

Piece baker(const std::vector<Cube>& cubes);
Matrix matTRS(V3 pos, float yaw_deg, float scale = 1.0f);   // yaw autour de Y (convention : avant = (sin,0,cos))

struct Rendu {
    Shader sh{};
    int uSun, uSunC, uAmb, uCam, uFogC, uFogR, uMode, uFlash, uSpec, uShadowOn, uShadowRes, uLightVP, uShadowMap, uPcf, uNBias, uDirect, uBump;
    std::string qualite = "moyen"; int ombres = 1; bool post = true;      // ombres : 0 aucune, 1 moyennes (1024), 2 hautes (2048)
    float fogA = 80, fogB = 340, nuit_k = 0;
    Col brume{.64f, .74f, .85f, 1}, sky{1, 1, 1, 1}, disque{1, .92f, .55f, 1};
    void init();
    void maj_jour(float k);                    // 0 = plein jour -> 1 = pleine nuit
    void set_camera(V3 pos);
    void mode(int m, float flash = 0);         // 0 monde, 1 émissif, 2 sans lumière, 3 plat+brouillard, 4 profondeur seule (ombres)
    void set_spec(float s);                    // brillance de la matière (verre, métal, route)
    void preset(const std::string& q);         // bas / moyen / haut
    Texture2D tex(const std::string& nom);
    std::map<std::string, Texture2D> cache;
    // ---- post-traitement, ciel, ombres
    Shader shSky{}, shBright{}, shBlur{}, shComp{}, shFxaa{};
    int sSunTo = 0, sSunC = 0, sHor = 0, sZen = 0, sNuit = 0, sTime = 0, bThr = 0, blDir = 0, cBloom1 = 0, cBloom2 = 0, cExp = 0, cBI = 0, cNuit = 0, fxRes = 0;
    Model skyModel{}; RenderTexture2D scene{}, ldr{}, b1a{}, b1b{}, b2a{}, b2b{}, shadow{}; int rw = 0, rh = 0, shadow_res = 0; bool hdr = true, post_actif = false; Matrix lightVP{}; bool passe_ombre = false;
    int last_mode = -1, last_spec_i = -1000; float last_flash = -1, last_spec = -1;
    void init_post(); void dimensionner(int w, int h);
    void ombre_debut(V3 centre); void ombre_fin(); void lier_ombre(); bool ombre_active() const { return ombres > 0 && qualite != "bas"; }
    void ciel(V3 camPos); void scene_debut(); void scene_fin();
};
extern Rendu R;
void dessiner(const Piece& p, Matrix m, int mode = 0, float flash = 0);

// ---- monde : batches de faces regroupés par bloc de 48 m (un seul maillage par bloc/batch)
constexpr float CHUNK = 48;
struct Sommet { V3 p, n; float u, v; Col c; };
struct Bloc { int cx, cz; std::vector<Sommet> s; Model model{}; V3 centre{}; float rayon = 0; bool ok = false; };
struct Batch {
    std::string nom, tex; float tu, tv; bool emissif;
    std::map<std::pair<int, int>, Bloc> blocs;
    void quad(const V3 p[4], V3 n, Col c, const float uv[4][2]);
    void plan(float x0, float x1, float z0, float z1, float y, Col c, float tu = 0, float tv = 0, bool tourne = false);
    void box(float x0, float x1, float y0, float y1, float z0, float z1, Col c, Batch* haut = nullptr, bool bas = false, const Col* ch = nullptr);
    void construire();
};
struct Monde {
    std::map<std::string, Batch> b;
    void clear() { b.clear(); }
    Batch& batch(const std::string& nom, const std::string& tex, float tu, float tv, bool emi = false);
    void construire() { for (auto& kv : b) kv.second.construire(); }
    void dessiner(const Camera3D& cam, V3 avant);
    void dessiner_ombre(V3 centre, float rayon);          // passe de profondeur pour la carte d'ombres
};
extern Monde W;

// ---- effets visuels (particules, explosions, secousse)
namespace FX {
extern float shake;
void etincelles(V3 pos, Col col, int n = 5, float vit = 2.2f, float dur = .35f, float taille = .07f);
void sang(V3 pos, int n = 7);
void flash(V3 pos, float taille = .2f);
void impact(V3 pos);
void explosion(V3 pos, float taille = 8, Col col = {1, .55f, .05f, 1});
void secousse(float a);
void trace(V3 a, V3 b, Col col, float ep = .04f, float dur = .05f);
void update(float dt);
void dessiner();
void vider();
}
