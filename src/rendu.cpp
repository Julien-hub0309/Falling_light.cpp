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
    R.mode(mode, flash); R.set_spec(0);
    Model& md = const_cast<Model&>(p.model);
    md.transform = m;
    DrawModel(md, {0, 0, 0}, 1.0f, WHITE);
}

// ------------------------------------------------------------------ shader principal (éclairage, ombres, relief, reflets, brume)
static const char* VS = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 lightVP; uniform float nbias;
out vec2 uvs; out vec3 wn; out vec3 wp; out vec4 vc; out vec4 lpos;
void main(){
 vec4 w=matModel*vec4(vertexPosition,1.0); wp=w.xyz; wn=normalize(mat3(matModel)*vertexNormal);
 gl_Position=mvp*vec4(vertexPosition,1.0); uvs=vertexTexCoord; vc=vertexColor; lpos=lightVP*vec4(wp+wn*nbias,1.0);
})";
static const char* FS = R"(#version 330
in vec2 uvs; in vec3 wn; in vec3 wp; in vec4 vc; in vec4 lpos;
uniform sampler2D texture0; uniform sampler2D shadowMap; uniform vec4 colDiffuse;
uniform vec3 sun_dir; uniform vec3 sun_color; uniform vec3 amb_color; uniform vec3 camera_pos;
uniform vec4 fog_color; uniform vec2 fog_range; uniform int mode; uniform float flash;
uniform float spec; uniform int shadow_on; uniform float shadow_res; uniform int pcf; uniform int direct; uniform float bump;
out vec4 fragColor;
float hash21(vec2 p){ p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y); }
float vnoise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
 return mix(mix(hash21(i),hash21(i+vec2(1,0)),f.x),mix(hash21(i+vec2(0,1)),hash21(i+vec2(1,1)),f.x),f.y); }
vec3 tonemap(vec3 c){ float m=max(c.r,max(c.g,c.b)); if(m<=0.75) return c; float t=0.75+0.25*(1.0-exp(-(m-0.75)/0.25)); return c*(t/m); }
vec3 bumpNormal(vec3 n, vec3 p, float h){
 vec3 dpdx=dFdx(p), dpdy=dFdy(p); float dhx=dFdx(h), dhy=dFdy(h);
 vec3 r1=cross(dpdy,n), r2=cross(n,dpdx); float det=dot(dpdx,r1); vec3 grad=sign(det)*(dhx*r1+dhy*r2);
 return normalize(abs(det)*n-bump*grad); }
float shadowTerm(float ndl){
 if(shadow_on==0) return 1.0;
 vec3 pc=lpos.xyz/lpos.w*0.5+0.5;
 if(pc.z>=1.0||pc.x<0.0||pc.x>1.0||pc.y<0.0||pc.y>1.0) return 1.0;
 float bias=max(0.0008*(1.0-ndl),0.0003); float texel=1.0/shadow_res; float s=0.0, cnt=0.0;
 for(int x=-pcf;x<=pcf;x++) for(int y=-pcf;y<=pcf;y++){ float d=texture(shadowMap,pc.xy+vec2(x,y)*texel).r; s+=(pc.z-bias<=d)?1.0:0.0; cnt+=1.0; }
 s/=cnt; float e=min(min(pc.x,1.0-pc.x),min(pc.y,1.0-pc.y)); return mix(1.0,s,smoothstep(0.0,0.06,e)); }
void main(){
 if(mode==4){ fragColor=vec4(1.0); return; }
 vec4 t=texture(texture0,uvs); vec4 c=t*colDiffuse*vc;
 if(mode==2){ c.rgb*=1.0+flash; fragColor=c; return; }
 float nuit=1.0-clamp(length(amb_color)/1.1,0.0,1.0);
 vec3 V=wp-camera_pos; float d=length(V); vec3 vdir=V/max(d,1e-4);
 if(mode==1){ c.rgb*=0.55+0.9*nuit; }
 else if(mode==3){ }
 else {
  vec3 ng=normalize(wn); vec3 n=ng;
  if(bump>0.0) n=bumpNormal(ng,wp,dot(t.rgb,vec3(0.3333)));
  float ndl=max(dot(n,-sun_dir),0.0);
  float sh=shadowTerm(max(dot(ng,-sun_dir),0.0));
  float sky=0.5+0.5*n.y; float wall=1.0-abs(ng.y);
  float ao=mix(1.0,mix(0.58,1.0,smoothstep(0.0,2.6,wp.y)),wall);
  float var=0.88+0.24*vnoise(wp.xz*0.045+vec2(wp.y*0.03));
  vec3 alb=c.rgb*var;
  float em=step(0.82,t.r)*step(0.6,t.g)*step(t.b,0.55);
  vec3 bounce=sun_color*0.22*max(dot(n,normalize(vec3(sun_dir.x,0.0,sun_dir.z))),0.0)*(1.0-ndl)*ao;
  vec3 ecl=alb*(amb_color*(0.72+0.28*sky)*ao+sun_color*ndl*sh*1.15+bounce);
  if(spec>0.0){ vec3 Vv=-vdir; vec3 H=normalize(Vv-sun_dir); float sp=pow(max(dot(n,H),0.0),48.0)*spec*sh;
   float fr=pow(1.0-max(dot(n,Vv),0.0),4.0); ecl+=sun_color*sp*1.4+mix(amb_color*1.2,fog_color.rgb,0.35)*fr*spec*0.7; }
  c.rgb=mix(ecl,t.rgb*vc.rgb*1.15*(1.0+0.7*nuit),em*nuit);
 }
 c.rgb*=1.0+flash;
 float f=clamp((d-fog_range.x)/(fog_range.y-fog_range.x),0.0,1.0);
 float sunamt=pow(max(dot(vdir,-sun_dir),0.0),5.0);
 vec3 fogc=fog_color.rgb+sun_color*sunamt*0.22*(1.0-nuit);
 c.rgb=mix(c.rgb,fogc,f);
 if(direct==1) c.rgb=tonemap(c.rgb);
 fragColor=c; })";

// ------------------------------------------------------------------ ciel procédural
static const char* SKY_VS = R"(#version 330
in vec3 vertexPosition; uniform mat4 mvp; out vec3 vdir;
void main(){ vdir=vertexPosition; vec4 p=mvp*vec4(vertexPosition,1.0); gl_Position=p.xyww; })";
static const char* SKY_FS = R"(#version 330
in vec3 vdir; uniform vec3 sunTo; uniform vec3 sunc; uniform vec3 hor; uniform vec3 zen; uniform float nuit; uniform float tms;
out vec4 fragColor;
float hash21(vec2 p){ p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y); }
float vnoise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
 return mix(mix(hash21(i),hash21(i+vec2(1,0)),f.x),mix(hash21(i+vec2(0,1)),hash21(i+vec2(1,1)),f.x),f.y); }
