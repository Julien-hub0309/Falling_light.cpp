#include "rendu.hpp"
#include "rlgl.h"

PyRandom RG(3);
Rendu R;
Monde W;

// ------------------------------------------------------------------ maillages
static Mesh meshDepuis(const std::vector<Sommet>& s) {
    Mesh m{};
    m.vertexCount = (int)s.size(); m.triangleCount = m.vertexCount / 3;
    m.vertices = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.normals = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc(m.vertexCount * 2 * sizeof(float));
    m.colors = (unsigned char*)MemAlloc(m.vertexCount * 4);
    for (int i = 0; i < m.vertexCount; i++) {
        const Sommet& v = s[i];
        m.vertices[i * 3] = v.p.x; m.vertices[i * 3 + 1] = v.p.y; m.vertices[i * 3 + 2] = v.p.z;
        m.normals[i * 3] = v.n.x; m.normals[i * 3 + 1] = v.n.y; m.normals[i * 3 + 2] = v.n.z;
        m.texcoords[i * 2] = v.u; m.texcoords[i * 2 + 1] = v.v;
        Color c = toC(v.c); m.colors[i * 4] = c.r; m.colors[i * 4 + 1] = c.g; m.colors[i * 4 + 2] = c.b; m.colors[i * 4 + 3] = c.a;
    }
    UploadMesh(&m, false);
    return m;
}

static void pushQuad(std::vector<Sommet>& out, const V3 p[4], V3 n, Col c, const float uv[4][2]) {
    V3 a = p[1] - p[0], b = p[2] - p[0];
    bool inv = dot(cross3(a, b), n) < 0;        // face tournée vers l'extérieur (sens trigo vu de l'extérieur)
    int o[4] = {0, 1, 2, 3}; if (inv) { o[0] = 3; o[1] = 2; o[2] = 1; o[3] = 0; }
    auto S = [&](int k) { return Sommet{p[o[k]], n, uv[o[k]][0], uv[o[k]][1], c}; };
    out.push_back(S(0)); out.push_back(S(1)); out.push_back(S(2));
    out.push_back(S(0)); out.push_back(S(2)); out.push_back(S(3));
}

Piece baker(const std::vector<Cube>& cubes) {
    static const int FACES[6][4][3] = {
        {{1, -1, -1}, {1, -1, 1}, {1, 1, 1}, {1, 1, -1}}, {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}},
        {{-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1}}, {{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}},
        {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}, {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}}};
    static const V3 NORM[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    std::vector<Sommet> s;
    for (const Cube& c : cubes)
        for (int f = 0; f < 6; f++) {
            V3 p[4]; float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (int k = 0; k < 4; k++)
                p[k] = {c.pos.x + FACES[f][k][0] * c.sc.x / 2, c.pos.y + FACES[f][k][1] * c.sc.y / 2, c.pos.z + FACES[f][k][2] * c.sc.z / 2};
            pushQuad(s, p, NORM[f], c.col, uv);
        }
    Piece pc; pc.model = LoadModelFromMesh(meshDepuis(s));
    pc.model.materials[0].shader = R.sh; pc.ok = true;
    return pc;
}

Matrix matTRS(V3 pos, float yaw, float sc) {
    return MatrixMultiply(MatrixScale(sc, sc, sc), MatrixMultiply(MatrixRotateY(rad(yaw)), MatrixTranslate(pos.x, pos.y, pos.z)));
}

void dessiner(const Piece& p, Matrix m, int mode, float flash) {
    if (!p.ok) return;
    R.mode(mode, flash);
    Model& md = const_cast<Model&>(p.model);
    md.transform = m;
    DrawModel(md, {0, 0, 0}, 1.0f, WHITE);
}

// ------------------------------------------------------------------ shader
static const char* VS = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;
uniform mat4 mvp; uniform mat4 matModel;
out vec2 uvs; out vec3 wn; out vec3 wp; out vec4 vc;
void main(){ gl_Position=mvp*vec4(vertexPosition,1.0); uvs=vertexTexCoord;
 wn=normalize(mat3(matModel)*vertexNormal); wp=(matModel*vec4(vertexPosition,1.0)).xyz; vc=vertexColor; })";
