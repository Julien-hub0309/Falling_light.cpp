// FALLING LIGHT - état global du monde : boîtes de collision, grille, rayons, loots, fumigènes.
#pragma once
#include "common.hpp"
#include "rendu.hpp"

struct Box { float x0, x1, z0, z1, y0, y1; };
struct AccesToit { V3 base; std::vector<V3> pts; V3 toit; float y; };
struct FumeeData { V3 pos; float rayon, vie; };
enum LootType { L_SOIN, L_MUNITIONS, L_ARMURE, L_GRENADE, L_ARME };
struct Loot { V3 pos; LootType type; bool temporaire, actif = true; float vie = 45, t = 0; };

namespace E {
extern std::vector<Box> boxes;
extern std::vector<AccesToit> acces_toit;
extern std::vector<V3> spawns;
extern std::vector<FumeeData> fumees;
extern std::vector<Loot*> loots;

void init_grille();
void ajouter_box(float x0, float x1, float z0, float z1, float y0, float y1);
float hits(float x, float y, float z, float r, float h);      // haut de la boîte la plus haute touchée, ou HNONE
inline bool touche(float x, float y, float z, float r, float h) { return hits(x, y, z, r, h) > HNONE / 2; }
float ray_world(V3 o, V3 d, float portee);
float ray_cyl(V3 o, V3 d, float cx, float cz, float cy, float r, float h);   // -1 si raté
bool visible(V3 a, V3 b);
bool libre(V3 a, V3 b, float hauteur = .48f);        // teste deux hauteurs (juste sous la marche franchissable, et le corps)
int box_sous(float x, float y, float z);              // indice de la boîte dont le dessus est à ~y sous (x,z), sinon -1
float hauteur_sol(float x, float z, float r, float ymax);
V3 point_libre(float rayon = 1.0f, const std::vector<V3>& loin_de = {}, float dmin = 0, int essais = 80);
Loot* nouveau_loot(V3 pos, int type = -1, bool temporaire = false);   // type -1 = tirage pondéré
void construire_map();
}