float fbm(vec2 p){ float s=0.0,a=0.5; for(int i=0;i<5;i++){ s+=a*vnoise(p); p=p*2.03+vec2(7.1,3.7); a*=0.5; } return s; }
void main(){
 vec3 d=normalize(vdir); float h=clamp(d.y,0.0,1.0);
 vec3 col=mix(hor,zen,pow(h,0.5)); if(d.y<0.0) col=hor;
 float s=max(dot(d,sunTo),0.0);
 vec3 glow=sunc*(pow(s,6.0)*0.20+pow(s,48.0)*0.50);
 float disc=smoothstep(0.9972,0.9986,s);
 if(d.y>0.0){
  vec2 p=d.xz/(d.y+0.18)*1.7+vec2(tms*0.010,tms*0.004); float n=fbm(p);
  float cov=mix(0.50,0.60,nuit); float cl=smoothstep(cov,cov+0.22,n)*smoothstep(0.0,0.22,d.y);
  float lit=0.55+0.45*pow(s,2.0); vec3 cc=mix(vec3(0.60,0.64,0.72),vec3(1.0,0.98,0.94),lit);
  cc=mix(cc,cc*vec3(0.10,0.13,0.26),nuit); cc*=1.0-0.18*smoothstep(0.62,0.9,n);
  col=mix(col,cc,cl*0.88); }
 if(nuit>0.05&&d.y>0.02){ vec3 g=floor(d*230.0); float hs=fract(sin(dot(g,vec3(12.9898,78.233,37.719)))*43758.5453);
  float star=step(0.9975,hs)*(0.6+0.4*sin(tms*3.0+hs*80.0)); col+=vec3(star)*nuit*smoothstep(0.02,0.2,d.y)*1.6; }
 col+=glow*(1.0-0.5*nuit); col=mix(col,sunc*7.0,disc);
 fragColor=vec4(col,1.0); })";

// ------------------------------------------------------------------ post-traitement
static const char* BRIGHT_FS = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform float thr; out vec4 finalColor;
void main(){ vec3 c=texture(texture0,fragTexCoord).rgb; float l=max(c.r,max(c.g,c.b)); float w=smoothstep(thr-0.10,thr+0.55,l); finalColor=vec4(c*w,1.0); })";
static const char* BLUR_FS = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform vec2 dir; out vec4 finalColor;
void main(){ vec3 s=texture(texture0,fragTexCoord).rgb*0.2270270270;
 s+=texture(texture0,fragTexCoord+dir*1.3846153846).rgb*0.3162162162; s+=texture(texture0,fragTexCoord-dir*1.3846153846).rgb*0.3162162162;
 s+=texture(texture0,fragTexCoord+dir*3.2307692308).rgb*0.0702702703; s+=texture(texture0,fragTexCoord-dir*3.2307692308).rgb*0.0702702703;
 finalColor=vec4(s,1.0); })";
static const char* COMP_FS = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform sampler2D bloom1; uniform sampler2D bloom2;
uniform float expo; uniform float bi; uniform float nuit; out vec4 finalColor;
vec3 tonemap(vec3 c){ float m=max(c.r,max(c.g,c.b)); if(m<=0.75) return c; float t=0.75+0.25*(1.0-exp(-(m-0.75)/0.25)); return c*(t/m); }
float hash(vec2 p){ return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453); }
void main(){
 vec3 c=texture(texture0,fragTexCoord).rgb;
 vec3 b=texture(bloom1,fragTexCoord).rgb*0.65+texture(bloom2,fragTexCoord).rgb*0.85;
 c=(c+b*bi)*expo; c=tonemap(c);
 float l=dot(c,vec3(0.2126,0.7152,0.0722)); c=mix(vec3(l),c,1.10);             // légère saturation
 c=(c-0.5)*1.05+0.5; c=clamp(c,0.0,1.0);
 vec2 q=fragTexCoord-0.5; float vg=1.0-dot(q,q)*0.55; c*=mix(1.0,vg,0.9);       // vignettage
 c+=(hash(fragTexCoord*vec2(1280.0,720.0))-0.5)/255.0;                          // tramage anti-bandes
 finalColor=vec4(c,1.0); })";
static const char* FXAA_FS = R"(#version 330
in vec2 fragTexCoord; uniform sampler2D texture0; uniform vec2 res; out vec4 finalColor;
float luma(vec3 c){ return dot(c,vec3(0.299,0.587,0.114)); }
void main(){
 vec2 px=1.0/res; vec3 rgbM=texture(texture0,fragTexCoord).rgb;
 vec3 rgbNW=texture(texture0,fragTexCoord+vec2(-1,-1)*px).rgb, rgbNE=texture(texture0,fragTexCoord+vec2(1,-1)*px).rgb;
 vec3 rgbSW=texture(texture0,fragTexCoord+vec2(-1,1)*px).rgb, rgbSE=texture(texture0,fragTexCoord+vec2(1,1)*px).rgb;
 float lM=luma(rgbM), lNW=luma(rgbNW), lNE=luma(rgbNE), lSW=luma(rgbSW), lSE=luma(rgbSE);
 float lmin=min(lM,min(min(lNW,lNE),min(lSW,lSE))), lmax=max(lM,max(max(lNW,lNE),max(lSW,lSE)));
 vec2 dir; dir.x=-((lNW+lNE)-(lSW+lSE)); dir.y=((lNW+lSW)-(lNE+lSE));
 float dirReduce=max((lNW+lNE+lSW+lSE)*0.25*0.125,1.0/128.0); float rcpDirMin=1.0/(min(abs(dir.x),abs(dir.y))+dirReduce);
 dir=clamp(dir*rcpDirMin,vec2(-8.0),vec2(8.0))*px;
 vec3 rgbA=0.5*(texture(texture0,fragTexCoord+dir*(1.0/3.0-0.5)).rgb+texture(texture0,fragTexCoord+dir*(2.0/3.0-0.5)).rgb);
 vec3 rgbB=rgbA*0.5+0.25*(texture(texture0,fragTexCoord+dir*-0.5).rgb+texture(texture0,fragTexCoord+dir*0.5).rgb);
 float lB=luma(rgbB); finalColor=vec4((lB<lmin||lB>lmax)?rgbA:rgbB,1.0); })";