static const char* FS = R"(#version 330
in vec2 uvs; in vec3 wn; in vec3 wp; in vec4 vc;
uniform sampler2D texture0; uniform vec4 colDiffuse;
uniform vec3 sun_dir; uniform vec3 sun_color; uniform vec3 amb_color; uniform vec3 camera_pos;
uniform vec4 fog_color; uniform vec2 fog_range; uniform int mode; uniform float flash;
out vec4 fragColor;
void main(){
 vec4 t=texture(texture0,uvs); vec4 c=t*colDiffuse*vc;
 if(mode==2){ c.rgb*=1.0+flash; fragColor=c; return; }
 float nuit=1.0-clamp(length(amb_color)/1.1,0.0,1.0);
 if(mode==1){ c.rgb*=0.55+0.9*nuit; }
 else if(mode==3){ }
 else { vec3 n=normalize(wn); float ndl=max(dot(n,-sun_dir),0.0); float sky=0.5+0.5*n.y;
  float em=step(0.82,t.r)*step(0.6,t.g)*step(t.b,0.55);
  vec3 ecl=c.rgb*(amb_color*(0.72+0.28*sky)+sun_color*ndl);
  c.rgb=mix(ecl,t.rgb*vc.rgb*1.15,em*nuit); }
 c.rgb*=1.0+flash;
 float d=length(wp-camera_pos); float f=clamp((d-fog_range.x)/(fog_range.y-fog_range.x),0.0,1.0);
 c.rgb=mix(c.rgb,fog_color.rgb,f); fragColor=c; })";

void Rendu::init() {
    sh = LoadShaderFromMemory(VS, FS);
    sh.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(sh, "matModel");
    uSun = GetShaderLocation(sh, "sun_dir"); uSunC = GetShaderLocation(sh, "sun_color"); uAmb = GetShaderLocation(sh, "amb_color");
    uCam = GetShaderLocation(sh, "camera_pos"); uFogC = GetShaderLocation(sh, "fog_color"); uFogR = GetShaderLocation(sh, "fog_range");
    uMode = GetShaderLocation(sh, "mode"); uFlash = GetShaderLocation(sh, "flash");
    V3 sun = norm({-.5f, -1.1f, -.6f}); SetShaderValue(sh, uSun, &sun, SHADER_UNIFORM_VEC3);
    float fr[2] = {fogA, fogB}; SetShaderValue(sh, uFogR, fr, SHADER_UNIFORM_VEC2);
    maj_jour(0); mode(0);
}

static Col mixc(Col a, Col b, float k) { return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k, 1}; }
void Rendu::maj_jour(float k) {
    Col s = mixc({.95f, .88f, .72f, 1}, {.20f, .26f, .45f, 1}, k), a = mixc({.66f, .68f, .74f, 1}, {.18f, .20f, .32f, 1}, k);
    brume = mixc({.64f, .74f, .85f, 1}, {.03f, .04f, .09f, 1}, k);
    sky = mixc({1, 1, 1, 1}, {.10f, .12f, .28f, 1}, k);
    disque = mixc({1, .92f, .55f, 1}, {.78f, .84f, 1, 1}, k);
    float sv[3] = {s.r, s.g, s.b}, av[3] = {a.r, a.g, a.b}, fv[4] = {brume.r, brume.g, brume.b, 1};
    SetShaderValue(sh, uSunC, sv, SHADER_UNIFORM_VEC3); SetShaderValue(sh, uAmb, av, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, uFogC, fv, SHADER_UNIFORM_VEC4);
}
void Rendu::set_camera(V3 pos) { SetShaderValue(sh, uCam, &pos, SHADER_UNIFORM_VEC3); }
void Rendu::mode(int m, float flash) {
    if (m == 0 && qualite == "bas") m = 3;
    SetShaderValue(sh, uMode, &m, SHADER_UNIFORM_INT); SetShaderValue(sh, uFlash, &flash, SHADER_UNIFORM_FLOAT);
}

