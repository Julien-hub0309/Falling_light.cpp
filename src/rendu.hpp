// FALLING LIGHT - rendu : shader jour/nuit + brouillard, textures procédurales, maillages fusionnés, effets.
#pragma once
#include "common.hpp"

struct Cube { V3 sc, pos; Col col; };              // pièce d'un modèle (échelle, position, couleur de sommet)
struct Piece { Model model{}; bool ok = false; };  // maillage fusionné prêt à dessiner

Piece baker(const std::vector<Cube>& cubes);
Matrix matTRS(V3 pos, float yaw_deg, float scale = 1.0f);   // yaw autour de Y (convention : avant = (sin,0,cos))

struct Rendu {
    Shader sh{};
    int uSun, uSunC, uAmb, uCam, uFogC, uFogR, uMode, uFlash;
    std::string qualite = "moyen";
    float fogA = 80, fogB = 340;
    Col brume{.64f, .74f, .85f, 1}, sky{1, 1, 1, 1}, disque{1, .92f, .55f, 1};
    void init();
    void maj_jour(float k);                    // 0 = plein jour -> 1 = pleine nuit
    void set_camera(V3 pos);
    void mode(int m, float flash = 0);         // 0 monde, 1 émissif, 2 sans lumière, 3 plat+brouillard
    Texture2D tex(const std::string& nom);
    std::map<std::string, Texture2D> cache;
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