void Rendu::init() {
    sh = LoadShaderFromMemory(VS, FS);
    sh.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(sh, "matModel");
    uSun = GetShaderLocation(sh, "sun_dir"); uSunC = GetShaderLocation(sh, "sun_color"); uAmb = GetShaderLocation(sh, "amb_color");
    uCam = GetShaderLocation(sh, "camera_pos"); uFogC = GetShaderLocation(sh, "fog_color"); uFogR = GetShaderLocation(sh, "fog_range");
    uMode = GetShaderLocation(sh, "mode"); uFlash = GetShaderLocation(sh, "flash"); uSpec = GetShaderLocation(sh, "spec");
    uShadowOn = GetShaderLocation(sh, "shadow_on"); uShadowRes = GetShaderLocation(sh, "shadow_res"); uLightVP = GetShaderLocation(sh, "lightVP");
    uShadowMap = GetShaderLocation(sh, "shadowMap"); uPcf = GetShaderLocation(sh, "pcf"); uNBias = GetShaderLocation(sh, "nbias");
    uDirect = GetShaderLocation(sh, "direct"); uBump = GetShaderLocation(sh, "bump");
    V3 sun = norm({-.5f, -1.1f, -.6f}); SetShaderValue(sh, uSun, &sun, SHADER_UNIFORM_VEC3);
    float fr[2] = {fogA, fogB}; SetShaderValue(sh, uFogR, fr, SHADER_UNIFORM_VEC2);
    int zero = 0; float bump = 0.035f; SetShaderValue(sh, uShadowOn, &zero, SHADER_UNIFORM_INT); SetShaderValue(sh, uBump, &bump, SHADER_UNIFORM_FLOAT);
    lightVP = MatrixIdentity(); SetShaderValueMatrix(sh, uLightVP, lightVP);
    maj_jour(0); mode(0); set_spec(0);
    init_post();
}
void Rendu::preset(const std::string& q) { qualite = q; if (q == "bas") { ombres = 0; post = false; } else if (q == "haut") { ombres = 2; post = true; } else { qualite = "moyen"; ombres = 1; post = true; } }

static Col mixc(Col a, Col b, float k) { return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k, 1}; }
void Rendu::maj_jour(float k) {
    nuit_k = k;
    Col s = mixc({.95f, .88f, .72f, 1}, {.20f, .26f, .45f, 1}, k), a = mixc({.66f, .68f, .74f, 1}, {.18f, .20f, .32f, 1}, k);
    brume = mixc({.64f, .74f, .85f, 1}, {.03f, .04f, .09f, 1}, k);
    sky = mixc({.26f, .50f, .90f, 1}, {.015f, .025f, .09f, 1}, k);            // zénith
    disque = mixc({1, .92f, .55f, 1}, {.78f, .84f, 1, 1}, k);
    float sv[3] = {s.r, s.g, s.b}, av[3] = {a.r, a.g, a.b}, fv[4] = {brume.r, brume.g, brume.b, 1};
    SetShaderValue(sh, uSunC, sv, SHADER_UNIFORM_VEC3); SetShaderValue(sh, uAmb, av, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, uFogC, fv, SHADER_UNIFORM_VEC4);
    if (shSky.id) {
        V3 to = norm({.5f, 1.1f, .6f}); float sc[3] = {disque.r, disque.g, disque.b}, hor[3] = {brume.r, brume.g, brume.b}, zen[3] = {sky.r, sky.g, sky.b};
        SetShaderValue(shSky, sSunTo, &to, SHADER_UNIFORM_VEC3); SetShaderValue(shSky, sSunC, sc, SHADER_UNIFORM_VEC3);
        SetShaderValue(shSky, sHor, hor, SHADER_UNIFORM_VEC3); SetShaderValue(shSky, sZen, zen, SHADER_UNIFORM_VEC3); SetShaderValue(shSky, sNuit, &k, SHADER_UNIFORM_FLOAT);
    }
}
void Rendu::set_camera(V3 pos) { SetShaderValue(sh, uCam, &pos, SHADER_UNIFORM_VEC3); }
void Rendu::mode(int m, float flash) {
    if (passe_ombre) m = 4;
    else if (m == 0 && qualite == "bas") m = 3;
    if (m != last_mode) { SetShaderValue(sh, uMode, &m, SHADER_UNIFORM_INT); last_mode = m; }
    if (flash != last_flash) { SetShaderValue(sh, uFlash, &flash, SHADER_UNIFORM_FLOAT); last_flash = flash; }
}
void Rendu::set_spec(float s) { if (s != last_spec) { SetShaderValue(sh, uSpec, &s, SHADER_UNIFORM_FLOAT); last_spec = s; } }

// ---- cibles de rendu
static RenderTexture2D creer_rt(int w, int h, bool profondeur, bool hdr) {
    RenderTexture2D t{}; t.id = rlLoadFramebuffer(); if (!t.id) return t;
    rlEnableFramebuffer(t.id);
    int fmt = hdr ? RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16 : RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    t.texture.id = rlLoadTexture(nullptr, w, h, fmt, 1); t.texture.width = w; t.texture.height = h; t.texture.format = fmt; t.texture.mipmaps = 1;
    rlFramebufferAttach(t.id, t.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (profondeur) { t.depth.id = rlLoadTextureDepth(w, h, true); t.depth.width = w; t.depth.height = h; t.depth.format = 19; t.depth.mipmaps = 1; rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0); }
    bool ok = rlFramebufferComplete(t.id); rlDisableFramebuffer();
    if (!ok) { UnloadRenderTexture(t); return RenderTexture2D{}; }
    SetTextureFilter(t.texture, TEXTURE_FILTER_BILINEAR); SetTextureWrap(t.texture, TEXTURE_WRAP_CLAMP);
    return t;
}
static void liberer(RenderTexture2D& t) { if (t.id) UnloadRenderTexture(t); t = RenderTexture2D{}; }