// ------------------------------------------------------------------ textures procédurales
struct Img {
    int w, h; std::vector<unsigned char> px;
    Img(int w_, int h_, int r, int g, int b, int amp, uint32_t& seed) : w(w_), h(h_), px(w_ * h_ * 4) {
        for (int i = 0; i < w * h; i++) {
            seed = seed * 1664525u + 1013904223u; int n = (int)((seed >> 16) % (2 * amp + 1)) - amp;
            px[i * 4] = (unsigned char)std::clamp(r + n, 0, 255); px[i * 4 + 1] = (unsigned char)std::clamp(g + n, 0, 255);
            px[i * 4 + 2] = (unsigned char)std::clamp(b + n, 0, 255); px[i * 4 + 3] = 255;
        }
    }
    void set(int x, int y, int r, int g, int b) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        unsigned char* p = &px[(y * w + x) * 4]; p[0] = r; p[1] = g; p[2] = b;
    }
    void rect(int x0, int y0, int x1, int y1, int r, int g, int b) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) set(x, y, r, g, b); }
    void mul(int x0, int y0, int x1, int y1, float k) {
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1 && x < w; x++) if (y < h) { unsigned char* p = &px[(y * w + x) * 4]; p[0] *= k; p[1] *= k; p[2] *= k; }
    }
};
static float frnd(uint32_t& s) { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f; }

Texture2D Rendu::tex(const std::string& nom) {
    auto it = cache.find(nom); if (it != cache.end()) return it->second;
    uint32_t sd = 7;
    auto fenetres = [&](Img& im, int n, int px0, int py0, int step, int ww, int hh, int c0[3], int c1[3], int c2[3], bool croix, float pa, float pb) {
        for (int i = 0; i < n; i++) for (int j = 0; j < 4; j++) {
            int x = i * step + px0, y = j * 64 + py0; float r = frnd(sd);
            int* col = r < pa ? c0 : (r < pb ? c1 : c2);
            im.rect(x - 3, y - 3, x + ww + 2, y + hh + 2, 95, 92, 88);
            im.rect(x, y, x + ww, y + hh, col[0], col[1], col[2]);
            if (r >= pb && croix) { im.rect(x + ww / 2, y, x + ww / 2 + 1, y + hh, 95, 92, 88); im.rect(x, y + hh / 2, x + ww, y + hh / 2 + 1, 95, 92, 88); }
        }
    };
    Img img(64, 64, 140, 136, 130, 14, sd);
    if (nom == "facade") {
        img = Img(256, 256, 150, 148, 144, 14, sd);
        int a[3] = {255, 205, 110}, b[3] = {12, 14, 18}, c[3] = {38, 52, 72};
        fenetres(img, 4, 14, 12, 64, 36, 42, a, b, c, true, .13f, .24f);
        for (int j = 0; j < 4; j++) img.rect(0, j * 64 + 58, 255, j * 64 + 60, 120, 117, 112);
    } else if (nom == "sol") {
        img = Img(128, 128, 88, 86, 80, 18, sd);
        for (int k = 0; k < 14; k++) { int x = (int)(frnd(sd) * 128), y = (int)(frnd(sd) * 128); img.rect(std::max(0, x - 20), y, x + 20, y, 50, 48, 46); }
    } else if (nom == "route") {
        img = Img(64, 128, 52, 53, 58, 10, sd);
        img.rect(3, 0, 5, 127, 190, 190, 190); img.rect(58, 0, 60, 127, 190, 190, 190);
        for (int y = 0; y < 128; y += 32) img.rect(30, y, 33, y + 16, 215, 175, 40);
    } else if (nom == "metal") {
        img = Img(64, 64, 200, 200, 200, 10, sd);
        for (int x = 0; x < 64; x += 8) img.mul(x, 0, x, 63, .72f);
    } else if (nom == "brique") {
        img = Img(256, 256, 150, 78, 62, 16, sd);
        for (int y = 0; y < 256; y += 8) { img.rect(0, y, 255, y, 100, 70, 60); for (int x = (y / 8 % 2) * 8; x < 256; x += 16) img.rect(x, y, x, y + 7, 100, 70, 60); }
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
            int x = i * 64 + 16, y = j * 64 + 10; float r = frnd(sd);
            int* dummy = nullptr; (void)dummy;
            int col[3] = {45, 58, 78}; if (r < .12f) { col[0] = 255; col[1] = 205; col[2] = 110; } else if (r < .25f) { col[0] = 10; col[1] = 12; col[2] = 16; }
            img.rect(x - 3, y - 3, x + 35, y + 47, 215, 205, 190); img.rect(x, y, x + 32, y + 44, col[0], col[1], col[2]);
        }
    } else if (nom == "verre") {
        img = Img(256, 256, 62, 92, 118, 10, sd);
        for (int i = 0; i <= 256; i += 32) img.rect(i, 0, i + 1, 255, 30, 40, 52);
        for (int j = 0; j <= 256; j += 64) img.rect(0, j, 255, j + 3, 30, 40, 52);
        for (int i = 0; i < 8; i++) for (int j = 0; j < 4; j++) {
            float r = frnd(sd);
            if (r < .14f) img.rect(i * 32 + 3, j * 64 + 6, i * 32 + 29, j * 64 + 58, 255, 215, 130);
            else if (r < .3f) img.rect(i * 32 + 3, j * 64 + 6, i * 32 + 29, j * 64 + 58, 20, 28, 40);
        }
    } else if (nom == "trottoir") {
        img = Img(128, 128, 128, 126, 120, 10, sd);
        for (int y = 0; y < 128; y += 32) img.rect(0, y, 127, y, 88, 86, 82);
        for (int x = 0; x < 128; x += 32) img.rect(x, 0, x, 127, 88, 86, 82);
    } else if (nom == "herbe") img = Img(128, 128, 84, 92, 52, 26, sd);
    else if (nom == "bois") { img = Img(64, 64, 110, 78, 50, 12, sd); for (int x = 0; x < 64; x += 6) img.mul(x, 0, x, 63, .75f); }
    Image im{img.px.data(), img.w, img.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D t = LoadTextureFromImage(im);
    GenTextureMipmaps(&t); SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR); SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    cache[nom] = t; return t;
}

