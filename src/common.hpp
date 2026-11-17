// FALLING LIGHT - socle commun : types, maths, aléatoire identique à Python, constantes.
#pragma once
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <map>
#include <array>
#include <set>
#include <cstdlib>

using V3 = Vector3;
inline V3 operator*(float k, V3 a) { return a * k; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float len(V3 a) { return std::sqrt(dot(a, a)); }
inline float dist(V3 a, V3 b) { return len(a - b); }
inline float hyp(float a, float b) { return std::sqrt(a * a + b * b); }
inline V3 norm(V3 v) { float l = len(v); return l > 1e-9f ? v * (1.0f / l) : V3{0, 0, 1}; }
inline V3 cross3(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline V3 lerp3(V3 a, V3 b, float t) { return a + (b - a) * t; }
inline float rad(float d) { return d * 0.017453292519943295f; }
inline float deg(float r) { return r * 57.29577951308232f; }

// HSV (h 0..360, s,v 0..1) -> couleur flottante, comme ursina color.hsv
struct Col { float r, g, b, a; };
inline Col hsv(float h, float s, float v, float a = 1) {
    h = std::fmod(h, 360.0f); if (h < 0) h += 360;
    float c = v * s, hp = h / 60.0f, x = c * (1 - std::fabs(std::fmod(hp, 2.0f) - 1)), r = 0, g = 0, b = 0;
    if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; }
    else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
    float m = v - c; return {r + m, g + m, b + m, a};
}
inline Col gris(float v) { return {v, v, v, 1}; }
inline Col teinte(Col c, float k, float a = 1) { return {c.r * k, c.g * k, c.b * k, a}; }
inline Color toC(Col c) {
    auto f = [](float v) { return (unsigned char)clampf(v * 255.0f + 0.5f, 0, 255); };
    return {f(c.r), f(c.g), f(c.b), f(c.a)};
}
const Col BLANCF{1, 1, 1, 1};

// ---- aléatoire : Mersenne Twister + méthodes de Python (random.Random) -> même carte que la version Python
struct PyRandom {
    uint32_t mt[624]; int idx = 624;
    explicit PyRandom(uint32_t seed = 5489u) { this->seed(seed); }
    void init_genrand(uint32_t s) {
        mt[0] = s;
        for (int i = 1; i < 624; i++) mt[i] = 1812433253u * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i;
        idx = 624;
    }
    void seed(uint32_t s) {                       // random.seed(int) = init_by_array([s])
        init_genrand(19650218u);
        uint32_t key[1] = {s}; int i = 1, j = 0, k = 624;
        for (; k; k--) {
            mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525u)) + key[j] + j;
            i++; j++; if (i >= 624) { mt[0] = mt[623]; i = 1; } if (j >= 1) j = 0;
        }
        for (k = 623; k; k--) {
            mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941u)) - i;
            i++; if (i >= 624) { mt[0] = mt[623]; i = 1; }
        }
        mt[0] = 0x80000000u; idx = 624;
    }
    uint32_t u32() {
        if (idx >= 624) {
            for (int k = 0; k < 624; k++) {
                uint32_t y = (mt[k] & 0x80000000u) | (mt[(k + 1) % 624] & 0x7fffffffu);
                mt[k] = mt[(k + 397) % 624] ^ (y >> 1) ^ ((y & 1) ? 0x9908b0dfu : 0);
            }
            idx = 0;
        }
        uint32_t y = mt[idx++];
        y ^= y >> 11; y ^= (y << 7) & 0x9d2c5680u; y ^= (y << 15) & 0xefc60000u; y ^= y >> 18;
        return y;
    }
    double random() { uint32_t a = u32() >> 5, b = u32() >> 6; return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0); }
    float uniform(float a, float b) { return (float)(a + (b - a) * random()); }
    uint32_t randbelow(uint32_t n) {
        if (n == 0) return 0;
        int k = 0; uint32_t t = n; while (t) { k++; t >>= 1; }
        uint32_t r = u32() >> (32 - k);
        while (r >= n) r = u32() >> (32 - k);
        return r;
    }
    int randint(int a, int b) { return a + (int)randbelow((uint32_t)(b - a + 1)); }
    template <class T> T choice(const std::vector<T>& v) { return v[randbelow((uint32_t)v.size())]; }
    template <class T> void shuffle(std::vector<T>& v) {
        for (int i = (int)v.size() - 1; i >= 1; i--) { int j = (int)randbelow((uint32_t)(i + 1)); std::swap(v[i], v[j]); }
    }
    int choices(const std::vector<double>& w) {   // random.choices(pop, w)[0] -> indice
        std::vector<double> cum; double s = 0; for (double x : w) { s += x; cum.push_back(s); }
        double r = random() * s; int n = (int)cum.size();
        int lo = 0, hi = n - 1;
        while (lo < hi) { int mid = (lo + hi) / 2; if (r < cum[mid]) hi = mid; else lo = mid + 1; }
        return lo;
    }
};
extern PyRandom RG;   // random global (seed 3 à la construction de la carte, puis libre)
inline float rnd(float a, float b) { return RG.uniform(a, b); }
inline double rnd01() { return RG.random(); }
inline int rndint(int a, int b) { return RG.randint(a, b); }

// ---- règles
constexpr float DEMI_MAP = 180, CELL = 12;
constexpr int DUREE_PARTIE = 600, COUT_TITAN = 50, COUT_RESPAWN = 100;
constexpr float DELAI_RESPAWN_ZOMBIE = 4, RAYON_BRUIT_TIR = 90;
constexpr float FL = 3.3f, MARCHE = 0.45f;
constexpr float HNONE = -1e9f;

struct Cible;
struct Partie;