void Rendu::init_post() {
    shSky = LoadShaderFromMemory(SKY_VS, SKY_FS);
    sSunTo = GetShaderLocation(shSky, "sunTo"); sSunC = GetShaderLocation(shSky, "sunc"); sHor = GetShaderLocation(shSky, "hor"); sZen = GetShaderLocation(shSky, "zen");
    sNuit = GetShaderLocation(shSky, "nuit"); sTime = GetShaderLocation(shSky, "tms");
    Mesh m = GenMeshSphere(1.0f, 24, 32); skyModel = LoadModelFromMesh(m); skyModel.materials[0].shader = shSky;
    shBright = LoadShaderFromMemory(nullptr, BRIGHT_FS); bThr = GetShaderLocation(shBright, "thr");
    shBlur = LoadShaderFromMemory(nullptr, BLUR_FS); blDir = GetShaderLocation(shBlur, "dir");
    shComp = LoadShaderFromMemory(nullptr, COMP_FS); cBloom1 = GetShaderLocation(shComp, "bloom1"); cBloom2 = GetShaderLocation(shComp, "bloom2");
    cExp = GetShaderLocation(shComp, "expo"); cBI = GetShaderLocation(shComp, "bi"); cNuit = GetShaderLocation(shComp, "nuit");
    shFxaa = LoadShaderFromMemory(nullptr, FXAA_FS); fxRes = GetShaderLocation(shFxaa, "res");
    maj_jour(nuit_k);
}
void Rendu::dimensionner(int w, int h) {
    bool veut = post && qualite != "bas";
    if (!veut) { if (post_actif) { liberer(scene); liberer(ldr); liberer(b1a); liberer(b1b); liberer(b2a); liberer(b2b); post_actif = false; rw = rh = 0; } return; }
    if (post_actif && w == rw && h == rh) return;
    liberer(scene); liberer(ldr); liberer(b1a); liberer(b1b); liberer(b2a); liberer(b2b);
    hdr = true; scene = creer_rt(w, h, true, true);
    if (!scene.id) { hdr = false; scene = creer_rt(w, h, true, false); }                       // carte graphique sans HDR : on retombe sur 8 bits
    ldr = creer_rt(w, h, false, false); b1a = creer_rt(w / 2, h / 2, false, hdr); b1b = creer_rt(w / 2, h / 2, false, hdr);
    b2a = creer_rt(w / 4, h / 4, false, hdr); b2b = creer_rt(w / 4, h / 4, false, hdr);
    post_actif = scene.id && ldr.id && b1a.id && b1b.id && b2a.id && b2b.id; rw = w; rh = h;
    if (!post_actif) { liberer(scene); liberer(ldr); liberer(b1a); liberer(b1b); liberer(b2a); liberer(b2b); post = false; }   // pas de post-traitement possible
}
void Rendu::scene_debut() { if (post_actif) BeginTextureMode(scene); int d = post_actif ? 0 : 1; SetShaderValue(sh, uDirect, &d, SHADER_UNIFORM_INT); }
static void passe(RenderTexture2D& dst, const Texture2D& src, int sw, int sh_, Shader* s) {
    BeginTextureMode(dst); if (s) BeginShaderMode(*s);
    DrawTexturePro(src, {0, 0, (float)sw, -(float)sh_}, {0, 0, (float)dst.texture.width, (float)dst.texture.height}, {0, 0}, 0.0f, WHITE);
    if (s) EndShaderMode(); EndTextureMode();
}
void Rendu::scene_fin() {
    if (!post_actif) return;
    EndTextureMode();
    int W_ = scene.texture.width, H_ = scene.texture.height;
    float thr = 1.0f; SetShaderValue(shBright, bThr, &thr, SHADER_UNIFORM_FLOAT);
    passe(b1a, scene.texture, W_, H_, &shBright);
    float dx[2] = {1.0f / b1a.texture.width, 0}, dy[2] = {0, 1.0f / b1a.texture.height};
    SetShaderValue(shBlur, blDir, dx, SHADER_UNIFORM_VEC2); passe(b1b, b1a.texture, b1a.texture.width, b1a.texture.height, &shBlur);
    SetShaderValue(shBlur, blDir, dy, SHADER_UNIFORM_VEC2); passe(b1a, b1b.texture, b1b.texture.width, b1b.texture.height, &shBlur);
    passe(b2a, b1a.texture, b1a.texture.width, b1a.texture.height, nullptr);
    float ex[2] = {1.0f / b2a.texture.width, 0}, ey[2] = {0, 1.0f / b2a.texture.height};
    SetShaderValue(shBlur, blDir, ex, SHADER_UNIFORM_VEC2); passe(b2b, b2a.texture, b2a.texture.width, b2a.texture.height, &shBlur);
    SetShaderValue(shBlur, blDir, ey, SHADER_UNIFORM_VEC2); passe(b2a, b2b.texture, b2b.texture.width, b2b.texture.height, &shBlur);
    float expo = 1.0f + .25f * nuit_k, bi = .55f + .25f * nuit_k;
    SetShaderValue(shComp, cExp, &expo, SHADER_UNIFORM_FLOAT); SetShaderValue(shComp, cBI, &bi, SHADER_UNIFORM_FLOAT); SetShaderValue(shComp, cNuit, &nuit_k, SHADER_UNIFORM_FLOAT);
    BeginTextureMode(ldr); BeginShaderMode(shComp);
    SetShaderValueTexture(shComp, cBloom1, b1a.texture); SetShaderValueTexture(shComp, cBloom2, b2a.texture);
    DrawTexturePro(scene.texture, {0, 0, (float)W_, -(float)H_}, {0, 0, (float)W_, (float)H_}, {0, 0}, 0.0f, WHITE);
    EndShaderMode(); EndTextureMode();
    BeginShaderMode(shFxaa); float res[2] = {(float)W_, (float)H_}; SetShaderValue(shFxaa, fxRes, res, SHADER_UNIFORM_VEC2);
    DrawTexturePro(ldr.texture, {0, 0, (float)W_, -(float)H_}, {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, {0, 0}, 0.0f, WHITE);
    EndShaderMode();
}
void Rendu::ciel(V3 cam) {
    float t = (float)GetTime(); SetShaderValue(shSky, sTime, &t, SHADER_UNIFORM_FLOAT);
    rlDisableBackfaceCulling(); rlDisableDepthMask(); DrawModel(skyModel, cam, 330.0f, WHITE); rlEnableDepthMask(); rlEnableBackfaceCulling();
}

// ---- ombres : carte de profondeur orthographique centrée sur le joueur, stabilisée au texel près
void Rendu::ombre_debut(V3 centre) {
    int res = ombres == 2 ? 2048 : 1024; float ext = ombres == 2 ? 180.0f : 120.0f;
    if (res != shadow_res || !shadow.id) {
        liberer(shadow); shadow.id = rlLoadFramebuffer(); shadow.texture.width = res; shadow.texture.height = res;
        if (shadow.id) { rlEnableFramebuffer(shadow.id); shadow.depth.id = rlLoadTextureDepth(res, res, false); shadow.depth.width = res; shadow.depth.height = res; shadow.depth.format = 19; shadow.depth.mipmaps = 1;
            rlFramebufferAttach(shadow.id, shadow.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0); rlDisableFramebuffer(); }
        shadow_res = res;
    }
    V3 L = norm({-.5f, -1.1f, -.6f}), droite = norm(cross3(L, {0, 1, 0})), haut = cross3(droite, L);
    float texel = ext / res, cx = std::floor(dot(centre, droite) / texel) * texel, cy = std::floor(dot(centre, haut) / texel) * texel;
    V3 c = droite * cx + haut * cy + L * dot(centre, L);
    Camera3D lc{}; lc.position = c - L * 200.0f; lc.target = c; lc.up = {0, 1, 0}; lc.fovy = ext; lc.projection = CAMERA_ORTHOGRAPHIC;
    BeginTextureMode(shadow); ClearBackground(WHITE); BeginMode3D(lc);
    lightVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()); passe_ombre = true; last_mode = -1;
}
void Rendu::ombre_fin() {
    passe_ombre = false; last_mode = -1; EndMode3D(); EndTextureMode();
    int on = ombres, pcf = ombres == 2 ? 2 : 1; float res = (float)shadow_res, nb = 2.4f * (ombres == 2 ? 180.0f : 120.0f) / res;
    SetShaderValueMatrix(sh, uLightVP, lightVP); SetShaderValue(sh, uShadowOn, &on, SHADER_UNIFORM_INT); SetShaderValue(sh, uShadowRes, &res, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, uPcf, &pcf, SHADER_UNIFORM_INT); SetShaderValue(sh, uNBias, &nb, SHADER_UNIFORM_FLOAT);
}
void Rendu::lier_ombre() {
    if (!ombre_active() || !shadow.depth.id) { int zero = 0; SetShaderValue(sh, uShadowOn, &zero, SHADER_UNIFORM_INT); return; }
    rlEnableShader(sh.id); int slot = 10; rlActiveTextureSlot(10); rlEnableTexture(shadow.depth.id); rlSetUniform(uShadowMap, &slot, SHADER_UNIFORM_INT, 1);
}