// ------------------------------------------------------------------ batches
Batch& Monde::batch(const std::string& nom, const std::string& tex, float tu, float tv, bool emi) {
    auto it = b.find(nom);
    if (it == b.end()) { Batch bt; bt.nom = nom; bt.tex = tex; bt.tu = tu; bt.tv = tv; bt.emissif = emi; it = b.emplace(nom, std::move(bt)).first; }
    return it->second;
}
void Batch::quad(const V3 p[4], V3 n, Col c, const float uv[4][2]) {
    float cx = (p[0].x + p[2].x) * .5f, cz = (p[0].z + p[2].z) * .5f;
    int ix = (int)std::floor(cx / CHUNK), iz = (int)std::floor(cz / CHUNK);
    Bloc& bl = blocs[{ix, iz}]; bl.cx = ix; bl.cz = iz;
    pushQuad(bl.s, p, n, c, uv);
}
void Batch::plan(float x0, float x1, float z0, float z1, float y, Col c, float tu_, float tv_, bool tourne) {
    float tu = tu_ > 0 ? tu_ : this->tu, tv = tv_ > 0 ? tv_ : this->tv;
    V3 p[4] = {{x0, y, z0}, {x1, y, z0}, {x1, y, z1}, {x0, y, z1}}; float uv[4][2];
    for (int i = 0; i < 4; i++) {
        if (tourne) { uv[i][0] = (p[i].z - z0) / tu; uv[i][1] = (p[i].x - x0) / tv; }
        else { uv[i][0] = (p[i].x - x0) / tu; uv[i][1] = (p[i].z - z0) / tv; }
    }
    quad(p, {0, 1, 0}, c, uv);
}
void Batch::box(float x0, float x1, float y0, float y1, float z0, float z1, Col c, Batch* haut, bool bas, const Col* ch) {
    float ts = tu;
    struct F { V3 a, b, c, d, n; bool z; };
    F f[4] = {{{x1, y0, z0}, {x1, y0, z1}, {x1, y1, z1}, {x1, y1, z0}, {1, 0, 0}, true},
              {{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {-1, 0, 0}, true},
              {{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0, 0, 1}, false},
              {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, {0, 0, -1}, false}};
    for (auto& q : f) {
        V3 p[4] = {q.a, q.b, q.c, q.d}; float uv[4][2];
        for (int i = 0; i < 4; i++) { uv[i][0] = (q.z ? p[i].z : p[i].x) / ts; uv[i][1] = p[i].y / ts; }
        quad(p, q.n, c, uv);
    }
    Batch* cible = haut ? haut : this;
    cible->plan(x0, x1, z0, z1, y1, ch ? *ch : c, cible->tu, cible->tv);
    if (bas) { V3 p[4] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}}; float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}}; quad(p, {0, -1, 0}, c, uv); }
}
void Batch::construire() {
    Texture2D t = tex.empty() ? Texture2D{} : R.tex(tex);
    for (auto& kv : blocs) {
        Bloc& bl = kv.second; if (bl.s.empty()) continue;
        V3 mn = bl.s[0].p, mx = mn;
        for (auto& v : bl.s) { mn = {std::min(mn.x, v.p.x), std::min(mn.y, v.p.y), std::min(mn.z, v.p.z)}; mx = {std::max(mx.x, v.p.x), std::max(mx.y, v.p.y), std::max(mx.z, v.p.z)}; }
        bl.centre = (mn + mx) * .5f; bl.rayon = len(mx - mn) * .5f;
        bl.model = LoadModelFromMesh(meshDepuis(bl.s));
        bl.model.materials[0].shader = R.sh;
        if (!tex.empty()) bl.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = t;
        bl.ok = true; bl.s.clear(); bl.s.shrink_to_fit();
    }
}
void Monde::dessiner(const Camera3D& cam, V3 avant) {
    V3 cp = cam.position;
    for (auto& kv : b) {
        Batch& bt = kv.second; R.mode(bt.emissif ? 1 : 0);
        for (auto& bk : bt.blocs) {
            Bloc& bl = bk.second; if (!bl.ok) continue;
            V3 d = bl.centre - cp; float l = len(d);
            if (l - bl.rayon > R.fogB + 40) continue;                                 // au-delà du brouillard
            if (l > bl.rayon + 1 && dot(d, avant) < -bl.rayon) continue;              // derrière la caméra
            if (l > bl.rayon + 1) { float c = dot(d, avant) / l, s = bl.rayon / l; if (c < std::cos(rad(75)) - s) continue; }
            DrawModel(bl.model, {0, 0, 0}, 1.0f, WHITE);
        }
    }
}