// ------------------------------------------------------------------ textures procédurales (bruit pavé périodique : se raccordent sans couture)
static inline float h2(int x, int y, uint32_t s) { uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16; return (h & 0xFFFFFF) / 16777215.0f; }
static float vnp(float x, float y, int P, uint32_t s) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y); float fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    auto w = [&](int a) { return ((a % P) + P) % P; };
    float a = h2(w(xi), w(yi), s), b = h2(w(xi + 1), w(yi), s), c = h2(w(xi), w(yi + 1), s), d = h2(w(xi + 1), w(yi + 1), s);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
static float fbmp(float u, float v, int P0, int oct, uint32_t s) { float sum = 0, amp = .5f, tot = 0; int P = P0; for (int o = 0; o < oct; o++) { sum += amp * vnp(u * P, v * P, P, s + o * 17); tot += amp; amp *= .5f; P *= 2; } return sum / tot; }
struct Img {
    int w, h; std::vector<unsigned char> px;
    Img(int w_, int h_) : w(w_), h(h_), px(w_ * h_ * 4, 255) {}
    void put(int x, int y, float r, float g, float b) {
        if (x < 0 || y < 0 || x >= w || y >= h) return; unsigned char* p = &px[(y * w + x) * 4];
        p[0] = (unsigned char)clampf(r, 0, 255); p[1] = (unsigned char)clampf(g, 0, 255); p[2] = (unsigned char)clampf(b, 0, 255); p[3] = 255;
    }
    void get(int x, int y, float& r, float& g, float& b) const { const unsigned char* p = &px[(((y % h) + h) % h * w + ((x % w) + w) % w) * 4]; r = p[0]; g = p[1]; b = p[2]; }
    void mixp(int x, int y, float r, float g, float b, float a) { if (x < 0 || y < 0 || x >= w || y >= h) return; float R_, G_, B_; get(x, y, R_, G_, B_); put(x, y, R_ + (r - R_) * a, G_ + (g - G_) * a, B_ + (b - B_) * a); }
    void rect(int x0, int y0, int x1, int y1, float r, float g, float b) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) put(x, y, r, g, b); }
    void darken(int x0, int y0, int x1, int y1, float k) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) { float r, g, b; if (x < 0 || y < 0 || x >= w || y >= h) continue; get(x, y, r, g, b); put(x, y, r * k, g * k, b * k); } }
};
static void remplir(Img& im, float r, float g, float b, float amp, int P, uint32_t seed, float grain = 6) {       // base bruitée (grandes taches + grain fin)
    for (int y = 0; y < im.h; y++) for (int x = 0; x < im.w; x++) {
        float n = fbmp((float)x / im.w, (float)y / im.h, P, 5, seed), k = (n - .5f) * 2 * amp + (h2(x, y, seed + 5) - .5f) * grain;
        im.put(x, y, r + k, g + k * .98f, b + k * .95f);
    }
}
// fenêtre : type 0 allumée, 1 sombre, 2 vitre claire ; cadre biseauté, meneau, rebord et traînée de salissure dessous
static void fenetre(Img& im, int x0, int y0, int w, int h, int type, uint32_t sd, bool croix, bool rebord = true) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        float t = (float)y / h, u = (float)x / w, n = (h2(x, y, sd) - .5f) * 8, r, g, b;
        if (type == 0) { r = 255 - 17 * t; g = 222 - 46 * t; b = 128 - 40 * t; if (u < .1f || u > .9f) { r *= .78f; g *= .72f; b *= .6f; } }          // rideaux sur les côtés
        else if (type == 1) { r = 16 + 14 * t; g = 18 + 22 * t; b = 24 + 32 * t; }
        else { r = 56 - 20 * t; g = 76 - 26 * t; b = 100 - 34 * t; float diag = std::fabs(u + t * .7f - .55f); if (diag < .12f) { float k = (1 - diag / .12f) * 38; r += k; g += k; b += k * 1.1f; } }
        im.put(x0 + x, y0 + y, r + n, g + n, b + n);
    }
    for (int i = 0; i < 4; i++) { im.rect(x0 - 4 + i, y0 - 4 + i, x0 + w + 3 - i, y0 - 4 + i, 205 - i * 6, 203 - i * 6, 198 - i * 6); im.rect(x0 - 4 + i, y0 - 4 + i, x0 - 4 + i, y0 + h + 3 - i, 195 - i * 6, 193 - i * 6, 188 - i * 6);
        im.rect(x0 - 4 + i, y0 + h + 3 - i, x0 + w + 3 - i, y0 + h + 3 - i, 84 + i * 4, 82 + i * 4, 78 + i * 4); im.rect(x0 + w + 3 - i, y0 - 4 + i, x0 + w + 3 - i, y0 + h + 3 - i, 92 + i * 4, 90 + i * 4, 86 + i * 4); }
    if (croix && type != 0) { im.rect(x0 + w / 2 - 1, y0, x0 + w / 2 + 1, y0 + h - 1, 150, 148, 144); im.rect(x0, y0 + h / 2 - 1, x0 + w - 1, y0 + h / 2 + 1, 150, 148, 144); }
    if (rebord) { im.rect(x0 - 8, y0 + h + 4, x0 + w + 7, y0 + h + 8, 196, 194, 188); im.rect(x0 - 8, y0 + h + 9, x0 + w + 7, y0 + h + 9, 70, 68, 64);
        for (int y = 0; y < 52; y++) for (int x = -6; x < w + 6; x++) { float a = (1 - y / 52.0f) * .26f * (.55f + .9f * vnp((x0 + x) * .21f, y * .09f, 4096, sd + 3)); im.mixp(x0 + x, y0 + h + 10 + y, 30, 28, 26, a); } }
}
Texture2D Rendu::tex(const std::string& nom) {
    auto it = cache.find(nom); if (it != cache.end()) return it->second;
    uint32_t sd = 7;
    auto typefen = [&](int cx, int cy, uint32_t s, float pl, float pd) { float r = h2(cx, cy, s); return r < pl ? 0 : (r < pl + pd ? 1 : 2); };
    Img img(64, 64);
    if (nom == "facade") {                       // béton préfabriqué : 4 colonnes x 4 étages de fenêtres
        img = Img(512, 512); remplir(img, 150, 148, 144, 20, 6, 11, 11);
        for (int cy = 0; cy < 4; cy++) {
            int y = cy * 128; img.rect(0, y + 120, 511, y + 122, 118, 116, 112); img.rect(0, y + 123, 511, y + 123, 176, 174, 170);       // joint de dalle
            for (int cx = 0; cx < 4; cx++) { int ty = typefen(cx, cy, 21, .13f, .11f); fenetre(img, cx * 128 + 26, y + 18, 76, 82, ty, 31 + cx * 7 + cy, h2(cx, cy, 5) < .5f); }
        }
    } else if (nom == "brique") {
        img = Img(512, 512);
        for (int y = 0; y < 512; y++) for (int x = 0; x < 512; x++) {
            int row = y / 16, off = (row % 2) * 16, bx = (x + off) % 32, by = y % 16; float g = h2(x, y, 9) - .5f;
            if (bx < 2 || by < 2) { float m = 110 + g * 14 + (fbmp(x / 512.f, y / 512.f, 12, 3, 4) - .5f) * 16; img.put(x, y, m + 4, m - 2, m - 12); }
            else { float s = h2((x + off) / 32, row, 77), k = (s - .5f), n = (fbmp(x / 512.f, y / 512.f, 8, 4, 8) - .5f) * 26; float edge = (bx < 4 || by < 4) ? 8.0f : 0.0f;
                img.put(x, y, 152 + k * 56 + n + g * 14 + edge, 80 + k * 26 + n * .6f + g * 8 + edge * .7f, 64 + k * 20 + n * .5f + g * 7); }
        }
        for (int cy = 0; cy < 4; cy++) for (int cx = 0; cx < 4; cx++) { int x = cx * 128 + 32, y = cy * 128 + 22; int ty = typefen(cx, cy, 33, .12f, .13f);
            img.rect(x - 8, y - 12, x + 71, y - 6, 218, 208, 192); fenetre(img, x, y, 64, 84, ty, 61 + cx * 5 + cy, true); }
    } else if (nom == "verre") {                  // mur-rideau : panneaux de 64 x 128
        img = Img(512, 512);
        for (int pc = 0; pc < 8; pc++) for (int pr = 0; pr < 4; pr++) {
            int x0 = pc * 64, y0 = pr * 128, ty = typefen(pc, pr, 41, .13f, .15f); float hh = h2(pc, pr, 99);
            for (int y = 0; y < 128; y++) for (int x = 0; x < 64; x++) {
                float t = y / 128.0f, u = x / 64.0f, n = (h2(x0 + x, y0 + y, 13) - .5f) * 7, r, g, b;
                if (ty == 0) { r = 255 - 17 * t; g = 218 - 46 * t; b = 128 - 40 * t; } else if (ty == 1) { r = 18 + 10 * t; g = 24 + 16 * t; b = 36 + 24 * t; }
                else { r = 86 - 40 * t; g = 122 - 52 * t; b = 152 - 56 * t; float diag = std::fabs(u * .8f + t * .9f - hh); if (diag < .11f) { float k = (1 - diag / .11f) * 34; r += k; g += k; b += k * 1.1f; } }
                img.put(x0 + x, y0 + y, r + n, g + n, b + n);
            }
            img.rect(x0, y0, x0 + 63, y0 + 2, 34, 44, 56); img.rect(x0, y0, x0 + 2, y0 + 127, 34, 44, 56); img.rect(x0, y0 + 3, x0 + 63, y0 + 3, 88, 100, 112);
        }
    } else if (nom == "sol") {                    // bitume / terre battue
        img = Img(256, 256); remplir(img, 86, 84, 79, 17, 5, 3, 12);
        for (int k = 0; k < 7; k++) { float x = h2(k, 1, 5) * 256, y = h2(k, 2, 5) * 256, a = h2(k, 3, 5) * 6.28f; for (int s = 0; s < 70; s++) { x += std::cos(a) * 1.5f; y += std::sin(a) * 1.5f; a += (h2(k, s, 8) - .5f) * .7f; img.mixp((int)x, (int)y, 36, 34, 32, .75f); } }
    } else if (nom == "route") {                  // asphalte usé : traces de pneus, lignes écaillées, fissures, taches d'huile
        img = Img(128, 256); remplir(img, 52, 53, 58, 10, 5, 12, 8);
        for (int y = 0; y < 256; y++) for (int x = 0; x < 128; x++) { float tr = 0; for (int c : {40, 88}) tr += std::exp(-((x - c) * (x - c)) / 150.0f); float k = 1 - .16f * tr * (.6f + .8f * fbmp(x / 128.f, y / 256.f, 4, 3, 2)); float r, g, b; img.get(x, y, r, g, b); img.put(x, y, r * k, g * k, b * k); }
        for (int y = 0; y < 256; y++) for (int x = 0; x < 128; x++) {
            float wear = clampf(.55f + .9f * fbmp(x / 128.f, y / 256.f, 8, 4, 40), 0, 1);
            if ((x >= 6 && x <= 10) || (x >= 117 && x <= 121)) img.mixp(x, y, 192, 192, 190, .85f * wear);
            if (x >= 62 && x <= 66 && (y % 64) < 32) img.mixp(x, y, 218, 178, 42, .85f * wear);
        }
        for (int k = 0; k < 5; k++) { float x = h2(k, 1, 6) * 128, y = h2(k, 2, 6) * 256, a = 1.57f + (h2(k, 3, 6) - .5f); for (int s = 0; s < 60; s++) { x += std::cos(a) * 1.6f; y += std::sin(a) * 1.6f; a += (h2(k, s, 9) - .5f) * .8f; img.mixp((int)x, (int)y, 24, 24, 27, .8f); } }
        for (int k = 0; k < 4; k++) { float cx = h2(k, 4, 7) * 128, cy = h2(k, 5, 7) * 256, rr = 8 + h2(k, 6, 7) * 10; for (int y = -20; y <= 20; y++) for (int x = -20; x <= 20; x++) { float d = std::sqrt((float)(x * x + y * y)) / rr; if (d < 1) img.mixp((int)cx + x, (int)cy + y, 24, 24, 30, (1 - d) * .5f); } }
    } else if (nom == "trottoir") {               // dalles de 1,5 m, joints sombres, tons variés
        img = Img(256, 256);
        for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) {
            int sx = x / 64, sy = y / 64, bx = x % 64, by = y % 64; float tone = (h2(sx, sy, 3) - .5f) * 22, g = h2(x, y, 4) - .5f, n = (fbmp(x / 256.f, y / 256.f, 6, 4, 6) - .5f) * 22;
            float base = 128 + tone + n + g * 9; if (bx < 2 || by < 2) base = 78 + g * 8; else if (bx < 4 || by < 4) base += 7;
            img.put(x, y, base, base - 2, base - 7);
        }
    } else if (nom == "herbe") {
        img = Img(256, 256);
        for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) {
            float n = fbmp(x / 256.f, y / 256.f, 4, 5, 15), m = fbmp(x / 256.f, y / 256.f, 8, 3, 25), g = h2(x, y, 6) - .5f, dirt = clampf((m - .6f) * 5, 0, 1);
            float r = 66 + n * 56 + g * 22, gg = 84 + n * 54 + g * 28, b = 40 + n * 28 + g * 12; r += (112 - r) * dirt * .6f; gg += (96 - gg) * dirt * .6f; b += (66 - b) * dirt * .5f;
            img.put(x, y, r, gg, b);
        }
    } else if (nom == "bois") {
        img = Img(128, 128);
        for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
            int pl = x / 16, bx = x % 16; float tone = (h2(pl, 1, 7) - .5f) * 28, grain = (h2(x, y / 5, 9) - .5f) * 18 + (fbmp(x / 128.f, y / 128.f, 8, 3, 3) - .5f) * 22;
            float v = 108 + tone + grain; if (bx < 1 || bx > 14) v *= .55f; img.put(x, y, v, v * .72f, v * .46f);
        }
        for (int pl = 0; pl < 8; pl++) for (int ny : {10, 118}) img.rect(pl * 16 + 7, ny, pl * 16 + 8, ny + 1, 52, 44, 36);
    } else if (nom == "metal") {                  // tôle ondulée peinte, joints, rivets, traînées de rouille
        img = Img(128, 128);
        for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
            float rib = .86f + .14f * std::sin(x * 6.2832f / 8.0f), n = (h2(x, y, 5) - .5f) * 12 + (fbmp(x / 128.f, y / 128.f, 4, 3, 5) - .5f) * 22;
            float rust = clampf((vnp(x * .12f, y * .03f, 16, 7) - .62f) * 4, 0, 1) * .35f; float v = (200 + n) * rib;
            img.put(x, y, v + 40 * rust, v - 16 * rust, v - 52 * rust);
        }
        for (int s : {0, 64}) { img.rect(0, s, 127, s + 1, 104, 104, 106); for (int x = 8; x < 128; x += 16) img.rect(x, s + 4, x + 1, s + 5, 70, 70, 72); }
    } else if (nom == "mur") {                    // mur d'enceinte : panneaux de béton de 4 m, joints, boulons, coulures
        img = Img(512, 512); remplir(img, 140, 138, 134, 18, 5, 17, 10);
        for (int px = 0; px < 4; px++) for (int py = 0; py < 2; py++) { float tone = (h2(px, py, 31) - .5f) * 14; img.darken(px * 128 + 2, py * 256 + 2, px * 128 + 125, py * 256 + 253, 1.0f + tone / 140.0f); }
        for (int x = 0; x < 512; x += 128) { img.rect(x, 0, x + 2, 511, 82, 80, 78); img.rect(x + 3, 0, x + 3, 511, 176, 174, 170); }
        for (int y = 0; y < 512; y += 256) { img.rect(0, y, 511, y + 2, 82, 80, 78); img.rect(0, y + 3, 511, y + 3, 176, 174, 170); }
        for (int px = 0; px < 4; px++) for (int py = 0; py < 2; py++) for (int c = 0; c < 4; c++) { int bx = px * 128 + (c % 2 ? 112 : 14), by = py * 256 + (c / 2 ? 238 : 16); img.rect(bx - 2, by - 2, bx + 2, by + 2, 96, 94, 92); img.rect(bx - 1, by - 1, bx + 1, by + 1, 168, 166, 162); }
        for (int x = 0; x < 512; x++) { float st = clampf((h2(x / 5, 3, 41) - .62f) * 3.2f, 0, 1) * .5f; for (int y = 0; y < 512; y++) { float k = st * (.4f + .6f * vnp(x * .05f, y * .01f, 4096, 9)); if (k > 0) img.mixp(x, y, 40, 38, 36, k); } }
        for (int y = 0; y < 512; y++) for (int x = 0; x < 512; x++) { float g = clampf(1 - (y % 256) / 70.0f, 0, 1) * .0f; (void)g; }
    } else {                                      // "toit" : béton / gravillons (escaliers, dalles, parapets, barrières)
        img = Img(256, 256); remplir(img, 146, 142, 136, 16, 6, 14, 12);
        for (int k = 0; k < 90; k++) img.mixp((int)(h2(k, 1, 9) * 256), (int)(h2(k, 2, 9) * 256), 70, 68, 64, .7f);
        for (int s : {0, 128}) { img.rect(0, s, 255, s, 96, 94, 90); img.rect(s, 0, s, 255, 96, 94, 90); }
    }
    Image im{img.px.data(), img.w, img.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D t = LoadTextureFromImage(im);
    GenTextureMipmaps(&t); SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR); SetTextureFilter(t, TEXTURE_FILTER_ANISOTROPIC_8X); SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
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
static float brillance(const std::string& nom) { if (nom == "verre") return .9f; if (nom == "metal") return .55f; if (nom == "route") return .22f; return 0.0f; }
void Monde::dessiner(const Camera3D& cam, V3 avant) {
    V3 cp = cam.position;
    for (auto& kv : b) {
        Batch& bt = kv.second; R.mode(bt.emissif ? 1 : 0); R.set_spec(brillance(bt.nom));
        for (auto& bk : bt.blocs) {
            Bloc& bl = bk.second; if (!bl.ok) continue;
            V3 d = bl.centre - cp; float l = len(d);
            if (l - bl.rayon > R.fogB + 40) continue;                                 // au-delà du brouillard
            if (l > bl.rayon + 1 && dot(d, avant) < -bl.rayon) continue;              // derrière la caméra
            if (l > bl.rayon + 1) { float c = dot(d, avant) / l, s = bl.rayon / l; if (c < std::cos(rad(75)) - s) continue; }
            DrawModel(bl.model, {0, 0, 0}, 1.0f, WHITE);
        }
    }
    R.set_spec(0);
}
void Monde::dessiner_ombre(V3 centre, float rayon) {              // tous les blocs proches, quel que soit l'angle de vue
    for (auto& kv : b) {
        Batch& bt = kv.second; if (bt.emissif) continue; R.mode(0);
        for (auto& bk : bt.blocs) { Bloc& bl = bk.second; if (!bl.ok) continue; if (dist(bl.centre, centre) - bl.rayon > rayon) continue; DrawModel(bl.model, {0, 0, 0}, 1.0f, WHITE); }
    }
}