// ------------------------------------------------------------------ effets
namespace FX {
float shake = 0;
struct Obj {
    int kind;            // 0 cube, 1 sphère, 2 faisceau
    V3 p0, p1, b; Col c0, c1; float s0, s1, dur, t; int ease;   // ease 0 out_quad, 1 out_expo, 2 linéaire
    V3 scale3_0{0, 0, 0}, scale3_1{0, 0, 0}; bool aplati = false;
};
static std::vector<Obj> objs; static int budget = 0;
static std::vector<std::pair<float, int>> rendus;
static bool libre(int n) { if (budget + n > 160) return false; budget += n; rendus.push_back({1.6f, n}); return true; }
static float ease(int e, float t) { if (e == 0) return 1 - (1 - t) * (1 - t); if (e == 1) return t >= 1 ? 1.0f : 1 - std::pow(2.0f, -10 * t); return t; }
static void particule(V3 pos, V3 vec, Col col, float taille, float dur) {
    V3 cible = pos + vec; cible.y -= std::fabs(vec.y) * .3f + .4f;
    objs.push_back({0, pos, cible, {0, 0, 0}, col, col, taille, .001f, dur, 0, 0});
}
void etincelles(V3 pos, Col col, int n, float vit, float dur, float taille) {
    if (!libre(n)) return;
    for (int i = 0; i < n; i++) { float a = rnd(-1, 1), b = rnd(.2f, 1.2f), c = rnd(-1, 1); particule(pos, V3{a, b, c} * vit, col, taille, dur * rnd(.7f, 1.2f)); }
}
void sang(V3 pos, int n) {
    if (!libre(n)) return;
    for (int i = 0; i < n; i++) { float a = rnd(-1, 1), b = rnd(.3f, 1.4f), c = rnd(-1, 1); Col co = hsv(0, .9f, rnd(.35f, .6f)); float t = rnd(.05f, .1f); particule(pos, V3{a, b, c} * 2.2f, co, t, .5f); }
}
void flash(V3 pos, float taille) { objs.push_back({1, pos, pos, {0, 0, 0}, {1, .85f, .4f, .9f}, {1, .85f, .4f, .9f}, taille, taille, .05f, 0, 2}); }
void impact(V3 pos) { etincelles(pos, {.8f, .8f, .8f, 1}, 3, 1.6f, .25f, .05f); }
void explosion(V3 pos, float taille, Col col) {
    if (taille < 4) { flash(pos, taille * .5f); etincelles(pos, col, 6, 2.5f, .4f, .1f); return; }
    if (!libre(14)) return;
    objs.push_back({1, pos, pos, {0, 0, 0}, {col.r, col.g, col.b, .9f}, {.25f, .1f, .05f, 0}, .3f, taille, .55f, 0, 1});
    objs.push_back({1, pos, pos, {0, 0, 0}, {1, 1, .85f, 1}, {1, 1, .8f, 0}, .2f, taille * .55f, .2f, 0, 2});
    Obj o{1, {pos.x, std::max(pos.y, .15f), pos.z}, {pos.x, std::max(pos.y, .15f), pos.z}, {0, 0, 0}, {1, 1, 1, .3f}, {1, 1, 1, 0}, .5f, taille * 2.3f, .35f, 0, 1};
    o.aplati = true; objs.push_back(o);
    for (int i = 0; i < 6; i++) { float a = rnd(-1, 1), b = rnd(.5f, 1.6f), c = rnd(-1, 1); Col co = hsv(rnd(10, 40), .9f, 1); float t = rnd(.12f, .3f); particule(pos, V3{a, b, c} * (taille * .45f), co, t, .7f); }
    for (int i = 0; i < 3; i++) {
        float a = rnd(-1, 1), c = rnd(-1, 1);
        objs.push_back({1, pos + V3{a, 0, c} * (taille * .2f), pos + V3{a, 0, c} * (taille * .2f) + V3{0, taille * .9f, 0}, {0, 0, 0},
                        {.22f, .22f, .22f, .55f}, {.2f, .2f, .2f, 0}, taille * .3f, taille * .8f, 1.4f, 0, 2});
    }
}
void secousse(float a) { shake = std::max(shake, std::min(a, 1.6f)); }
void trace(V3 a, V3 b, Col col, float ep, float dur) {
    if (dist(a, b) < .5f) return;
    objs.push_back({2, a, b, {0, 0, 0}, col, col, ep, ep, dur, 0, 2});
}
void update(float dt) {
    for (auto& o : objs) o.t += dt;
    objs.erase(std::remove_if(objs.begin(), objs.end(), [](const Obj& o) { return o.t >= o.dur; }), objs.end());
    for (auto& r : rendus) r.first -= dt;
    for (size_t i = 0; i < rendus.size();) if (rendus[i].first <= 0) { budget = std::max(0, budget - rendus[i].second); rendus.erase(rendus.begin() + i); } else i++;
}
void dessiner() {
    rlDisableBackfaceCulling(); rlDisableDepthMask();
    for (auto& o : objs) {
        float k = clampf(o.t / o.dur, 0, 1), e = ease(o.ease, k);
        V3 p = lerp3(o.p0, o.p1, o.kind == 2 ? 0.0f : e);
        float s = lerpf(o.s0, o.s1, e);
        Col c = {lerpf(o.c0.r, o.c1.r, k), lerpf(o.c0.g, o.c1.g, k), lerpf(o.c0.b, o.c1.b, k), lerpf(o.c0.a, o.c1.a, k)};
        Color col = toC(c);
        if (o.kind == 0) DrawCube(p, s, s, s, col);
        else if (o.kind == 1) {
            if (o.aplati) { rlPushMatrix(); rlTranslatef(p.x, p.y, p.z); rlScalef(1, .14f, 1); DrawSphereEx({0, 0, 0}, s * .5f, 8, 10, col); rlPopMatrix(); }
            else DrawSphereEx(p, s * .5f, 8, 10, col);
        } else DrawCylinderEx(o.p0, o.p1, s * .5f, s * .5f, 4, col);
    }
    rlEnableDepthMask(); rlEnableBackfaceCulling();
}
void vider() { objs.clear(); budget = 0; rendus.clear(); }
}