// ------------------------------------------------------------------ effets
namespace FX {
float shake = 0;
struct Obj {
    int kind;            // 0 cube, 1 sphère, 2 faisceau
    V3 p0, p1, b; Col c0, c1; float s0, s1, dur, t; int ease;   // ease 0 out_quad, 1 out_expo, 2 linéaire
    V3 scale3_0{0, 0, 0}, scale3_1{0, 0, 0}; bool aplati = false; bool add = false;   // add : mélange additif (lumière)
};
static std::vector<Obj> objs; static int budget = 0;
static std::vector<std::pair<float, int>> rendus;
static bool libre(int n) { if (budget + n > 160) return false; budget += n; rendus.push_back({1.6f, n}); return true; }
static float ease(int e, float t) { if (e == 0) return 1 - (1 - t) * (1 - t); if (e == 1) return t >= 1 ? 1.0f : 1 - std::pow(2.0f, -10 * t); return t; }
static void particule(V3 pos, V3 vec, Col col, float taille, float dur, bool add = false) {
    V3 cible = pos + vec; cible.y -= std::fabs(vec.y) * .3f + .4f;
    objs.push_back({0, pos, cible, {0, 0, 0}, col, col, taille, .001f, dur, 0, 0}); objs.back().add = add;
}
void etincelles(V3 pos, Col col, int n, float vit, float dur, float taille) {
    if (!libre(n)) return;
    for (int i = 0; i < n; i++) { float a = rnd(-1, 1), b = rnd(.2f, 1.2f), c = rnd(-1, 1); particule(pos, V3{a, b, c} * vit, col, taille, dur * rnd(.7f, 1.2f), true); }
}
void sang(V3 pos, int n) {
    if (!libre(n)) return;
    for (int i = 0; i < n; i++) { float a = rnd(-1, 1), b = rnd(.3f, 1.4f), c = rnd(-1, 1); Col co = hsv(0, .9f, rnd(.35f, .6f)); float t = rnd(.05f, .1f); particule(pos, V3{a, b, c} * 2.2f, co, t, .5f); }
}
void flash(V3 pos, float taille) { objs.push_back({1, pos, pos, {0, 0, 0}, {1, .85f, .4f, .9f}, {1, .85f, .4f, .9f}, taille, taille, .05f, 0, 2}); objs.back().add = true; }
void impact(V3 pos) { etincelles(pos, {.8f, .8f, .8f, 1}, 3, 1.6f, .25f, .05f); }
void explosion(V3 pos, float taille, Col col) {
    if (taille < 4) { flash(pos, taille * .5f); etincelles(pos, col, 6, 2.5f, .4f, .1f); return; }
    if (!libre(14)) return;
    objs.push_back({1, pos, pos, {0, 0, 0}, {col.r, col.g, col.b, .9f}, {.25f, .1f, .05f, 0}, .3f, taille, .55f, 0, 1}); objs.back().add = true;
    objs.push_back({1, pos, pos, {0, 0, 0}, {1, 1, .85f, 1}, {1, 1, .8f, 0}, .2f, taille * .55f, .2f, 0, 2}); objs.back().add = true;
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
    objs.push_back({2, a, b, {0, 0, 0}, col, col, ep, ep, dur, 0, 2}); objs.back().add = true;
}
void update(float dt) {
    for (auto& o : objs) o.t += dt;
    objs.erase(std::remove_if(objs.begin(), objs.end(), [](const Obj& o) { return o.t >= o.dur; }), objs.end());
    for (auto& r : rendus) r.first -= dt;
    for (size_t i = 0; i < rendus.size();) if (rendus[i].first <= 0) { budget = std::max(0, budget - rendus[i].second); rendus.erase(rendus.begin() + i); } else i++;
}
static void dessiner_objets(bool additif) {
    for (auto& o : objs) {
        if (o.add != additif) continue;
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
}
void dessiner() {
    rlDisableBackfaceCulling(); rlDisableDepthMask();
    dessiner_objets(false);                                            // fumée, sang, poussière : mélange normal
    BeginBlendMode(BLEND_ADDITIVE); dessiner_objets(true); EndBlendMode();   // feu, étincelles, éclairs, traînées : lumière additive
    rlEnableDepthMask(); rlEnableBackfaceCulling();
}
void vider() { objs.clear(); budget = 0; rendus.clear(); }
}
