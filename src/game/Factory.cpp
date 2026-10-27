#include "Factory.h"
#include "../platform/Audio.h"
#include "../renderer/Props.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include <random>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "../vendor/stb_image.h"

// ── recipes (per machine type; some machines offer a choice) ────────────────
// deploy = -1 for a normal item recipe; else a DKind produced at the machine's
// deploy point (robots walk there; structures appear there).
struct Recipe { int inA,qA,inB,qB,out,qOut; float time; const char* name; int deploy; };
// Miner tiers — a cheap way to pull more ore from the same node (just pick a
// faster tier from the miner's menu; overclock, Satisfactory-style).
static const Recipe MINER_R[] = {
    {-1,0,-1,0, Factory::ORE,1, 1.4f,  "Mine Ore  (x1)", -1},
    {-1,0,-1,0, Factory::ORE,2, 1.4f,  "Overclock  (x2)", -1},
    {-1,0,-1,0, Factory::ORE,3, 1.2f,  "Turbo  (x3)", -1},
};
static const Recipe SMELT_R[] = {{Factory::ORE,1,-1,0, Factory::INGOT,1, 2.0f, "Ore -> Ingot", -1}};
static const Recipe CONS_R[]  = {
    {Factory::INGOT,1,-1,0, Factory::SCREW,2, 1.5f, "Ingot -> 2 Screw", -1},
    {Factory::INGOT,1,-1,0, Factory::PLATE,1, 2.0f, "Ingot -> Plate", -1},
    {Factory::INGOT,1,-1,0, Factory::ROD,  1, 1.2f, "Ingot -> Rod", -1},
};
static const Recipe ASM_R[] = {
    {Factory::PLATE,1, Factory::SCREW,2, Factory::PART,1, 3.0f, "Plate + 2 Screw -> Part", -1},
    {Factory::ROD,1,   Factory::SCREW,1, Factory::PART,1, 2.5f, "Rod + Screw -> Part", -1},
    {Factory::PLATE,2, Factory::SCREW,4, Factory::ROBOT_ITEM,1, 4.0f, "Plate + Screw -> Robot", -1},
    {Factory::PLATE,1, Factory::ROD,1,   -1,0, 3.5f, "Turret", Factory::DK_TURRET},
    {Factory::PLATE,1, Factory::SCREW,2, -1,0, 2.5f, "Mine",  Factory::DK_MINE},
    {Factory::ROD,2,   -1,0, -1,0, 2.0f, "Fence", Factory::DK_FENCE},
    {Factory::ROD,1,   Factory::SCREW,1, -1,0, 2.0f, "Tripwire", Factory::DK_TRIPWIRE},
};
// Barracks: storage depot — belt Robot items in; they're held until a
// Terminal orders them out. No production of its own.
static const Recipe BARR_R[] = {{-1,0,-1,0,-1,0, 999.f, "Robot Storage", -1}};
// Terminal: no production; interacting opens the command map.
static const Recipe TERM_R[] = {{-1,0,-1,0,-1,0, 999.f, "Command robots", -1}};
// Hub: your home base; placing it (re)locates the objective. No recipe.
static const Recipe HUB_R[] = {{-1,0,-1,0,-1,0, 999.f, "Home Base", -1}};
struct RSet{ const Recipe* r; int n; };
static const RSet RSETS[Factory::MTYPE_N] =
    {{MINER_R,3},{SMELT_R,1},{CONS_R,3},{ASM_R,6},{BARR_R,1},{TERM_R,1},{HUB_R,1}};
static const Recipe& mrec(int type,int recipe){ const RSet&s=RSETS[type];
    return s.r[recipe % s.n]; }

static const char* TNAME[Factory::TOOL_N] =
    {"Miner","Smelter","Constructor","Assembler","Barracks","Terminal","Hub","Conveyor"};
static const char* INAME[Factory::ITEM_N] = {"Ore","Ingot","Screw","Plate","Rod","Part","Robot"};
static const glm::vec3 ICOL[Factory::ITEM_N] = {
    {0.50f,0.62f,0.58f},{0.95f,0.6f,0.28f},{0.72f,0.74f,0.8f},
    {0.55f,0.7f,0.9f},{0.85f,0.8f,0.5f},{0.5f,0.9f,0.6f},{0.45f,0.85f,0.95f}};

// ── shader (textured) ───────────────────────────────────────────────────────
static const char* VS = R"(#version 410 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aN; layout(location=2) in vec2 aUV;
uniform mat4 uMVP,uModel; uniform vec2 uUvRep,uUvOff;
out vec3 vN,vW; out vec2 vUV;
void main(){ vN=mat3(uModel)*aN; vW=vec3(uModel*vec4(aPos,1.0));
             vUV=aUV*uUvRep+uUvOff; gl_Position=uMVP*vec4(aPos,1.0); })";
static const char* FS = R"(#version 410 core
in vec3 vN,vW; in vec2 vUV; out vec4 F;
uniform vec3 uColor,uSun,uCam; uniform float uFog,uAlpha; uniform sampler2D uTex;
void main(){ float d=max(dot(normalize(vN),normalize(uSun)),0.0);
  vec3 t=texture(uTex,vUV).rgb; vec3 c=uColor*t*(0.42+0.58*d);
  float dist=length(vW-uCam); float f=exp(-uFog*dist*dist);
  vec3 sky=vec3(0.62,0.66,0.72);
  F=vec4(mix(sky,c,clamp(f,0.0,1.0)),uAlpha); })";
static GLuint comp(GLenum t,const char*s){GLuint x=glCreateShader(t);glShaderSource(x,1,&s,0);glCompileShader(x);return x;}

static GLuint makeTex(const std::vector<uint8_t>& rgba,int w,int h){
    GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D); return t;
}
void Factory::initTextures(){
    const int N=64; std::vector<uint8_t> px(N*N*4);
    auto put=[&](int x,int y,int r,int g,int b){int i=(y*N+x)*4;
        px[i]=r>255?255:r;px[i+1]=g>255?255:g;px[i+2]=b>255?255:b;px[i+3]=255;};
    // Grass floor — use the engine's real grass texture if present
    { int gw,gh,gc; unsigned char* gd=stbi_load("assets/textures/grass.jpg",&gw,&gh,&gc,4);
      if(gd){ glGenTextures(1,&texFloor); glBindTexture(GL_TEXTURE_2D,texFloor);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,gw,gh,0,GL_RGBA,GL_UNSIGNED_BYTE,gd);
        glGenerateMipmap(GL_TEXTURE_2D); stbi_image_free(gd);
      } else { for(int y=0;y<N;y++)for(int x=0;x<N;x++){ int n=(x*17+y*11)%30;
          put(x,y,52+n/2,95+n,48+n/3); } texFloor=makeTex(px,N,N); } }
    for(int y=0;y<N;y++)for(int x=0;x<N;x++){ int n=(x*13+y*29)%22; int base=150+n;
        bool border=(x<3||y<3||x>N-4||y>N-4); if(border)base=base*0.7f; put(x,y,base,base,base);}
    for(int rx=0;rx<2;rx++)for(int ry=0;ry<2;ry++){int cx=rx?N-7:6,cy=ry?N-7:6;
        for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++) if(dx*dx+dy*dy<=4) put(cx+dx,cy+dy,90,92,98);}
    texMetal=makeTex(px,N,N);
    for(int y=0;y<N;y++)for(int x=0;x<N;x++){ int base=60; int cx=x%32;
        int arm=std::abs((y<N/2)?(y-N/4):(3*N/4-y)); bool chev=std::abs(cx-arm-4)<3;
        put(x,y, chev?210:base, chev?180:base, chev?70:base+8);}
    texBelt=makeTex(px,N,N);
}

void Factory::initCube(){
    GLuint v=comp(GL_VERTEX_SHADER,VS),f=comp(GL_FRAGMENT_SHADER,FS);
    prog=glCreateProgram();glAttachShader(prog,v);glAttachShader(prog,f);
    glLinkProgram(prog);glDeleteShader(v);glDeleteShader(f);
    struct Vx{float p[3],n[3],uv[2];};
    struct Face{ glm::vec3 n; glm::vec3 c[4]; };
    const Face F[6]={
        {{ 1,0,0},{{.5,-.5,.5},{.5,-.5,-.5},{.5,.5,-.5},{.5,.5,.5}}},
        {{-1,0,0},{{-.5,-.5,-.5},{-.5,-.5,.5},{-.5,.5,.5},{-.5,.5,-.5}}},
        {{0, 1,0},{{-.5,.5,.5},{.5,.5,.5},{.5,.5,-.5},{-.5,.5,-.5}}},
        {{0,-1,0},{{-.5,-.5,-.5},{.5,-.5,-.5},{.5,-.5,.5},{-.5,-.5,.5}}},
        {{0,0, 1},{{-.5,-.5,.5},{.5,-.5,.5},{.5,.5,.5},{-.5,.5,.5}}},
        {{0,0,-1},{{.5,-.5,-.5},{-.5,-.5,-.5},{-.5,.5,-.5},{.5,.5,-.5}}},
    };
    const glm::vec2 uv[4]={{0,0},{1,0},{1,1},{0,1}};
    std::vector<Vx> vs; int tri[6]={0,1,2,0,2,3};
    for(int fi=0;fi<6;fi++)for(int k=0;k<6;k++){int j=tri[k]; glm::vec3 p=F[fi].c[j];
        vs.push_back({{p.x,p.y,p.z},{F[fi].n.x,F[fi].n.y,F[fi].n.z},{uv[j].x,uv[j].y}});}
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,vs.size()*sizeof(Vx),vs.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vx),(void*)0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vx),(void*)(3*sizeof(float)));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vx),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}
Factory::~Factory(){ if(vao)glDeleteVertexArrays(1,&vao); if(vbo)glDeleteBuffers(1,&vbo);
    if(gVao)glDeleteVertexArrays(1,&gVao); if(gVbo)glDeleteBuffers(1,&gVbo); if(gEbo)glDeleteBuffers(1,&gEbo);
    if(prog)glDeleteProgram(prog);
    if(texFloor)glDeleteTextures(1,&texFloor); if(texMetal)glDeleteTextures(1,&texMetal);
    if(texBelt)glDeleteTextures(1,&texBelt); }

static void drawModel(GLuint prog,const glm::mat4&VP,const glm::mat4&M,glm::vec3 col,
                      GLuint tex,glm::vec2 uvRep,glm::vec2 uvOff,float alpha){
    glm::mat4 MVP=VP*M;
    glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),1,0,glm::value_ptr(MVP));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"),1,0,glm::value_ptr(M));
    glUniform3fv(glGetUniformLocation(prog,"uColor"),1,glm::value_ptr(col));
    glUniform1f(glGetUniformLocation(prog,"uAlpha"),alpha);
    glUniform2fv(glGetUniformLocation(prog,"uUvRep"),1,glm::value_ptr(uvRep));
    glUniform2fv(glGetUniformLocation(prog,"uUvOff"),1,glm::value_ptr(uvOff));
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,tex);
    glUniform1i(glGetUniformLocation(prog,"uTex"),0);
    glDrawArrays(GL_TRIANGLES,0,36);
}
void Factory::cube(const glm::mat4&VP,glm::vec3 c,glm::vec3 h,glm::vec3 col,
                   glm::vec3,float,glm::vec3,float alpha,GLuint tex,glm::vec2 uvRep,glm::vec2 uvOff){
    glm::mat4 M=glm::translate(glm::mat4(1.f),c)*glm::scale(glm::mat4(1.f),h*2.f);
    drawModel(prog,VP,M,col,tex,uvRep,uvOff,alpha);
}
void Factory::cubeYaw(const glm::mat4&VP,glm::vec3 c,glm::vec3 h,float yaw,glm::vec3 col,
                      GLuint tex,glm::vec2 uvRep,float pitch){
    glm::mat4 M=glm::translate(glm::mat4(1.f),c)*glm::rotate(glm::mat4(1.f),yaw,glm::vec3(0,1,0))
               *glm::rotate(glm::mat4(1.f),-pitch,glm::vec3(1,0,0))*glm::scale(glm::mat4(1.f),h*2.f);
    drawModel(prog,VP,M,col,tex,uvRep,{0,0},1.f);
}

// ── Fence pencil / mine brush ──────────────────────────────────────────────
static const float FENCE_SPAN = 3.f;     // max wall length between two posts
bool Factory::placeFencePost(glm::vec2 p){
    if(fenceChainOpen){
        glm::vec2 e{fenceChainEnd.x,fenceChainEnd.z}; float L=glm::length(p-e);
        if(L<0.75f) return true;                       // same spot as the last post
        if(L>FENCE_SPAN){                              // fill the gap: every post joins up
            int n=(int)std::ceil(L/FENCE_SPAN);
            for(int i=1;i<n;i++) if(!placeFencePost(e+(p-e)*((float)i/n))) return false;
        }
    }
    if(fenceBank<=0){ fenceChainOpen=false; return false; }
    Deployable f{}; f.kind=DK_FENCE; f.pos={p.x,0.f,p.y}; f.walking=false;
    f.linked=fenceChainOpen; f.linkPos=fenceChainEnd;
    deploys.push_back(f); fenceBank--;
    fenceChainEnd=f.pos; fenceChainOpen=true;
    return true;
}
int Factory::paintMines(glm::vec2 c,float radius){
    const float SP=4.f;                                // one mine per 4u grid cell
    int placed=0;
    for(float x=std::floor((c.x-radius)/SP)*SP; x<=c.x+radius; x+=SP)
    for(float z=std::floor((c.y-radius)/SP)*SP; z<=c.y+radius; z+=SP){
        if(mineBank<=0) return placed;
        glm::vec2 q{x+SP*0.5f,z+SP*0.5f};
        if(glm::length(q-c)>radius || std::abs(q.x)>mapHalf || std::abs(q.y)>mapHalf) continue;
        bool taken=false;
        for(auto&d:deploys) if(d.kind==DK_MINE && glm::length(glm::vec2(d.pos.x,d.pos.z)-q)<SP*0.9f){ taken=true; break; }
        if(taken) continue;
        Deployable m{}; m.kind=DK_MINE; m.pos={q.x,0.f,q.y}; m.walking=false;
        deploys.push_back(m); mineBank--; placed++;
    }
    return placed;
}
void Factory::pushOutOfFences(glm::vec3& p,float rad) const{
    const float HALF=0.25f;                            // wall half-thickness
    for(auto&f:deploys){ if(f.kind!=DK_FENCE) continue;
        glm::vec2 a{f.pos.x,f.pos.z}, b=f.linked?glm::vec2(f.linkPos.x,f.linkPos.z):a, q{p.x,p.z};
        glm::vec2 ab=b-a; float t=glm::dot(ab,ab)>1e-6f? glm::clamp(glm::dot(q-a,ab)/glm::dot(ab,ab),0.f,1.f):0.f;
        glm::vec2 aw=q-(a+ab*t); float dd=glm::length(aw), minD=HALF+rad;
        if(dd<minD && dd>1e-3f){ glm::vec2 push=(aw/dd)*(minD-dd); p.x+=push.x; p.z+=push.y; }
    }
}

glm::vec3 Factory::footprint(int type) const{
    switch(type){
        case MINER:       return {1.2f,2.6f,1.2f};
        case SMELTER:     return {1.4f,3.0f,1.4f};
        case CONSTRUCTOR: return {1.6f,2.0f,1.1f};
        case ASSEMBLER:   return {2.0f,2.6f,1.5f};
        case BARRACKS:    return {3.5f,3.4f,4.2f};  // large
        case TERMINAL:    return {1.3f,1.8f,1.3f};
        case HUB:         return {4.0f,5.0f,4.0f};   // large home base
        default:          return {2.0f,2.6f,1.5f};
    }
}
void Factory::drawMachine(const glm::mat4&VP,int type,glm::vec3 pos,float yaw,
                          glm::vec3 sun,float fog,glm::vec3 cam,float alpha){
    (void)yaw; glm::vec3 fp=footprint(type); glm::vec3 b{pos.x,baseY(pos,fp.x,fp.z),pos.z}; glm::vec2 rep{2,1};
    switch(type){
        case MINER:
            cube(VP,b+glm::vec3(0,1.0f,0),{1.2f,1.0f,1.2f},{0.55f,0.42f,0.30f},sun,fog,cam,alpha,texMetal,rep);
            cube(VP,b+glm::vec3(0,2.2f,0),{0.35f,0.6f,0.35f},{0.8f,0.55f,0.2f},sun,fog,cam,alpha,texMetal,rep); break;
        case SMELTER:
            cube(VP,b+glm::vec3(0,1.1f,0),{1.4f,1.1f,1.4f},{0.5f,0.52f,0.56f},sun,fog,cam,alpha,texMetal,rep);
            cube(VP,b+glm::vec3(0,2.4f,0),{0.6f,0.5f,0.6f},{1.6f,0.65f,0.2f},sun,fog,cam,alpha,texMetal); break;
        case CONSTRUCTOR:
            cube(VP,b+glm::vec3(0,0.9f,0),{1.6f,0.9f,1.1f},{0.45f,0.55f,0.72f},sun,fog,cam,alpha,texMetal,rep);
            cube(VP,b+glm::vec3(0,1.7f,0),{1.0f,0.25f,0.5f},{0.6f,0.78f,1.0f},sun,fog,cam,alpha,texMetal); break;
        case ASSEMBLER:
            cube(VP,b+glm::vec3(0,1.1f,0),{2.0f,1.1f,1.5f},{0.4f,0.62f,0.52f},sun,fog,cam,alpha,texMetal,{3,1});
            cube(VP,b+glm::vec3(0,2.1f,0),{1.4f,0.4f,1.0f},{0.5f,0.9f,0.75f},sun,fog,cam,alpha,texMetal,{2,1}); break;
        case BARRACKS:
            cube(VP,b+glm::vec3(0,1.5f,0),{3.5f,1.5f,4.2f},{0.42f,0.4f,0.46f},sun,fog,cam,alpha,texMetal,{4,3});
            cube(VP,b+glm::vec3(0,3.1f,0),{3.6f,0.25f,4.3f},{0.5f,0.35f,0.35f},sun,fog,cam,alpha,texMetal,{4,3}); // roof
            cube(VP,b+glm::vec3(0,0.9f,4.1f),{1.0f,0.9f,0.2f},{0.2f,0.22f,0.25f},sun,fog,cam,alpha,texMetal,{1,1}); // door
            cube(VP,b+glm::vec3(0,3.6f,0),{0.6f,0.5f,0.6f},{0.5f,0.75f,0.9f},sun,fog,cam,alpha,texMetal,{1,1}); break; // beacon
        case TERMINAL:
            cube(VP,b+glm::vec3(0,0.5f,0),{1.2f,0.5f,1.2f},{0.35f,0.38f,0.44f},sun,fog,cam,alpha,texMetal,{1,1});
            cube(VP,b+glm::vec3(0,1.2f,0.4f),{1.0f,0.6f,0.15f},{0.3f,0.9f,0.7f},sun,fog,cam,alpha,texMetal,{1,1}); break; // screen
    }
}

// Full node list of a belt at belt height (from → waypoints → to).
static std::vector<glm::vec3> beltNodes(const std::vector<glm::vec3>& machinePos,
                                        int from,int to,const std::vector<glm::vec3>& pts,
                                        float h){
    std::vector<glm::vec3> n;
    n.push_back(machinePos[from]+glm::vec3(0,h,0));
    for(auto&p:pts) n.push_back({p.x,h,p.z});
    n.push_back(machinePos[to]+glm::vec3(0,h,0));
    return n;
}
// World position at fraction t (0..1) along a polyline; also its total length.
static glm::vec3 posAlong(const std::vector<glm::vec3>& n,float t,float* total=nullptr){
    float len=0.f; std::vector<float> seg(n.size()-1);
    for(size_t i=0;i+1<n.size();i++){ seg[i]=glm::length(n[i+1]-n[i]); len+=seg[i]; }
    if(total)*total=len;
    if(len<1e-4f) return n.front();
    float d=t*len;
    for(size_t i=0;i+1<n.size();i++){ if(d<=seg[i]||i+2==n.size()){
        float f=seg[i]>1e-4f?d/seg[i]:0.f; return glm::mix(n[i],n[i+1],f); } d-=seg[i]; }
    return n.back();
}

glm::vec2 Factory::avoid(glm::vec3 pos,glm::vec2 dir) const{
    glm::vec2 p{pos.x,pos.z}, push{0,0};
    for(auto&m:machines){ glm::vec3 h=footprint(m.type);
        glm::vec2 mp{m.pos.x,m.pos.z}; glm::vec2 away=p-mp; float d=glm::length(away);
        float r=std::max(h.x,h.z)+1.6f;
        if(d<r && d>0.01f) push+=(away/d)*((r-d)/r)*1.8f; }
    for(auto&n:nodes){ glm::vec2 np{n.pos.x,n.pos.z}; glm::vec2 away=p-np; float d=glm::length(away);
        if(d<n.r+1.2f && d>0.01f) push+=(away/d)*0.6f; }
    for(auto&t:trees){ glm::vec2 tp{t.x,t.z}; glm::vec2 away=p-tp; float d=glm::length(away);
        if(d<t.y+1.2f && d>0.01f) push+=(away/d)*((t.y+1.2f-d))*1.2f; }
    glm::vec2 out=dir+push;
    float L=glm::length(out); return L>0.01f? out/L : dir;
}

int Factory::cellOf(float wx,float wz) const{
    int cx=(int)((wx+mapHalf)/CELL), cz=(int)((wz+mapHalf)/CELL);
    if(cx<0||cz<0||cx>=gridN||cz>=gridN) return -1;
    return cz*gridN+cx;
}

// ── Terrain: rolling hills ─────────────────────────────────────────────────
float Factory::hillRaw(float x,float z) const{
    float h=0.f;
    for(auto&H:hills){ float dx=x-H.c.x, dz=z-H.c.y;
        float u=(dx*H.cs+dz*H.sn)/H.a, v=(-dx*H.sn+dz*H.cs)/H.b, r2=u*u+v*v;
        if(r2<1.f){ float k=1.f-r2; h+=H.h*k*k; } }          // smooth, compact bump
    if(h>6.f) h=6.f+(h-6.f)*0.35f;                             // overlaps swell, not spike
    float edge=glm::clamp((mapHalf-std::max(std::abs(x),std::abs(z)))/40.f,0.f,1.f);   // level rim
    float home=glm::smoothstep(18.f,45.f,glm::length(glm::vec2(x,z)));                 // level start
    return h*edge*home;
}
float Factory::heightAt(float x,float z) const{
    if(hills.empty() || std::abs(x)>mapHalf || std::abs(z)>mapHalf) return GROUND;   // lobby etc.
    float h=hillRaw(x,z);
    for(auto&n:nodes){ float d=glm::length(glm::vec2(x-n.pos.x,z-n.pos.z)), R=n.r+6.f;   // ore pads
        if(d<R) h=glm::mix(h,n.pos.y-GROUND,1.f-glm::smoothstep(n.r+0.5f,R,d)); }
    return GROUND+h;
}
float Factory::baseY(glm::vec3 p,float hx,float hz) const{
    float y=heightAt(p.x,p.z);    // lowest corner: buildings dig into the uphill side, never float
    for(int sx=-1;sx<=1;sx+=2) for(int sz=-1;sz<=1;sz+=2) y=std::min(y,heightAt(p.x+sx*hx,p.z+sz*hz));
    return y;
}
void Factory::buildGround(){
    const int N=(int)std::ceil(2.f*mapHalf/3.f); const float st=2.f*mapHalf/N;   // ~3u cells
    std::vector<float> H((size_t)(N+1)*(N+1));
    for(int z=0;z<=N;z++) for(int x=0;x<=N;x++) H[(size_t)z*(N+1)+x]=heightAt(-mapHalf+x*st,-mapHalf+z*st);
    auto at=[&](int x,int z){ x=glm::clamp(x,0,N); z=glm::clamp(z,0,N); return H[(size_t)z*(N+1)+x]; };
    struct Vx{float p[3],n[3],uv[2];}; std::vector<Vx> vs; vs.reserve((size_t)(N+1)*(N+1));
    for(int z=0;z<=N;z++) for(int x=0;x<=N;x++){ float wx=-mapHalf+x*st, wz=-mapHalf+z*st;
        glm::vec3 n=glm::normalize(glm::vec3(at(x-1,z)-at(x+1,z),2.f*st,at(x,z-1)-at(x,z+1)));
        vs.push_back({{wx,at(x,z),wz},{n.x,n.y,n.z},{wx/5.f,wz/5.f}}); }
    std::vector<uint32_t> ix; ix.reserve((size_t)N*N*6);
    for(int z=0;z<N;z++) for(int x=0;x<N;x++){ uint32_t a=z*(N+1)+x, b=a+1, c=a+(N+1), d=c+1;
        ix.insert(ix.end(),{a,c,b, b,c,d}); }                  // CCW seen from above
    if(!gVao){ glGenVertexArrays(1,&gVao); glGenBuffers(1,&gVbo); glGenBuffers(1,&gEbo); }
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER,gVbo); glBufferData(GL_ARRAY_BUFFER,vs.size()*sizeof(Vx),vs.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gEbo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,ix.size()*4,ix.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vx),(void*)0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vx),(void*)(3*sizeof(float)));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vx),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glEnableVertexAttribArray(2);
    glBindVertexArray(0); gIdx=(int)ix.size();
}
// Belt polyline draped over the hills: segments split to <=3u, each node on the ground.
static std::vector<glm::vec3> drape(const std::vector<glm::vec3>& in,const Factory& f,float lift){
    std::vector<glm::vec3> out;
    for(size_t i=0;i<in.size();i++){
        if(i){ glm::vec3 a=in[i-1], b=in[i]; float L=glm::length(glm::vec2(b.x-a.x,b.z-a.z));
            int n=(int)std::ceil(L/3.f);
            for(int k=1;k<n;k++){ glm::vec3 p=glm::mix(a,b,(float)k/n); out.push_back({p.x,f.heightAt(p.x,p.z)+lift,p.z}); } }
        out.push_back({in[i].x,f.heightAt(in[i].x,in[i].z)+lift,in[i].z}); }
    return out;
}

void Factory::reset(unsigned seed){
    if(!prog){ initCube(); initTextures(); }
    machines.clear(); belts.clear(); deploys.clear();
    enemies.clear(); tracers.clear(); enemySpawnCd=10.f;
    ebasePos={mapHalf*0.7f,0.f,mapHalf*0.7f};
    ebaseHp=ebaseMax; ebaseAlive=false; won=false; lost=false;   // no AI enemy base — PvP only
    aiEconomy=0.f; gameClock=0.f; enemySpawnCd=6.f;
    opps.clear(); everSawOpp=false; netSendCd=0.f; seedSendCd=0.f; mapSynced=false;
    hubAlive=false; cmdMapOpen=false; fenceBank=0; mineBank=0; turretBank=0; hubBrush=0; fenceChainOpen=false; // hub is placed on drop
    active=true; buildMode=false; selType=0; ghostYaw=0.f; pendingSrc=-1; aimedMachine=-1;
    beltPts.clear();
    menuOpen=false; mapMode=false; pendingRecipe=-1; menuMachine=-1; deployTarget=-1;
    gridN=(int)(mapHalf*2.f/CELL);
    explored.assign((size_t)gridN*gridN, 0);
    genMap(seed);
    phase=P_MODE; multiplayer=false; dropReady=false; lobbyEnter=true; lobbyNearTable=false;
}

// The battlefield from a seed: ore, forests, trees, hills. In a LAN match the
// host's seed is sent to everyone, so all players stand on the same map.
// Only mt19937's raw output is used — the std:: distributions differ between
// Mac and Windows standard libraries, which would split the map per platform.
void Factory::genMap(unsigned seed){
    mapSeed=seed?seed:1234u;
    nodes.clear(); trees.clear(); hills.clear();
    std::mt19937 rng(mapSeed);
    auto r01=[](std::mt19937& g){ return (float)((double)g()/4294967296.0); };
    auto U=[&](){ return -mapHalf+14.f+r01(rng)*(2.f*mapHalf-28.f); };
    int n=36+rng()%22;   // ore nodes scale with the bigger map
    for(int i=0;i<n;i++){ float x=std::round(U()/GRID)*GRID; float z=std::round(U()/GRID)*GRID;
        nodes.push_back({{x,GROUND,z}, 3.5f}); }
    // ── Biome layout: forests (dense trees) + plains (open) + passageways ──
    std::vector<glm::vec3> forests;   // x, radius(y), z
    int nf=5+rng()%4;
    for(int i=0;i<nf;i++){ float x=U(); float r=34.f+(rng()%42); float z=U(); forests.push_back({x,r,z}); }
    std::vector<glm::vec4> paths;      // ax,az,bx,bz — clear corridors through forests
    int np=3+rng()%3;
    for(int i=0;i<np;i++){ float ax=U(); float az=U(); float bx=U(); float bz=U(); paths.push_back({ax,az,bx,bz}); }
    const float PATH_W=9.f;
    auto distSeg=[&](float px,float pz,glm::vec4 s)->float{
        glm::vec2 a{s.x,s.y},b{s.z,s.w},p{px,pz},ab=b-a; float L2=glm::dot(ab,ab);
        float t=L2>1e-3f?glm::clamp(glm::dot(p-a,ab)/L2,0.f,1.f):0.f;
        return glm::length(p-(a+ab*t)); };
    auto rnd=[&](){ return (float)(rng()%10000)/10000.f; };
    int cap=700, tries=0;
    while((int)trees.size()<cap && tries<24000){ tries++;
        float x=U(); float z=U();
        // Inside a forest? Denser toward the centre, thinning at the edge, so
        // forests read as real woods with soft boundaries.
        float dens=-1.f;
        for(auto&f:forests){ float d=glm::length(glm::vec2(x-f.x,z-f.z));
            if(d<f.y){ dens=std::max(dens, 1.f-(d/f.y)); } }
        if(dens<0.f) continue;                                         // plains stay open
        if(rnd() > 0.35f + dens*0.65f) continue;                       // density falloff
        bool onPath=false; for(auto&p:paths) if(distSeg(x,z,p)<PATH_W){ onPath=true; break; }
        if(onPath) continue;                                           // keep passageways clear
        if(glm::length(glm::vec2(x,z))<24.f) continue;                 // player start
        bool clash=false;
        for(auto&nd:nodes) if(glm::length(glm::vec2(x-nd.pos.x,z-nd.pos.z))<5.f){ clash=true; break; }
        for(auto&t:trees) if(glm::length(glm::vec2(x-t.x,z-t.z))<1.6f){ clash=true; break; } // min spacing
        if(clash) continue;
        trees.push_back({x, 1.5f+(rng()%18)/10.f, z}); }
    // ── Rolling hills: soft bumps plus some long ridges that break sightlines ──
    { std::mt19937 hr(mapSeed^0x9E3779B9u);
      auto HU=[&](){ return -mapHalf+40.f+r01(hr)*(2.f*mapHalf-80.f); };
      int nh=55+hr()%15;
      for(int i=0;i<nh;i++){ Hill H; float cx=HU(); float cz=HU(); H.c={cx,cz}; H.a=35.f+r01(hr)*55.f;
          bool ridge=r01(hr)<0.35f;                                          // ~1 in 3 is a long ridge
          H.b=H.a*(ridge? 0.45f+r01(hr)*0.15f : 0.6f+r01(hr)*0.4f);
          float ang=r01(hr)*3.14159f; H.cs=std::cos(ang); H.sn=std::sin(ang);
          H.h=(2.f+r01(hr)*4.5f)*(ridge?0.7f:1.f); hills.push_back(H); } }
    for(auto&nd:nodes) nd.pos.y=GROUND+hillRaw(nd.pos.x,nd.pos.z);   // ore sits on a level pad
    buildGround();
    syncCollision();
    mapDirty=true;   // main rebuilds the tree mesh
}

int Factory::outItem(const Machine& m) const { return mrec(m.type,m.recipe).out; }

void Factory::toggleBuild(){ buildMode=!buildMode; pendingSrc=-1;
    Audio::get().play("interact",0.5f,buildMode?1.4f:0.8f); }
void Factory::selectType(int t){ if(t>=0&&t<TOOL_N){selType=t; pendingSrc=-1;
    Audio::get().play("interact",0.35f,1.1f);} }
void Factory::cycleType(int d){ selType=(selType+d+TOOL_N)%TOOL_N; pendingSrc=-1;
    Audio::get().play("interact",0.35f,1.1f); }
void Factory::rotateGhost(){ ghostYaw+=1.5707963f; }

bool Factory::rayGround(glm::vec3 ro,glm::vec3 rd,glm::vec3&out) const{
    float prev=0.f;                          // march the hills, then bisect the crossing
    for(float t=0.5f;t<=70.f;t+=0.5f){ glm::vec3 p=ro+rd*t;
        if(p.y<=heightAt(p.x,p.z)){ float lo=prev,hi=t;
            for(int i=0;i<8;i++){ float m=(lo+hi)*0.5f; glm::vec3 q=ro+rd*m;
                if(q.y<=heightAt(q.x,q.z)) hi=m; else lo=m; }
            out=ro+rd*hi; return true; }
        prev=t; }
    return false;
}
bool Factory::nodeAt(glm::vec3 p) const{
    for(auto&n:nodes){ glm::vec2 d{p.x-n.pos.x,p.z-n.pos.z}; if(glm::length(d)<n.r) return true; }
    return false;
}
int Factory::pickMachine(glm::vec3 g) const{
    int best=-1; float bd=4.f;
    for(int i=0;i<(int)machines.size();i++){ glm::vec2 d{machines[i].pos.x-g.x,machines[i].pos.z-g.z};
        float dd=glm::length(d); if(dd<bd){bd=dd;best=i;} } return best;
}
int Factory::pickBelt(glm::vec3 g) const{
    int best=-1; float bd=1.2f; glm::vec2 P{g.x,g.z};
    for(int i=0;i<(int)belts.size();i++){ const Belt&b=belts[i];
        if(b.from>=(int)machines.size()||b.to>=(int)machines.size()) continue;
        // check every segment of the belt's polyline
        std::vector<glm::vec2> n; n.push_back({machines[b.from].pos.x,machines[b.from].pos.z});
        for(auto&p:b.pts) n.push_back({p.x,p.z});
        n.push_back({machines[b.to].pos.x,machines[b.to].pos.z});
        for(size_t s=0;s+1<n.size();s++){ glm::vec2 A=n[s],B=n[s+1],AB=B-A;
            float L2=glm::dot(AB,AB); if(L2<1e-3f) continue;
            float t=glm::clamp(glm::dot(P-A,AB)/L2,0.f,1.f);
            float dd=glm::length(P-(A+AB*t)); if(dd<bd){bd=dd;best=i;} } }
    return best;
}

void Factory::eraseMachine(int k){
    if(k<0||k>=(int)machines.size()) return;
    machines.erase(machines.begin()+k);
    for(int i=(int)belts.size()-1;i>=0;i--){
        if(belts[i].from==k||belts[i].to==k){ belts.erase(belts.begin()+i); continue; }
        if(belts[i].from>k) belts[i].from--;
        if(belts[i].to>k)   belts[i].to--;
    }
    if(pendingSrc==k) pendingSrc=-1; else if(pendingSrc>k) pendingSrc--;
    syncCollision();
}

void Factory::syncCollision(){
    if(!collider) return;
    collider->clear();
    for(auto&m:machines){ glm::vec3 h=footprint(m.type);
        float by=baseY(m.pos,h.x,h.z);
        collider->addBox({m.pos.x-h.x, by, m.pos.z-h.z},
                         {m.pos.x+h.x, by+h.y, m.pos.z+h.z}, {0.4f,0.4f,0.4f}, false);
    }
    for(auto&t:trees)   // trunks block the player
    { float ty=heightAt(t.x,t.z);
        collider->addBox({t.x-0.4f, ty-0.5f, t.z-0.4f},{t.x+0.4f, ty+3.f, t.z+0.4f},{0.3f,0.2f,0.1f}, false); }
    if(hubAlive)        // the hub is a solid structure too
    { float hy=baseY(hubPos,4.f,4.f);
        collider->addBox({hubPos.x-4.f, hy, hubPos.z-4.f},{hubPos.x+4.f, hy+5.f, hubPos.z+4.f},{0.4f,0.4f,0.5f}, false); }
    for(auto&d:deploys) if(d.kind==DK_FENCE){  // fence posts + walls block the player as well
        { float fy=heightAt(d.pos.x,d.pos.z);
          collider->addBox({d.pos.x-0.3f, fy-0.5f, d.pos.z-0.3f},{d.pos.x+0.3f, fy+2.2f, d.pos.z+0.3f},{0.35f,0.37f,0.4f}, false); }
        if(!d.linked) continue;                // a wall at any angle = a run of small boxes
        glm::vec3 ab=d.linkPos-d.pos; int n=std::max(1,(int)std::ceil(glm::length(ab)/0.5f));
        for(int i=1;i<n;i++){ glm::vec3 c=d.pos+ab*((float)i/n);
            float cy=heightAt(c.x,c.z);
            collider->addBox({c.x-0.3f, cy-0.5f, c.z-0.3f},{c.x+0.3f, cy+2.2f, c.z+0.3f},{0.35f,0.37f,0.4f}, false); }
    }
    collider->buildMesh();
}

// ── LAN plumbing (N-player free-for-all) ───────────────────────────────────
static const uint16_t IRON_PORT = 49813;

static unsigned randomId(){ std::mt19937 r((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
                            unsigned v=r(); return v?v:1u; }
void Factory::netHost(){ if(net.host(IRON_PORT)){ netActive=true; myId=randomId(); hostIp=Net::localIP(); } }
void Factory::netJoin(const char* ip){ if(net.join(ip?ip:"127.0.0.1",IRON_PORT)){ netActive=true; myId=randomId(); } }
bool Factory::netConnected() const { return net.connected(); }
std::string Factory::netStatus() const { return net.status(); }
Factory::Opp* Factory::findOpp(unsigned id){ for(auto&o:opps) if(o.id==id) return &o; return nullptr; }

// Byte helpers (raw little-endian — LAN peers share byte order in practice).
static void putF(std::vector<uint8_t>& b,float f){ auto* p=(uint8_t*)&f; b.insert(b.end(),p,p+4); }
static void putU(std::vector<uint8_t>& b,unsigned u){ for(int i=0;i<4;i++) b.push_back((uint8_t)(u>>(i*8))); }
static float getF(const uint8_t* p){ float f; memcpy(&f,p,4); return f; }
static unsigned getU(const uint8_t* p){ return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24); }

// Drain inbound messages: 'S' a player's snapshot, 'D' damage to a hub,
// 'K' a mine blast, 'M' the host's map seed. Runs in the front-end too, so a
// joiner learns the map (and where rivals' hubs are) before they drop.
void Factory::netRecv(){
    std::vector<uint8_t> msg;
    while(net.recv(msg)){
        if(msg.empty()) continue;
        if(msg[0]=='M' && msg.size()>=1+4){
            unsigned sd=getU(&msg[1]);
            if(!net.isHost() && phase!=P_PLAY){ if(sd!=mapSeed) genMap(sd); mapSynced=true; }
        } else if(msg[0]=='S' && msg.size()>=1+4+4+1+8+2){
            size_t i=1;
            unsigned id=getU(&msg[i]); i+=4;
            if(id==myId) continue;                      // ignore echoes of myself
            float hp=getF(&msg[i]); i+=4;
            bool alive=msg[i++]!=0;
            glm::vec3 hub{ getF(&msg[i]),0.f,getF(&msg[i+4]) }; i+=8;
            unsigned n=(unsigned)(msg[i]|(msg[i+1]<<8)); i+=2;
            Opp* o=findOpp(id);
            if(!o){ opps.push_back(Opp{id,hub,hp,alive,{},0.f,netClock}); o=&opps.back(); everSawOpp=true; }
            o->hub=hub; o->hp=hp; o->alive=alive; o->lastSeen=netClock;
            o->units.clear();
            for(unsigned k=0;k<n && i+8<=msg.size();k++){ o->units.push_back({getF(&msg[i]),getF(&msg[i+4])}); i+=8; }
            // trailing avatar: x, z, yaw (their player character)
            if(i+12<=msg.size()){ { float ax=getF(&msg[i]), az=getF(&msg[i+4]); o->avatarPos={ax,heightAt(ax,az),az}; } o->avatarYaw=getF(&msg[i+8]); i+=12; }
        } else if(msg[0]=='D' && msg.size()>=1+4+4){
            unsigned target=getU(&msg[1]); float amt=getF(&msg[5]);
            if(target==myId && hubAlive) hubHp-=amt;    // someone's robots hit my hub
        } else if(msg[0]=='K' && msg.size()>=1+8){
            // A peer's mine went off here — cull my robots caught in the blast.
            glm::vec2 bp{getF(&msg[1]),getF(&msg[5])};
            for(int k=(int)deploys.size()-1;k>=0;k--)
                if(deploys[k].kind==DK_ROBOT && glm::length(glm::vec2(deploys[k].pos.x-bp.x,deploys[k].pos.z-bp.y))<5.f)
                    deploys.erase(deploys.begin()+k);
        }
    }
}

// Every frame while networked (front-end and match).
void Factory::netTick(float dt){
    if(!netActive) return;
    net.poll(); netClock+=dt;
    netRecv();
    // Host: announce the map seed ~1/s so every joiner (even a late one) builds the same map.
    if(net.isHost()){ mapSynced=true; seedSendCd-=dt;
        if(seedSendCd<=0.f && net.connected()){ seedSendCd=1.f;
            std::vector<uint8_t> m; m.push_back('M'); putU(m,mapSeed); net.broadcast(m); } }
    // A player we haven't heard from in a while has quit or dropped: remove their
    // hub, robots and avatar (otherwise they linger forever and you can never win).
    for(int k=(int)opps.size()-1;k>=0;k--) if(netClock-opps[k].lastSeen>8.f){
        opps.erase(opps.begin()+k); rivalLeftT=5.f; }
    if(phase!=P_PLAY) return;
    // Send my snapshot + any damage I dealt this window, ~15 Hz.
    netSendCd -= dt;
    if(netSendCd<=0.f && net.connected()){
        netSendCd = 1.f/15.f;
        std::vector<uint8_t> s; s.push_back('S');
        putU(s,myId); putF(s, hubAlive?hubHp:0.f); s.push_back(hubAlive?1:0);
        putF(s,hubPos.x); putF(s,hubPos.z);
        std::vector<const Deployable*> rr;
        for(auto& d:deploys) if(d.kind==DK_ROBOT){ rr.push_back(&d); if(rr.size()>=200) break; }
        unsigned n=(unsigned)rr.size();
        s.push_back((uint8_t)(n&0xFF)); s.push_back((uint8_t)(n>>8));
        for(auto* d:rr){ putF(s,d->pos.x); putF(s,d->pos.z); }
        putF(s,playerCam.x); putF(s,playerCam.z); putF(s,playerYaw);   // my avatar
        net.broadcast(s);
        for(auto& o:opps) if(o.pendingDmg>0.f){
            std::vector<uint8_t> dmsg; dmsg.push_back('D'); putU(dmsg,o.id); putF(dmsg,o.pendingDmg);
            net.broadcast(dmsg); o.pendingDmg=0.f;
        }
        for(auto& bp:pendingKills){   // mine blasts → peers cull robots there
            std::vector<uint8_t> km; km.push_back('K'); putF(km,bp.x); putF(km,bp.y); net.broadcast(km);
        }
        pendingKills.clear();
    }
    // Win check: outlived everyone (last hub standing).
    if(!lost && hubAlive && everSawOpp && !won){
        bool anyAlive=false; for(auto& o:opps) if(o.alive) anyAlive=true;
        if(!anyAlive){ won=true; Audio::get().play("explosion",0.8f); }
    }
}

std::vector<Factory::PeerView> Factory::peerAvatars() const {
    std::vector<PeerView> out;
    for(auto& o:opps) if(o.alive && (o.avatarPos.x!=0.f || o.avatarPos.z!=0.f))
        out.push_back({o.avatarPos, o.avatarYaw});
    return out;
}

// The player's pistol — a hitscan ray from the eye. Hits (nearest along the ray)
// enemies, robots, and opponent hubs, and does damage to whatever it strikes.
void Factory::playerFire(glm::vec3 camPos, glm::vec3 camFwd){
    if(phase!=P_PLAY || lost || won) return;
    glm::vec3 rd = glm::normalize(camFwd);
    const float REACH=140.f, HITR=1.2f;
    float best=REACH; int what=-1, idx=-1;   // what: 0 enemy, 2 opp hub, 3 AI base
    auto rayHit=[&](glm::vec3 c,float r,float& outT)->bool{
        glm::vec3 oc=c-camPos; float t=glm::dot(oc,rd); if(t<0||t>best) return false;
        glm::vec3 p=camPos+rd*t; if(glm::length(p-c)>r) return false; outT=t; return true; };
    float t;
    { glm::vec3 hit; if(rayGround(camPos,rd,hit)) best=std::min(best,glm::length(hit-camPos)); }   // hills block shots
    for(int i=0;i<(int)enemies.size();i++){ glm::vec3 c={enemies[i].pos.x,heightAt(enemies[i].pos.x,enemies[i].pos.z)+1.f,enemies[i].pos.z};
        if(rayHit(c,HITR,t)){ best=t; what=0; idx=i; } }
    for(int i=0;i<(int)opps.size();i++){ if(!opps[i].alive) continue;
        glm::vec3 c={opps[i].hub.x,baseY(opps[i].hub,4.f,4.f)+2.f,opps[i].hub.z};
        if(rayHit(c,3.2f,t)){ best=t; what=2; idx=i; } }
    if(!netActive && ebaseAlive){ glm::vec3 c={ebasePos.x,baseY(ebasePos,4.f,4.f)+2.f,ebasePos.z};
        if(rayHit(c,3.2f,t)){ best=t; what=3; idx=-1; } }
    // Beam starts at the ray-gun muzzle (bottom-right of view), not the eye.
    glm::vec3 wup{0,1,0};
    glm::vec3 right = glm::normalize(glm::cross(rd, wup));
    glm::vec3 muzzle = camPos + rd*0.65f + right*0.245f - wup*0.09f;   // the view model's glowing tip
    tracers.push_back({muzzle, camPos + rd*best, 0.08f, true});
    Audio::get().play("laser",0.4f,1.25f);
    const float DMG=34.f;
    if(what==0)      enemies[idx].hp-=DMG;
    else if(what==2) opps[idx].pendingDmg+=DMG;   // sent to the peer's hub in netTick
    else if(what==3) ebaseHp-=DMG;
}

void Factory::update(float dt,double now,glm::vec3 camPos,glm::vec3 camFwd,
                     bool placeEdge,bool deleteEdge,bool interactEdge,
                     float* playerHp){
    if(!active) return; (void)now;
    if(netActive && phase!=P_PLAY) netTick(dt);  // accept joiners, sync the map, track rivals
    if(phase==P_LOBBY){          // walkable staging room
        glm::vec3 c=lobbyCenter();
        float dx=camPos.x-c.x, dz=camPos.z-c.z;
        lobbyNearTable = (dx*dx+dz*dz) < 20.f;   // ~4.5u of the war table
        if(lobbyNearTable && interactEdge){ phase=P_DROP; Audio::get().play("interact",0.6f,1.1f); }
        return;
    }
    if(phase!=P_PLAY) return;   // frozen ImGui screens (mode / drop)
    playerCam = camPos;         // remember where the player is (for avatar sync)
    playerYaw = std::atan2(camFwd.x, camFwd.z);
    netTick(dt);                // exchange snapshots with the LAN peer
    if(lost) return;            // defeated — everything freezes
    glm::vec3 g; bool onGround=rayGround(camPos,glm::normalize(camFwd),g);

    // Reveal the map around the player (fog-of-war exploration)
    int pc=cellOf(camPos.x,camPos.z);
    int R=(int)(12.f/CELL);
    int px=(int)((camPos.x+mapHalf)/CELL), pz=(int)((camPos.z+mapHalf)/CELL);
    for(int dz=-R;dz<=R;dz++)for(int dx=-R;dx<=R;dx++){ int cx=px+dx,cz=pz+dz;
        if(cx>=0&&cz>=0&&cx<gridN&&cz<gridN && dx*dx+dz*dz<=R*R) explored[cz*gridN+cx]=1; }
    (void)pc;

    // Aimed machine (for interact/recipe), within reach of the player
    aimedMachine=-1;
    if(onGround){ int pk=pickMachine(g);
        if(pk>=0){ glm::vec2 d{machines[pk].pos.x-camPos.x,machines[pk].pos.z-camPos.z};
            if(glm::length(d)<6.f) aimedMachine=pk; } }
    // E near the Hub toggles the top-down tactical view; otherwise E opens the
    // recipe popup for the aimed machine (Satisfactory-style).
    bool nearHub = hubAlive && glm::length(glm::vec2(camPos.x-hubPos.x,camPos.z-hubPos.z))<7.f;
    if(interactEdge){
        if(cmdMapOpen){ cmdMapOpen=false; }
        else if(menuOpen){ menuOpen=false; mapMode=false; menuMachine=-1; }
        else if(nearHub){ cmdMapOpen=true; Audio::get().play("interact",0.6f,1.2f); }
        else if(aimedMachine>=0){ menuOpen=true; mapMode=false; menuMachine=aimedMachine;
            Audio::get().play("interact",0.5f,1.2f); }
    }

    ghostValid=false;
    if(buildMode && !menuOpen && onGround){
        g.x=std::round(g.x/GRID)*GRID; g.z=std::round(g.z/GRID)*GRID; g.y=0.f;
        ghostPos=g;
        if(selType==CONVEYOR){
            int pick=pickMachine(g);
            ghostValid = (pendingSrc<0) ? (pick>=0) : true;
            if(placeEdge){
                if(pendingSrc<0){                     // start on a source machine
                    if(pick>=0){ pendingSrc=pick; beltPts.clear();
                        Audio::get().play("interact",0.4f,1.3f); }
                } else if(pick>=0 && pick!=pendingSrc && outItem(machines[pendingSrc])>=0){
                    // finish belt into the destination machine
                    belts.push_back({pendingSrc,pick,outItem(machines[pendingSrc]),beltPts,{},0.f});
                    Audio::get().play("melee_hit",0.5f,1.2f); pendingSrc=-1; beltPts.clear();
                } else if(pick<0){                    // empty ground → corner waypoint
                    beltPts.push_back(g); Audio::get().play("interact",0.35f,1.6f);
                }
            }
            if(deleteEdge && pendingSrc>=0){          // RMB cancels the belt being drawn
                if(!beltPts.empty()) beltPts.pop_back();   // undo last corner
                else pendingSrc=-1;
                Audio::get().play("crash",0.3f,1.5f);
            }
        } else {
            bool clear=true;
            for(auto&m:machines){ glm::vec2 d{m.pos.x-g.x,m.pos.z-g.z};
                if(glm::length(d)<2.8f){clear=false;break;} }
            bool nodeOk=(selType!=MINER)||nodeAt(g);
            bool buildable=(selType!=HUB);   // the Hub is premade at spawn, not built
            ghostValid=clear&&nodeOk&&buildable&&std::abs(g.x)<mapHalf&&std::abs(g.z)<mapHalf;
            if(placeEdge && ghostValid){
                Machine m{}; m.type=selType; m.recipe=0; m.pos=ghostPos; m.yaw=ghostYaw;
                m.prog=0.f; m.onNode=(selType==MINER)&&nodeAt(ghostPos); m.out=0;
                for(int i=0;i<ITEM_N;i++) m.in[i]=0;
                machines.push_back(m); syncCollision();
                Audio::get().play("melee_hit",0.5f,0.9f);
            }
        }
        // Dismantle: delete the aimed machine (or belt) with RMB — but not
        // while drawing a conveyor (there RMB undoes the last corner).
        if(deleteEdge && !(selType==CONVEYOR && pendingSrc>=0)){
            int pm=pickMachine(g);
            if(pm>=0){ eraseMachine(pm); Audio::get().play("crash",0.4f,1.3f); }
            else { int pb=pickBelt(g);
                if(pb>=0){ belts.erase(belts.begin()+pb); Audio::get().play("crash",0.4f,1.4f); } }
        }
    }

    // Machines process from local buffers
    for(auto&m:machines){ const Recipe&r=mrec(m.type,m.recipe);
        if(m.type==TERMINAL||m.type==BARRACKS) continue;   // no production (order/storage)
        if(m.type==MINER){
            if(m.onNode && m.out<CAP){ m.prog+=dt; if(m.prog>=r.time){ m.out+=r.qOut; m.prog=0.f; } }
        } else {
            bool haveIn=(r.inA<0||m.in[r.inA]>=r.qA)&&(r.inB<0||m.in[r.inB]>=r.qB);
            // Fences, mines and turrets stockpile into banks, then get painted
            // from the hub command map. Robots/tripwires still deploy directly.
            int* bank = r.deploy==DK_FENCE?&fenceBank : r.deploy==DK_MINE?&mineBank
                      : r.deploy==DK_TURRET?&turretBank : nullptr;
            bool stock = (bank!=nullptr);
            bool deployable = r.deploy>=0 && !stock;
            bool ready = stock       ? (haveIn && *bank<120)
                       : deployable  ? (haveIn && m.hasDeploy)
                                     : (haveIn && m.out<CAP);
            if(ready){ m.prog+=dt;
                if(m.prog>=r.time){
                    if(r.inA>=0)m.in[r.inA]-=r.qA; if(r.inB>=0)m.in[r.inB]-=r.qB;
                    if(stock){ (*bank)++; }
                    else if(deployable){
                        Deployable d{}; d.kind=r.deploy; d.goal=m.deployPt;
                        if(r.deploy==DK_ROBOT){ d.pos=m.pos; d.walking=true; }
                        else if(r.deploy==DK_MINE || r.deploy==DK_TRIPWIRE){
                            // Lay out a field: fill a grid around the deploy point
                            const int GW=6; const float SP=3.5f;
                            int k=m.deployCount; int gx=k%GW, gz=k/GW;
                            d.pos=m.deployPt + glm::vec3((gx-(GW-1)*0.5f)*SP, 0.f, gz*SP);
                            d.walking=false; m.deployCount++;
                        } else { d.pos=m.deployPt; d.walking=false; }
                        d.t=0.f; deploys.push_back(d);
                        Audio::get().play("melee_hit",0.5f,0.7f);
                    } else if(r.out==PART) partsBank+=r.qOut;   // Parts = currency
                    else m.out+=r.qOut;
                    m.prog=0.f;
                }
            } else m.prog=std::min(m.prog,r.time);
        }
    }
    // ── Combat ────────────────────────────────────────────────────────────
    const float ROBOT_SPD=5.f, R_RANGE=22.f, T_RANGE=18.f;   // bigger robot aggro zone
    auto nearestEnemy=[&](glm::vec3 p,float range)->int{
        int best=-1; float bd=range;
        for(int i=0;i<(int)enemies.size();i++){ float d=glm::length(
            glm::vec2(enemies[i].pos.x-p.x,enemies[i].pos.z-p.z)); if(d<bd){bd=d;best=i;} }
        return best; };
    auto shoot=[&](glm::vec3 from,glm::vec3 to,bool friendly){
        tracers.push_back({{from.x,heightAt(from.x,from.z)+1.1f,from.z},{to.x,heightAt(to.x,to.z)+1.1f,to.z},0.09f,friendly});
        Audio::get().play("laser",0.3f,friendly?1.1f:0.7f); };

    // Hostile bases the robots fight: the single AI base offline, or every live
    // opponent hub in a net free-for-all. opp<0 means the AI enemy base.
    struct HBase { glm::vec3 pos; int opp; };
    std::vector<HBase> hbases;
    if(netActive){ for(size_t k=0;k<opps.size();k++) if(opps[k].alive) hbases.push_back({opps[k].hub,(int)k}); }
    else if(ebaseAlive) hbases.push_back({ebasePos,-1});
    auto nearestBase=[&](glm::vec3 p,float range)->int{ int best=-1; float bd=range;
        for(int i=0;i<(int)hbases.size();i++){ float d=glm::length(glm::vec2(hbases[i].pos.x-p.x,hbases[i].pos.z-p.z));
            if(d<bd){bd=d;best=i;} } return best; };
    auto hitBase=[&](int hi,float amt){ if(hbases[hi].opp<0) ebaseHp-=amt; else opps[hbases[hi].opp].pendingDmg+=amt; };

    for(size_t di=0;di<deploys.size();di++){ Deployable&d=deploys[di]; d.t+=dt; d.fireCd-=dt;
        if(d.kind==DK_ROBOT){
            // Advance on the ordered point; home on the nearest enemy near it,
            // or the nearest hostile hub if commanded there — never stop to fight.
            glm::vec3 mt=d.goal; int nearG=nearestEnemy(d.goal,40.f);   // seek enemies from farther
            int nbG = nearG<0 ? nearestBase(d.goal,16.f) : -1;
            if(nearG>=0) mt=enemies[nearG].pos;
            else if(nbG>=0) mt=hbases[nbG].pos;
            glm::vec2 to{mt.x-d.pos.x,mt.z-d.pos.z}; float dist=glm::length(to);
            // Formation: separate from nearby friendly robots so they spread
            glm::vec2 sep{0,0};
            for(size_t j=0;j<deploys.size();j++){ if(j==di||deploys[j].kind!=DK_ROBOT) continue;
                glm::vec2 aw{d.pos.x-deploys[j].pos.x,d.pos.z-deploys[j].pos.z};
                float dd=glm::length(aw); if(dd<2.0f&&dd>0.01f) sep+=(aw/dd)*((2.0f-dd)/2.0f); }
            if(dist>1.4f){ glm::vec2 dir=avoid(d.pos,to/dist)+sep*0.9f;
                float L=glm::length(dir); if(L>0.01f) dir/=L;
                d.pos.x+=dir.x*ROBOT_SPD*dt; d.pos.z+=dir.y*ROBOT_SPD*dt; d.walking=true; }
            else { d.walking=false;
                if(glm::length(sep)>0.01f){ d.pos.x+=sep.x*ROBOT_SPD*0.5f*dt; d.pos.z+=sep.y*ROBOT_SPD*0.5f*dt; } }
            int t=nearestEnemy(d.pos,R_RANGE);   // shoot on the move
            if(t>=0 && d.fireCd<=0.f){ enemies[t].hp-=13.f; d.fireCd=0.65f; shoot(d.pos,enemies[t].pos,true); }
            else if(t<0 && d.fireCd<=0.f){ int hb=nearestBase(d.pos,R_RANGE);
                if(hb>=0){ hitBase(hb,13.f); d.fireCd=0.65f; shoot(d.pos,hbases[hb].pos,true); } }
        } else if(d.kind==DK_TURRET){
            int t=nearestEnemy(d.pos,T_RANGE);
            if(t>=0 && d.fireCd<=0.f){ enemies[t].hp-=16.f; d.fireCd=0.45f; shoot(d.pos,enemies[t].pos,true); }
        } else if(d.kind==DK_MINE){
            // Trips for ANY hostile robot — AI enemies or an opponent's robots —
            // but never the deployer's own units.
            glm::vec2 mp{d.pos.x,d.pos.z};
            bool trip = (nearestEnemy(d.pos,3.2f)>=0);
            if(!trip) for(auto&o:opps){ for(auto&u:o.units) if(glm::length(u-mp)<3.2f){ trip=true; break; } if(trip)break; }
            if(trip){
                for(auto&e:enemies) if(glm::length(glm::vec2(e.pos.x-mp.x,e.pos.z-mp.y))<5.f) e.hp-=90.f;
                if(netActive) pendingKills.push_back(mp);   // ask peers to cull robots at the blast
                Audio::get().play("explosion",0.7f); d.hp=-1.f;
            }
        } else if(d.kind==DK_TRIPWIRE){
            int t=nearestEnemy(d.pos,1.8f);
            if(t>=0){ enemies[t].hp-=30.f; Audio::get().play("crash",0.4f,1.4f); d.hp=-1.f; }
        }
    }

    // Hard collisions so robots hold a real formation instead of overlapping:
    // push apart any two robots that occupy the same space, then push them out
    // of trees and buildings.
    const float R_RAD=0.65f;
    for(int iter=0; iter<2; iter++)
      for(size_t i=0;i<deploys.size();i++){ if(deploys[i].kind!=DK_ROBOT) continue;
        for(size_t j=i+1;j<deploys.size();j++){ if(deploys[j].kind!=DK_ROBOT) continue;
            glm::vec2 a{deploys[i].pos.x,deploys[i].pos.z}, b{deploys[j].pos.x,deploys[j].pos.z};
            glm::vec2 d=a-b; float dd=glm::length(d); float minD=R_RAD*2.f;
            if(dd<minD && dd>1e-3f){ glm::vec2 push=(d/dd)*((minD-dd)*0.5f);
                deploys[i].pos.x+=push.x; deploys[i].pos.z+=push.y;
                deploys[j].pos.x-=push.x; deploys[j].pos.z-=push.y; } } }
    for(auto&d:deploys){ if(d.kind!=DK_ROBOT) continue;
        for(auto&t:trees){ glm::vec2 aw{d.pos.x-t.x,d.pos.z-t.z}; float dd=glm::length(aw);
            float minD=t.y+R_RAD; if(dd<minD&&dd>1e-3f){ glm::vec2 p=(aw/dd)*(minD-dd); d.pos.x+=p.x; d.pos.z+=p.y; } }
        for(auto&m:machines){ glm::vec3 h=footprint(m.type);
            glm::vec2 aw{d.pos.x-m.pos.x,d.pos.z-m.pos.z}; float dd=glm::length(aw);
            float minD=std::max(h.x,h.z)+R_RAD; if(dd<minD&&dd>1e-3f){ glm::vec2 p=(aw/dd)*(minD-dd); d.pos.x+=p.x; d.pos.z+=p.y; } }
        pushOutOfFences(d.pos,R_RAD); }   // fences are walls that block robots

    // Keep enemies from stacking on top of each other (soft separation).
    { const float E_RAD=0.7f;
      for(size_t i=0;i<enemies.size();i++)
        for(size_t j=i+1;j<enemies.size();j++){
            glm::vec2 a{enemies[i].pos.x,enemies[i].pos.z}, b{enemies[j].pos.x,enemies[j].pos.z};
            glm::vec2 dv=a-b; float dd=glm::length(dv); float minD=E_RAD*2.f;
            if(dd<minD && dd>1e-3f){ glm::vec2 push=(dv/dd)*((minD-dd)*0.5f);
                enemies[i].pos.x+=push.x; enemies[i].pos.z+=push.y;
                enemies[j].pos.x-=push.x; enemies[j].pos.z-=push.y; } } }

    // Enemies march on the Hub (their objective), hitting the player, units,
    // and buildings that get in the way.
    glm::vec3 base = hubAlive ? hubPos : (machines.empty()? glm::vec3(0):machines[0].pos);
    glm::vec3 pfeet{camPos.x,0.f,camPos.z};
    for(auto&e:enemies){ e.fireCd-=dt;
        float pd = glm::length(glm::vec2(pfeet.x-e.pos.x,pfeet.z-e.pos.z));
        float hd = hubAlive? glm::length(glm::vec2(hubPos.x-e.pos.x,hubPos.z-e.pos.z)) : 1e9f;
        int bestD=-1; float bd=24.f;
        for(int i=0;i<(int)deploys.size();i++){ if(deploys[i].kind!=DK_ROBOT&&deploys[i].kind!=DK_TURRET) continue;
            float dd=glm::length(glm::vec2(deploys[i].pos.x-e.pos.x,deploys[i].pos.z-e.pos.z)); if(dd<bd){bd=dd;bestD=i;} }
        int bestM=-1; float bm=24.f;
        for(int i=0;i<(int)machines.size();i++){ float dd=glm::length(glm::vec2(machines[i].pos.x-e.pos.x,machines[i].pos.z-e.pos.z));
            if(dd<bm){bm=dd;bestM=i;} }
        // priority: player if reasonably close, else nearest unit, else building
        int tgt = 0;  // 0=player 1=unit 2=building
        if(bestD>=0 && bd<pd && (bestM<0||bd<=bm)) tgt=1;
        else if(bestM>=0 && bm<pd && (bestD<0||bm<bd)) tgt=2;
        else if(pd<24.f) tgt=0; else if(bestD>=0) tgt=1; else if(bestM>=0) tgt=2; else tgt=-1;
        glm::vec3 mt = tgt==0? pfeet : tgt==1? deploys[bestD].pos : tgt==2? machines[bestM].pos : base;
        glm::vec2 to{mt.x-e.pos.x,mt.z-e.pos.z}; float dist=glm::length(to);
        if(dist>2.2f){ glm::vec2 dir=avoid(e.pos,to/dist); e.pos.x+=dir.x*3.2f*dt; e.pos.z+=dir.y*3.2f*dt; }
        pushOutOfFences(e.pos,0.7f);   // fence walls stop the advance
        if(e.fireCd<=0.f){
            if(tgt==0 && pd<11.f && playerHp){ *playerHp-=8.f; e.fireCd=0.9f; shoot(e.pos,pfeet+glm::vec3(0,0.6f,0),false); }
            else if(tgt==1 && bd<11.f){ deploys[bestD].hp-=10.f; e.fireCd=0.9f; shoot(e.pos,deploys[bestD].pos,false); }
            else if(tgt==2 && bm<9.f){ machines[bestM].hp-=8.f; e.fireCd=1.0f; shoot(e.pos,machines[bestM].pos,false); }
            else if(hubAlive && hd<10.f){ hubHp-=9.f; e.fireCd=1.0f; shoot(e.pos,hubPos+glm::vec3(0,2.f,0),false); }
        }
    }
    // The player can be downed but isn't the objective: main respawns them at
    // the hub when health hits 0. Only your HUB being destroyed loses the match.
    if(playerHp && *playerHp<0.f) *playerHp=0.f;
    if(hubAlive && hubHp<=0.f){ hubAlive=false; if(!won) lost=true;   // your hub gone = you're out
        Audio::get().play("explosion",0.9f); }

    // Remove the dead and used-up; decay tracers
    for(int i=(int)enemies.size()-1;i>=0;i--) if(enemies[i].hp<=0.f){ enemies.erase(enemies.begin()+i);
        Audio::get().play("enemy_die",0.5f); }
    for(int i=(int)deploys.size()-1;i>=0;i--) if(deploys[i].hp<=0.f) deploys.erase(deploys.begin()+i);
    for(int i=(int)machines.size()-1;i>=0;i--) if(machines[i].hp<=0.f){ eraseMachine(i);
        Audio::get().play("crash",0.6f); }
    for(int i=(int)tracers.size()-1;i>=0;i--){ tracers[i].t-=dt; if(tracers[i].t<=0.f) tracers.erase(tracers.begin()+i); }

    // Enemy base destroyed = victory; stops the waves
    if(ebaseAlive && ebaseHp<=0.f){ ebaseAlive=false; won=true; Audio::get().play("explosion",0.8f); }

    // ── AI opponent economy (acts like a player building up) ──────────────
    // A peace phase lets you establish your factory; then the opponent's
    // "economy" grows, sending larger, more frequent waves the longer the
    // match runs. (A networked opponent would replace this with real orders.)
    gameClock += dt;
    if(rivalLeftT>0.f) rivalLeftT-=dt;
    if(!netActive && ebaseAlive && gameClock >= peaceTime){   // real peer replaces the AI in net matches
        aiEconomy += dt;
        enemySpawnCd -= dt;
        float interval = std::max(6.f, 16.f - aiEconomy*0.05f);   // ramps up
        int   cap      = std::min(40, 8 + (int)(aiEconomy/12.f));
        if(enemySpawnCd<=0.f && (int)enemies.size()<cap){
            std::mt19937 rng((unsigned)(now*1000)+enemies.size());
            int grp = 3 + (int)(aiEconomy/25.f) + rng()%3;         // bigger over time
            for(int k=0;k<grp;k++){ Enemy e; e.pos=ebasePos+glm::vec3((rng()%80)/10.f-4,0,(rng()%80)/10.f-4);
                e.goal=base; enemies.push_back(e); }
            enemySpawnCd=interval;
        }
    }
    // Belts carry items along their (possibly cornered) polyline path
    const float BELT_SPEED=4.f;   // world units / second
    std::vector<glm::vec3> mpos; mpos.reserve(machines.size());
    for(auto&m:machines) mpos.push_back(m.pos);
    for(auto&b:belts){
        if(b.from>=(int)machines.size()||b.to>=(int)machines.size()) continue;
        Machine&src=machines[b.from]; Machine&dst=machines[b.to];
        if(b.item<0){ b.prog.clear(); continue; }
        auto nodes=drape(beltNodes(mpos,b.from,b.to,b.pts,0.f),*this,0.35f);
        float total; posAlong(nodes,0.f,&total);
        float rate = total>0.1f ? BELT_SPEED/total : 1.f;
        for(auto&p:b.prog) p+=rate*dt;
        // deliver into the destination machine's input buffer (auto-merge:
        // several belts can feed one machine; auto-split: several belts can
        // pull from one machine's output below)
        for(int i=(int)b.prog.size()-1;i>=0;i--){
            if(b.prog[i]>=1.f){ if(dst.in[b.item]<CAP){ dst.in[b.item]++; b.prog.erase(b.prog.begin()+i);} else b.prog[i]=1.f; } }
        float minP=2.f; for(float p:b.prog) minP=std::min(minP,p);
        float gap = total>0.1f ? 0.8f/total : 0.13f;   // ~0.8u spacing
        b.feed-=dt;
        if(src.out>0 && b.feed<=0.f && (b.prog.empty()||minP>gap)){ src.out--; b.prog.push_back(0.f); b.feed=0.16f; }
    }
}

void Factory::render(const glm::mat4&VP,glm::vec3 sun,float fog,glm::vec3 cam,double now){
    if(!active||!prog) return;
    lastVP=VP;   // the HUD projects rival-hub markers with it
    glUseProgram(prog);
    glUniform3fv(glGetUniformLocation(prog,"uSun"),1,glm::value_ptr(sun));
    glUniform1f(glGetUniformLocation(prog,"uFog"),fog);
    glUniform3fv(glGetUniformLocation(prog,"uCam"),1,glm::value_ptr(cam));
    glBindVertexArray(vao);

    // Front-end: render the walkable lobby room instead of the battlefield.
    if(phase!=P_PLAY){
        glm::vec3 c=lobbyCenter(); const float H=14.f;
        cube(VP,c+glm::vec3(0,-0.5f,0),{H,0.5f,H},{0.6f,0.62f,0.68f},sun,fog,cam,1.f,texFloor,{6,6});      // floor
        cube(VP,c+glm::vec3(0,6.f,0),{H,0.4f,H},{0.3f,0.32f,0.38f},sun,fog,cam,1.f,texMetal,{6,6});         // ceiling
        for(int s=0;s<4;s++){ float a=s*1.5707963f; glm::vec3 n{std::sin(a),0,std::cos(a)};
            cube(VP,c+n*H+glm::vec3(0,3.f,0),{std::abs(n.z)>0.5f?H:0.4f,3.f,std::abs(n.x)>0.5f?H:0.4f},
                 {0.34f,0.36f,0.42f},sun,fog,cam,1.f,texMetal,{6,3}); }
        // War table (glowing holo top) — walk up and press E
        cube(VP,c+glm::vec3(0,0.8f,0),{2.2f,0.8f,2.2f},{0.28f,0.3f,0.36f},sun,fog,cam,1.f,texMetal,{2,1});
        float pulse=0.6f+0.4f*std::sin((float)now*2.f);
        cube(VP,c+glm::vec3(0,1.65f,0),{2.0f,0.08f,2.0f},{0.2f*pulse,0.9f*pulse,1.0f*pulse},sun,fog,cam,1.f,texMetal,{1,1});
        // corner light pillars
        for(int s=0;s<4;s++){ float a=0.785398f+s*1.5707963f;
            cube(VP,c+glm::vec3(std::sin(a)*(H-1.5f),3.f,std::cos(a)*(H-1.5f)),{0.4f,3.f,0.4f},{0.5f,0.7f,0.9f},sun,fog,cam,1.f,texMetal,{1,3}); }
        glBindVertexArray(0);
        return;
    }

    if(gVao){                       // the hilly battlefield floor
        glm::mat4 I(1.f);
        glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),1,0,glm::value_ptr(VP));
        glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"),1,0,glm::value_ptr(I));
        glUniform3f(glGetUniformLocation(prog,"uColor"),1.f,1.f,1.f);
        glUniform1f(glGetUniformLocation(prog,"uAlpha"),1.f);
        glUniform2f(glGetUniformLocation(prog,"uUvRep"),1.f,1.f);
        glUniform2f(glGetUniformLocation(prog,"uUvOff"),0.f,0.f);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texFloor);
        glUniform1i(glGetUniformLocation(prog,"uTex"),0);
        glBindVertexArray(gVao); glDrawElements(GL_TRIANGLES,gIdx,GL_UNSIGNED_INT,0);
        glBindVertexArray(vao);
    }
    for(auto&n:nodes)
        cube(VP,{n.pos.x,n.pos.y+0.05f,n.pos.z},{n.r,0.06f,n.r},{0.25f,0.85f,0.8f},sun,fog,cam,1.f,texMetal,{2,2});
    // Trees are drawn in main via the real OBJ model (see treeList()).
    {
        std::vector<glm::vec3> mpos; for(auto&m:machines) mpos.push_back(m.pos);
        for(auto&b:belts){
            if(b.from>=(int)machines.size()||b.to>=(int)machines.size()) continue;
            auto nodes=drape(beltNodes(mpos,b.from,b.to,b.pts,0.f),*this,0.35f);
            for(size_t s=0;s+1<nodes.size();s++){
                glm::vec3 A=nodes[s],B=nodes[s+1];
                glm::vec3 d=B-A; float hl=std::sqrt(d.x*d.x+d.z*d.z); if(hl<0.3f) continue;
                float len=glm::length(d), pitch=std::atan2(d.y,hl);   // belts tilt over the hills
                glm::vec3 mid=(A+B)*0.5f; float ang=std::atan2(-d.z,d.x);
                glm::mat4 M=glm::translate(glm::mat4(1.f),mid)*glm::rotate(glm::mat4(1.f),ang,glm::vec3(0,1,0))
                          *glm::rotate(glm::mat4(1.f),pitch,glm::vec3(0,0,1))
                          *glm::scale(glm::mat4(1.f),{len+0.05f,0.14f,0.9f});
                drawModel(prog,VP,M,{0.8f,0.8f,0.8f},texBelt,{len*0.5f,1.f},{(float)(-now*0.6),0.f},1.f);
            }
            // corner caps fill the wedge where two segments meet at an angle
            for(size_t s=1;s+1<nodes.size();s++)
                cube(VP,nodes[s],{0.45f,0.14f,0.45f},{0.5f,0.52f,0.55f},sun,fog,cam,1.f,texMetal,{1,1});
            if(b.item>=0) for(float p:b.prog){ glm::vec3 ip=posAlong(nodes,p)+glm::vec3(0,0.22f,0);
                cube(VP,ip,{0.16f,0.16f,0.16f},ICOL[b.item],sun,fog,cam,1.f,texMetal,{1,1}); }
        }
    }
    for(int i=0;i<(int)machines.size();i++){
        drawMachine(VP,machines[i].type,machines[i].pos,machines[i].yaw,sun,fog,cam,1.f);
        if(i==aimedMachine){  // highlight ring under the aimed machine
            cube(VP,{machines[i].pos.x,heightAt(machines[i].pos.x,machines[i].pos.z)+0.06f,machines[i].pos.z},{footprint(machines[i].type).x+0.3f,0.04f,footprint(machines[i].type).z+0.3f},
                 {0.4f,1.f,0.7f},sun,fog,cam,1.f,texMetal,{1,1}); }
        if(machines[i].hasDeploy){  // deploy-point flag for this machine
            glm::vec3 dp=machines[i].deployPt;
            float fy=heightAt(dp.x,dp.z);
            cube(VP,{dp.x,fy+0.05f,dp.z},{0.5f,0.04f,0.5f},{0.9f,0.8f,0.3f},sun,fog,cam,1.f,texMetal,{1,1});
            cube(VP,{dp.x,fy+0.9f,dp.z},{0.05f,0.9f,0.05f},{0.9f,0.8f,0.3f},sun,fog,cam,1.f,texMetal,{1,1});
        }
    }
    // Deployables: robots (walk), mines/fences/tripwires (static)
    for(auto&d:deploys){ glm::vec3 b{d.pos.x,heightAt(d.pos.x,d.pos.z),d.pos.z};
        switch(d.kind){
            case DK_ROBOT:
                cube(VP,b+glm::vec3(0,0.5f,0),{0.35f,0.5f,0.35f},{0.6f,0.62f,0.7f},sun,fog,cam,1.f,texMetal,{1,1});
                cube(VP,b+glm::vec3(0,1.1f,0),{0.25f,0.2f,0.25f},{0.4f,0.85f,0.9f},sun,fog,cam,1.f,texMetal,{1,1});
                break;
            case DK_MINE:
                cube(VP,b+glm::vec3(0,0.15f,0),{0.5f,0.15f,0.5f},{0.8f,0.3f,0.25f},sun,fog,cam,1.f,texMetal,{1,1});
                cube(VP,b+glm::vec3(0,0.35f,0),{0.18f,0.12f,0.18f},{1.f,0.5f,0.2f},sun,fog,cam,1.f,texMetal,{1,1});
                break;
            case DK_TURRET:
                cube(VP,b+glm::vec3(0,0.4f,0),{0.5f,0.4f,0.5f},{0.45f,0.48f,0.55f},sun,fog,cam,1.f,texMetal,{1,1});
                cube(VP,b+glm::vec3(0,1.0f,0),{0.3f,0.3f,0.3f},{0.6f,0.63f,0.7f},sun,fog,cam,1.f,texMetal,{1,1});
                cube(VP,b+glm::vec3(0,1.05f,0.5f),{0.08f,0.08f,0.5f},{0.3f,0.32f,0.36f},sun,fog,cam,1.f,texMetal,{1,1});
                break;
            case DK_FENCE:   // a post; linked posts get a tall belt-textured wall back to the previous one
                cube(VP,b+glm::vec3(0,1.1f,0),{0.2f,1.1f,0.2f},{0.55f,0.58f,0.64f},sun,fog,cam,1.f,texMetal,{1,2});
                cube(VP,b+glm::vec3(0,2.28f,0),{0.26f,0.1f,0.26f},{0.72f,0.68f,0.5f},sun,fog,cam,1.f,texMetal,{1,1});
                if(d.linked){
                    glm::vec3 e{d.linkPos.x,heightAt(d.linkPos.x,d.linkPos.z),d.linkPos.z}, ab=e-b;
                    float L=glm::length(glm::vec2(ab.x,ab.z));
                    if(L>0.05f){ float yaw=std::atan2(ab.x,ab.z), pitch=std::atan2(ab.y,L), S=glm::length(ab);
                        glm::vec3 mid=b+ab*0.5f;
                        cubeYaw(VP,mid+glm::vec3(0,0.95f,0),{0.12f,0.95f,S*0.5f},yaw,{0.34f,0.36f,0.4f},texBelt,{std::max(1.f,S*0.7f),2},pitch);
                        cubeYaw(VP,mid+glm::vec3(0,1.95f,0),{0.18f,0.1f,S*0.5f},yaw,{0.72f,0.68f,0.5f},texMetal,{std::max(1.f,S*0.5f),1},pitch); }
                }
                break;
            case DK_TRIPWIRE:
                cube(VP,b+glm::vec3(-1.f,0.4f,0),{0.06f,0.4f,0.06f},{0.7f,0.7f,0.7f},sun,fog,cam,1.f,texMetal,{1,1});
                cube(VP,b+glm::vec3( 1.f,0.4f,0),{0.06f,0.4f,0.06f},{0.7f,0.7f,0.7f},sun,fog,cam,1.f,texMetal,{1,1});
                cube(VP,b+glm::vec3(0,0.75f,0),{1.f,0.02f,0.02f},{1.f,0.2f,0.2f},sun,fog,cam,1.f,texMetal,{1,1});
                break;
        }
    }
    // Your Hub (wizard-purple keep) — lose it and the match is over
    if(hubAlive){ glm::vec3 b{hubPos.x,baseY(hubPos,4.f,4.f),hubPos.z}; float f=hubHp/hubMax;
        cube(VP,b+glm::vec3(0,2.0f,0),{4.0f,2.0f,4.0f},{0.42f,0.34f,0.62f},sun,fog,cam,1.f,texMetal,{4,3});
        cube(VP,b+glm::vec3(0,4.2f,0),{2.4f,0.9f,2.4f},{0.55f,0.42f,0.8f},sun,fog,cam,1.f,texMetal,{2,2});
        cube(VP,b+glm::vec3(0,5.6f,0),{0.6f,0.8f,0.6f},{0.5f*f+0.4f,0.9f,1.0f},sun,fog,cam,1.f,texMetal,{1,1}); } // beacon dims as it takes damage
    // Enemy base (large red fortress) — the objective
    if(ebaseAlive){ glm::vec3 b{ebasePos.x,baseY(ebasePos,4.f,4.f),ebasePos.z};
        cube(VP,b+glm::vec3(0,1.8f,0),{4.0f,1.8f,4.0f},{0.5f,0.2f,0.2f},sun,fog,cam,1.f,texMetal,{4,3});
        cube(VP,b+glm::vec3(0,3.8f,0),{2.2f,0.9f,2.2f},{0.7f,0.25f,0.22f},sun,fog,cam,1.f,texMetal,{2,2});
        cube(VP,b+glm::vec3(0,5.1f,0),{0.5f,0.6f,0.5f},{1.f,0.35f,0.25f},sun,fog,cam,1.f,texMetal,{1,1}); }
    // Enemy robots (red)
    for(auto&e:enemies){ glm::vec3 b{e.pos.x,heightAt(e.pos.x,e.pos.z),e.pos.z};
        cube(VP,b+glm::vec3(0,0.5f,0),{0.35f,0.5f,0.35f},{0.85f,0.3f,0.28f},sun,fog,cam,1.f,texMetal,{1,1});
        cube(VP,b+glm::vec3(0,1.1f,0),{0.25f,0.2f,0.25f},{1.f,0.5f,0.4f},sun,fog,cam,1.f,texMetal,{1,1}); }
    // Opponents (net free-for-all): each rival hub is a red fortress, their
    // robots stream in as crimson contacts.
    for(auto& o:opps){ if(!o.alive) continue;
        glm::vec3 b{o.hub.x,baseY(o.hub,4.f,4.f),o.hub.z};
        cube(VP,b+glm::vec3(0,1.8f,0),{4.0f,1.8f,4.0f},{0.5f,0.2f,0.2f},sun,fog,cam,1.f,texMetal,{4,3});
        cube(VP,b+glm::vec3(0,3.8f,0),{2.2f,0.9f,2.2f},{0.7f,0.25f,0.22f},sun,fog,cam,1.f,texMetal,{2,2});
        cube(VP,b+glm::vec3(0,5.1f,0),{0.5f,0.6f,0.5f},{1.f,0.35f,0.25f},sun,fog,cam,1.f,texMetal,{1,1});
        for(auto&u:o.units){ glm::vec3 p={u.x,heightAt(u.x,u.y),u.y};
            cube(VP,p+glm::vec3(0,0.5f,0),{0.35f,0.5f,0.35f},{0.9f,0.15f,0.35f},sun,fog,cam,1.f,texMetal,{1,1});
            cube(VP,p+glm::vec3(0,1.1f,0),{0.25f,0.2f,0.25f},{1.f,0.4f,0.6f},sun,fog,cam,1.f,texMetal,{1,1}); } }
    // Shot tracers (thin stretched box, colour by side)
    for(auto&tr:tracers){ glm::vec3 d=tr.b-tr.a; float len=glm::length(d); if(len<0.1f) continue;
        glm::vec3 mid=(tr.a+tr.b)*0.5f; float ang=std::atan2(-d.z,d.x);
        glm::mat4 M=glm::translate(glm::mat4(1.f),mid)*glm::rotate(glm::mat4(1.f),ang,glm::vec3(0,1,0))
                  *glm::scale(glm::mat4(1.f),{len,0.05f,0.05f});
        drawModel(prog,VP,M, tr.friendly?glm::vec3(0.5f,1.f,0.7f):glm::vec3(1.f,0.6f,0.3f),
                  texMetal,{1,1},{0,0},1.f); }
    // Deploy-point targeting ghost
    if(deployTarget>=0){
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
        cube(VP,{ghostPos.x,heightAt(ghostPos.x,ghostPos.z)+0.5f,ghostPos.z},{0.6f,0.5f,0.6f},{0.9f,0.85f,0.3f},sun,fog,cam,0.45f,texMetal,{1,1});
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
    if(buildMode && !menuOpen && deployTarget<0){
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
        glm::vec3 tint=ghostValid?glm::vec3(0.4f,1.f,0.5f):glm::vec3(1.f,0.4f,0.4f);
        if(selType==CONVEYOR){
            if(pendingSrc>=0 && pendingSrc<(int)machines.size()){
                // preview the whole polyline: source → waypoints → aim
                std::vector<glm::vec3> gh;
                gh.push_back(machines[pendingSrc].pos);
                for(auto&p:beltPts) gh.push_back(p);
                gh.push_back(ghostPos);
                gh=drape(gh,*this,0.4f);
                for(size_t s=0;s+1<gh.size();s++){ glm::vec3 A=gh[s],B=gh[s+1];
                    glm::vec3 d=B-A; float len=std::sqrt(d.x*d.x+d.z*d.z); if(len<0.3f) continue;
                    glm::vec3 mid=(A+B)*0.5f; float ang=std::atan2(-d.z,d.x);
                    glm::mat4 M=glm::translate(glm::mat4(1.f),mid)*glm::rotate(glm::mat4(1.f),ang,glm::vec3(0,1,0))
                        *glm::scale(glm::mat4(1.f),{len,0.1f,0.5f});
                    drawModel(prog,VP,M,{0.5f,0.9f,1.f},texBelt,{len*0.5f,1.f},{0,0},0.55f); }
                for(auto&p:beltPts)   // corner markers
                    cube(VP,{p.x,heightAt(p.x,p.z)+0.4f,p.z},{0.22f,0.22f,0.22f},{0.6f,1.f,1.f},sun,fog,cam,0.6f,texMetal,{1,1});
            }
            cube(VP,{ghostPos.x,heightAt(ghostPos.x,ghostPos.z)+0.4f,ghostPos.z},{0.3f,0.3f,0.3f},tint,sun,fog,cam,0.5f,texMetal,{1,1});
        } else {
            glm::vec3 h=footprint(selType);
            cube(VP,{ghostPos.x,baseY(ghostPos,h.x,h.z)+h.y*0.5f,ghostPos.z},{h.x,h.y*0.5f,h.z},tint,sun,fog,cam,0.4f,texMetal,{1,1});
        }
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
    glBindVertexArray(0);
}

void Factory::totals(int inv[ITEM_N]) const{
    for(int i=0;i<ITEM_N;i++) inv[i]=0;
    for(auto&m:machines){ for(int i=0;i<ITEM_N;i++) inv[i]+=m.in[i];
        int oi=outItem(m); if(m.out>0 && oi>=0) inv[oi]+=m.out; }
    for(auto&b:belts) if(b.item>=0) inv[b.item]+=(int)b.prog.size();
}

void Factory::renderHud(int winW,int winH){
    if(!active) return;

    // ── Pause menu (ESC) ─────────────────────────────────────────────────
    if(paused){
        ImDrawList* bg=ImGui::GetBackgroundDrawList();
        bg->AddRectFilled({0,0},{(float)winW,(float)winH}, IM_COL32(6,8,12,180)); // dim
        ImGui::SetNextWindowPos({winW*0.5f,winH*0.5f},ImGuiCond_Always,{0.5f,0.5f});
        ImGui::SetNextWindowSize({300,0});
        ImGui::Begin("PAUSED",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove);
        ImGui::TextColored({0.7f,0.9f,1.f,1.f},"Iron Command — Paused");
        ImGui::Separator();
        float w=ImGui::GetContentRegionAvail().x;
        if(ImGui::Button("Resume",{w,40})) paused=false;
        if(ImGui::Button("Main Menu",{w,34})) reqQuitToMenu=true;
        if(ImGui::Button("Quit Game",{w,34})) reqQuit=true;
        ImGui::End();
        return;   // hide the normal HUD while paused
    }

    // ── Walkable lobby: just a crosshair prompt, no blocking window ───────
    if(phase==P_LOBBY){
        ImDrawList* dl=ImGui::GetForegroundDrawList();
        const char* t = lobbyNearTable ? "[E]  Open the War Table"
                                       : "Walk to the glowing War Table in the centre";
        ImVec2 s=ImGui::CalcTextSize(t);
        dl->AddText({winW*0.5f-s.x*0.5f, winH*0.62f},
                    lobbyNearTable?IM_COL32(150,255,180,255):IM_COL32(200,215,230,220), t);
        dl->AddText({winW*0.5f-90, 22.f}, IM_COL32(180,200,230,220),
                    multiplayer?"LOBBY — MULTIPLAYER":"LOBBY — OFFLINE");
        dl->AddCircleFilled({winW*0.5f,winH*0.5f},3.f,IM_COL32(255,255,255,150));
        return;
    }
    // ── Frozen ImGui screens: mode select + drop map ─────────────────────
    if(phase!=P_PLAY){
        ImGui::SetNextWindowPos({winW*0.5f,winH*0.5f},ImGuiCond_Always,{0.5f,0.5f});
        if(phase==P_MODE){
            ImGui::SetNextWindowSize({420,0});
            ImGui::Begin("IRON COMMAND",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove);
            ImGui::TextColored({0.7f,0.85f,1.f,1.f},"Factory Wizards — choose a mode");
            ImGui::Separator();
            if(ImGui::Button("OFFLINE  (skirmish vs AI)",{ImGui::GetContentRegionAvail().x,40})){
                multiplayer=false; netActive=false; phase=P_LOBBY; lobbyEnter=true; }
            ImGui::Dummy({0,6}); ImGui::Separator();
            ImGui::TextColored({0.8f,1.f,0.85f,1.f},"LAN Free-for-all");
            if(!netActive){
                if(ImGui::Button("HOST a match",{ImGui::GetContentRegionAvail().x,34})){
                    netHost(); multiplayer=true; }
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-96);
                ImGui::InputText("##ip",joinIp,sizeof(joinIp)); ImGui::SameLine();
                if(ImGui::Button("JOIN",{88,0})){ netJoin(joinIp); multiplayer=true; }
                ImGui::TextDisabled("Host on one PC, then Join from the others using the host's IP. Port %u.", (unsigned)IRON_PORT);
                // Surface why a join attempt failed (otherwise it looks like nothing happened).
                std::string st=netStatus();
                if(st!="offline") ImGui::TextColored({1.f,0.55f,0.5f,1.f},"Join failed: %s", st.c_str());
            } else if(net.isHost()){
                ImGui::TextColored({0.6f,1.f,0.7f,1.f},"HOSTING — %s", netStatus().c_str());
                if(!hostIp.empty()){
                    ImGui::TextColored({1.f,0.95f,0.5f,1.f},"Friends type this to Join:");
                    ImGui::TextColored({0.7f,1.f,1.f,1.f},"    %s   (port %u)", hostIp.c_str(), (unsigned)IRON_PORT);
                } else {
                    ImGui::TextDisabled("(couldn't detect your LAN IP — find it in system settings)");
                }
                ImGui::TextDisabled("Others can keep joining while you play. Last hub standing wins.");
                if(ImGui::Button("ENTER LOBBY",{ImGui::GetContentRegionAvail().x,40})){
                    multiplayer=true; phase=P_LOBBY; lobbyEnter=true; }
                if(ImGui::Button("Cancel / stop hosting",{ImGui::GetContentRegionAvail().x,0})){
                    net.close(); netActive=false; multiplayer=false; }
            } else {
                ImGui::TextColored({0.6f,1.f,0.7f,1.f},"CONNECTED — %s", netStatus().c_str());
                ImGui::TextDisabled("Last hub standing wins.");
                if(ImGui::Button("ENTER LOBBY",{ImGui::GetContentRegionAvail().x,40})){
                    multiplayer=true; phase=P_LOBBY; lobbyEnter=true; }
                if(ImGui::Button("Cancel / disconnect",{ImGui::GetContentRegionAvail().x,0})){
                    net.close(); netActive=false; multiplayer=false; }
            }
            ImGui::End();
        } else if(phase==P_DROP){
            float MS=440.f; ImGui::SetNextWindowSize({MS+30,MS+90});
            ImGui::Begin("DROP ZONE",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove);
            bool waitMap = netActive && !mapSynced;   // joiners drop only on the host's map
            ImGui::TextColored({1.f,0.9f,0.5f,1.f},"%s", waitMap? "Syncing the host's map…"
                               : netActive? "Select where to drop (keep clear of rival hubs)" : "Select where to drop");
            ImVec2 o=ImGui::GetCursorScreenPos(); ImDrawList* d=ImGui::GetWindowDrawList();
            auto w2m=[&](float wx,float wz){ return ImVec2(o.x+(wx+mapHalf)/(2*mapHalf)*MS,o.y+(wz+mapHalf)/(2*mapHalf)*MS); };
            d->AddRectFilled(o,{o.x+MS,o.y+MS},IM_COL32(30,44,30,255));   // grass
            for(auto&t:trees){ ImVec2 p=w2m(t.x,t.z); d->AddCircleFilled(p,1.4f,IM_COL32(40,90,40,255)); }
            for(auto&nd:nodes){ ImVec2 p=w2m(nd.pos.x,nd.pos.z); d->AddCircleFilled(p,2.5f,IM_COL32(60,200,190,255)); }
            for(auto&op:opps) if(op.alive){ ImVec2 p=w2m(op.hub.x,op.hub.z);
                d->AddCircleFilled(p,60.f/(2*mapHalf)*MS,IM_COL32(120,30,30,90));   // keep-out radius
                d->AddRectFilled({p.x-6,p.y-6},{p.x+6,p.y+6},IM_COL32(230,60,50,255));
                d->AddText({p.x-14,p.y+7},IM_COL32(255,150,140,255),"RIVAL"); }
            d->AddRect(o,{o.x+MS,o.y+MS},IM_COL32(90,110,90,255));
            ImGui::InvisibleButton("drop",{MS,MS});
            if(ImGui::IsItemClicked(0) && !waitMap){ ImVec2 mp=ImGui::GetIO().MousePos;
                float wx=((mp.x-o.x)/MS)*2*mapHalf-mapHalf, wz=((mp.y-o.y)/MS)*2*mapHalf-mapHalf;
                float dEnemy=999.f;   // nearest live rival hub
                for(auto&op:opps) if(op.alive) dEnemy=std::min(dEnemy,glm::length(glm::vec2(wx-op.hub.x,wz-op.hub.z)));
                if(dEnemy>60.f){
                    // Premade hub at the chosen spot, nudged clear of any ore node.
                    // y=0 here; the renderer/collision seat it on the terrain (baseY).
                    glm::vec3 hp={std::round(wx/GRID)*GRID, 0.f, std::round(wz/GRID)*GRID};
                    for(auto&nd:nodes){ glm::vec2 d(hp.x-nd.pos.x,hp.z-nd.pos.z); float L=glm::length(d);
                        if(L<9.f){ glm::vec2 pu = L>0.01f? d/L : glm::vec2(1,0);
                            hp.x=std::round((nd.pos.x+pu.x*9.f)/GRID)*GRID;
                            hp.z=std::round((nd.pos.z+pu.y*9.f)/GRID)*GRID; } }
                    hubPos=hp; hubHp=hubMax; hubAlive=true;
                    ebaseAlive=false; enemies.clear();   // PvP only — no AI base/waves
                    // Player spawns to the SIDE of the hub, also off any node.
                    glm::vec3 sp=hp+glm::vec3(7.f,0.f,0.f);
                    for(auto&nd:nodes){ if(glm::length(glm::vec2(sp.x-nd.pos.x,sp.z-nd.pos.z))<5.f) sp.x+=6.f; }
                    dropPos=sp; dropReady=true; phase=P_PLAY; syncCollision();
                    Audio::get().play("shift",0.6f);
                } else Audio::get().play("crash",0.4f,0.7f);   // too close to a rival hub
            }
            if(ImGui::Button("Back")){ phase=P_LOBBY; lobbyEnter=true; }
            ImGui::End();
        }
        return;   // no in-game HUD during the front-end
    }

    // ── Satisfactory-style two-step menu ─────────────────────────────────
    if(menuOpen && menuMachine>=0 && menuMachine<(int)machines.size()){
        Machine& m=machines[menuMachine];
        auto itemName=[&](int it)->const char*{ return it<0?"":INAME[it]; };
        bool isTerminal = (m.type==TERMINAL);
        bool isBarracks = (m.type==BARRACKS);
        if(isTerminal) mapMode=true;   // terminals go straight to the command map
        if(isBarracks){
            // Storage depot — show how many robots are held
            ImGui::SetNextWindowPos({winW*0.5f,winH*0.5f},ImGuiCond_Always,{0.5f,0.5f});
            ImGui::SetNextWindowSize({320,0});
            ImGui::Begin("Barracks",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove);
            ImGui::TextColored({0.6f,0.85f,1.f,1.f},"BARRACKS — Robot storage");
            ImGui::Separator();
            ImGui::Text("Stored robots: %d / %d", m.in[ROBOT_ITEM], CAP);
            ImGui::TextDisabled("Belt Robots here from an Assembler.");
            ImGui::TextDisabled("Use a Terminal to deploy them.");
            if(ImGui::Button("Close")){ menuOpen=false; menuMachine=-1; }
            ImGui::End();
        }
        else if(!mapMode){
            // Step 1: choose recipe (item recipes apply directly; deploy
            // recipes advance to the map to choose a location).
            ImGui::SetNextWindowPos({winW*0.5f,winH*0.5f},ImGuiCond_Always,{0.5f,0.5f});
            ImGui::SetNextWindowSize({420,0});
            ImGui::Begin("Machine",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove);
            ImGui::TextColored({0.7f,0.85f,1.f,1.f},"%s",TNAME[m.type]);
            ImGui::Separator();
            const RSet& rs=RSETS[m.type];
            // Miner overclock tiers cost Parts (0 / 8 / 20).
            static const int MINER_COST[3]={0,8,20};
            for(int i=0;i<rs.n;i++){ const Recipe& r=rs.r[i];
                char cost[80]="";
                if(m.type==MINER && i>0) snprintf(cost,sizeof(cost),"%d Parts",MINER_COST[i]);
                else { if(r.inA>=0){ char a[24]; snprintf(a,sizeof(a),"%dx %s",r.qA,itemName(r.inA)); strcat(cost,a);}
                       if(r.inB>=0){ char a[24]; snprintf(a,sizeof(a),"  %dx %s",r.qB,itemName(r.inB)); strcat(cost,a);} }
                bool cur=(i==m.recipe);
                char lbl[144]; snprintf(lbl,sizeof(lbl),"%s%-12s  (%s)##%d",cur?"> ":"   ",r.name,cost,i);
                if(ImGui::Button(lbl,{ImGui::GetContentRegionAvail().x,0})){
                    // charge Parts for a miner upgrade (downgrades are free)
                    if(m.type==MINER && i>m.recipe && partsBank<MINER_COST[i]){
                        Audio::get().play("crash",0.4f,0.7f);   // not enough Parts
                    } else if(r.deploy>=0 && r.deploy!=DK_FENCE && r.deploy!=DK_MINE && r.deploy!=DK_TURRET){ pendingRecipe=i; mapMode=true; }   // → map (fence/mine/turret stockpile instead)
                    else {
                        if(m.type==MINER && i>m.recipe) partsBank-=MINER_COST[i];
                        m.recipe=i; m.prog=0.f; m.out=0;
                        for(int b=(int)belts.size()-1;b>=0;b--) if(belts[b].from==menuMachine){
                            if(outItem(m)<0) belts.erase(belts.begin()+b);
                            else { belts[b].item=outItem(m); belts[b].prog.clear(); } }
                        menuOpen=false; menuMachine=-1;
                    }
                }
            }
            ImGui::Separator();
            ImGui::TextDisabled("Deployables (robot, turret, mine...) ask where to send them.");
            if(ImGui::Button("Close")){ menuOpen=false; menuMachine=-1; }
            ImGui::End();
        } else {
            // Step 2: top-down map of explored areas — click to deploy there.
            ImGui::SetNextWindowPos({winW*0.5f,winH*0.5f},ImGuiCond_Always,{0.5f,0.5f});
            float MS=420.f;
            ImGui::SetNextWindowSize({MS+30,MS+90});
            ImGui::Begin("Map",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove);
            if(isTerminal) ImGui::TextColored({0.5f,1.f,0.7f,1.f},"COMMAND — click to march all robots there");
            else ImGui::TextColored({1.f,0.9f,0.5f,1.f},"Deploy %s — click an explored area",
                               RSETS[m.type].r[pendingRecipe].name);
            ImVec2 o=ImGui::GetCursorScreenPos();
            ImDrawList* d=ImGui::GetWindowDrawList();
            auto w2m=[&](float wx,float wz){ return ImVec2(
                o.x+(wx+mapHalf)/(2*mapHalf)*MS, o.y+(wz+mapHalf)/(2*mapHalf)*MS); };
            d->AddRectFilled(o,{o.x+MS,o.y+MS},IM_COL32(8,10,14,255));
            // explored cells
            float cs=MS/gridN;
            for(int cz=0;cz<gridN;cz++)for(int cx=0;cx<gridN;cx++) if(explored[cz*gridN+cx]){
                ImVec2 a{o.x+cx*cs,o.y+cz*cs};
                d->AddRectFilled(a,{a.x+cs+1,a.y+cs+1},IM_COL32(40,52,46,255)); }
            // ore nodes
            for(auto&n:nodes){ ImVec2 p=w2m(n.pos.x,n.pos.z);
                d->AddCircleFilled(p, n.r/(2*mapHalf)*MS, IM_COL32(60,200,190,220)); }
            // machines
            for(auto&mm:machines){ ImVec2 p=w2m(mm.pos.x,mm.pos.z);
                d->AddRectFilled({p.x-4,p.y-4},{p.x+4,p.y+4},IM_COL32(150,170,255,255)); }
            // existing deployables
            for(auto&dp:deploys){ ImVec2 p=w2m(dp.pos.x,dp.pos.z);
                d->AddCircleFilled(p,2.f,IM_COL32(255,150,80,255)); }
            // enemies (red) + enemy base (big red square, always shown)
            for(auto&en:enemies){ ImVec2 p=w2m(en.pos.x,en.pos.z);
                d->AddCircleFilled(p,2.f,IM_COL32(240,70,60,255)); }
            if(ebaseAlive){ ImVec2 p=w2m(ebasePos.x,ebasePos.z);
                d->AddRectFilled({p.x-6,p.y-6},{p.x+6,p.y+6},IM_COL32(230,60,50,255));
                d->AddText({p.x-14,p.y+7},IM_COL32(255,150,140,255),"BASE"); }
            for(auto&o:opps) if(o.alive){ ImVec2 p=w2m(o.hub.x,o.hub.z);
                d->AddRectFilled({p.x-6,p.y-6},{p.x+6,p.y+6},IM_COL32(230,60,50,255));
                for(auto&u:o.units){ ImVec2 q=w2m(u.x,u.y); d->AddCircleFilled(q,1.6f,IM_COL32(240,90,120,255)); } }
            // player marker (camPos passed via ghostPos hack — use last aim)
            d->AddRect(o,{o.x+MS,o.y+MS},IM_COL32(90,110,120,255));
            // click handling
            ImGui::InvisibleButton("map",{MS,MS});
            if(ImGui::IsItemClicked(0)){
                ImVec2 mp=ImGui::GetIO().MousePos;
                float wx=((mp.x-o.x)/MS)*2*mapHalf-mapHalf;
                float wz=((mp.y-o.y)/MS)*2*mapHalf-mapHalf;
                int c=cellOf(wx,wz);
                if(c>=0){   // robots can be ordered into unexplored ground too
                    glm::vec3 tgt{std::round(wx/GRID)*GRID,0.f,std::round(wz/GRID)*GRID};
                    if(isTerminal){
                        // Release every robot stored in barracks (they spawn at
                        // the barracks and walk out) and re-command any already
                        // deployed to the clicked location.
                        for(auto&bar:machines) if(bar.type==BARRACKS){
                            while(bar.in[ROBOT_ITEM]>0){ bar.in[ROBOT_ITEM]--;
                                Deployable d{}; d.kind=DK_ROBOT; d.pos=bar.pos; d.goal=tgt;
                                d.walking=true; d.t=0.f; deploys.push_back(d); }
                        }
                        for(auto&d:deploys) if(d.kind==DK_ROBOT){ d.goal=tgt; d.walking=true; }
                        Audio::get().play("interact",0.6f,1.3f);
                    } else {
                        m.recipe=pendingRecipe; m.prog=0.f; m.out=0;
                        m.deployPt=tgt; m.hasDeploy=true;
                        for(int b=(int)belts.size()-1;b>=0;b--) if(belts[b].from==menuMachine)
                            belts.erase(belts.begin()+b);
                        Audio::get().play("interact",0.6f,0.9f);
                    }
                    menuOpen=false; mapMode=false; menuMachine=-1; pendingRecipe=-1;
                }
            }
            if(!isTerminal){ if(ImGui::Button("Back")) mapMode=false; ImGui::SameLine(); }
            if(ImGui::Button(isTerminal?"Close":"Cancel")){ menuOpen=false; mapMode=false; menuMachine=-1; }
            ImGui::End();
        }
    }

    // ── Hub command map: march robots, pencil fences, brush mines, box turrets ─
    if(cmdMapOpen){
        ImGui::SetNextWindowPos({winW*0.5f,winH*0.5f},ImGuiCond_Always,{0.5f,0.5f});
        float MS=460.f; ImGui::SetNextWindowSize({MS+30,MS+96});
        ImGui::Begin("HUB COMMAND",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove);
        // Tool palette: pick what to draw on the map.
        const char* bn[4]={"March","Fence","Mines","Turrets"};
        for(int b=0;b<4;b++){ if(b) ImGui::SameLine();
            bool sel=(hubBrush==b); if(sel) ImGui::PushStyleColor(ImGuiCol_Button,IM_COL32(70,130,95,255));
            if(ImGui::Button(bn[b])) hubBrush=b; if(sel) ImGui::PopStyleColor(); }
        ImGui::SameLine(); ImGui::TextColored({0.7f,0.95f,1.f,1.f},"  Fence:%d  Mines:%d  Turrets:%d",fenceBank,mineBank,turretBank);
        const char* hint = hubBrush==0?"Click/drag: march all robots there"
                         : hubBrush==1?"Pencil: draw posts (they link up). Click to add on, right-click to lift"
                         : hubBrush==2?"Brush: paint mines where you drag. Wheel = brush size"
                                      :"Drag a box: place a turret nest";
        ImGui::TextColored({0.6f,0.9f,1.f,1.f},"%s",hint);
        ImVec2 o=ImGui::GetCursorScreenPos(); ImDrawList* d=ImGui::GetWindowDrawList();
        auto w2m=[&](float wx,float wz){ return ImVec2(o.x+(wx+mapHalf)/(2*mapHalf)*MS,o.y+(wz+mapHalf)/(2*mapHalf)*MS); };
        auto m2w=[&](ImVec2 p){ return glm::vec2(((p.x-o.x)/MS)*2*mapHalf-mapHalf,((p.y-o.y)/MS)*2*mapHalf-mapHalf); };
        d->AddRectFilled(o,{o.x+MS,o.y+MS},IM_COL32(20,26,22,255));
        float cs=MS/gridN;
        for(int cz=0;cz<gridN;cz++)for(int cx=0;cx<gridN;cx++) if(explored[cz*gridN+cx]){
            ImVec2 a{o.x+cx*cs,o.y+cz*cs}; d->AddRectFilled(a,{a.x+cs+1,a.y+cs+1},IM_COL32(38,54,42,255)); }
        for(auto&n:nodes){ ImVec2 p=w2m(n.pos.x,n.pos.z); d->AddCircleFilled(p,2.f,IM_COL32(60,200,190,220)); }
        for(auto&t:trees){ ImVec2 p=w2m(t.x,t.z); d->AddCircleFilled(p,1.f,IM_COL32(40,90,40,200)); }
        for(auto&mm:machines){ ImVec2 p=w2m(mm.pos.x,mm.pos.z); d->AddRectFilled({p.x-3,p.y-3},{p.x+3,p.y+3},IM_COL32(150,170,255,255)); }
        for(auto&dp:deploys){ ImVec2 p=w2m(dp.pos.x,dp.pos.z);
            if(dp.kind==DK_FENCE){ if(dp.linked) d->AddLine(p,w2m(dp.linkPos.x,dp.linkPos.z),IM_COL32(220,210,150,255),2.f);
                d->AddCircleFilled(p,1.5f,IM_COL32(240,230,170,255)); continue; }
            d->AddCircleFilled(p,2.f, dp.kind==DK_ROBOT?IM_COL32(120,200,255,255)
                                    : dp.kind==DK_MINE?IM_COL32(255,120,80,255):IM_COL32(230,180,80,255)); }
        for(auto&en:enemies){ ImVec2 p=w2m(en.pos.x,en.pos.z); d->AddCircleFilled(p,2.f,IM_COL32(240,70,60,255)); }
        if(hubAlive){ ImVec2 p=w2m(hubPos.x,hubPos.z); d->AddRectFilled({p.x-5,p.y-5},{p.x+5,p.y+5},IM_COL32(170,130,240,255)); }
        if(ebaseAlive){ ImVec2 p=w2m(ebasePos.x,ebasePos.z); d->AddRectFilled({p.x-6,p.y-6},{p.x+6,p.y+6},IM_COL32(230,60,50,255));
            d->AddText({p.x-14,p.y+7},IM_COL32(255,150,140,255),"ENEMY"); }
        for(auto&o:opps) if(o.alive){ ImVec2 p=w2m(o.hub.x,o.hub.z); d->AddRectFilled({p.x-6,p.y-6},{p.x+6,p.y+6},IM_COL32(230,60,50,255));
            d->AddText({p.x-16,p.y+7},IM_COL32(255,150,140,255),"RIVAL");
            for(auto&u:o.units){ ImVec2 q=w2m(u.x,u.y); d->AddCircleFilled(q,1.6f,IM_COL32(240,90,120,255)); } }
        d->AddRect(o,{o.x+MS,o.y+MS},IM_COL32(90,120,100,255));

        ImGui::InvisibleButton("hubmap",{MS,MS});
        const float FENCE_JOIN=30.f;          // pencil down this close to the open fence's end = keep drawing it
        static bool dragging=false; static ImVec2 dragStart; static glm::vec2 strokeLast;
        static double lastPaintSfx=0.0;
        ImGuiIO& io=ImGui::GetIO(); ImVec2 mouse=io.MousePos; glm::vec2 W=m2w(mouse);
        bool hov=ImGui::IsItemHovered();
        glm::vec2 chainEnd{fenceChainEnd.x,fenceChainEnd.z};
        bool joinable=fenceChainOpen && glm::length(W-chainEnd)<=FENCE_JOIN;
        if(hov && hubBrush==2 && io.MouseWheel!=0.f)       // wheel resizes the mine brush
            mineBrushR=glm::clamp(mineBrushR*(1.f+0.15f*io.MouseWheel),3.f,40.f);
        if(hov && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) fenceChainOpen=false;   // lift the pencil
        if(ImGui::IsItemActivated()){ dragging=true; dragStart=mouse; strokeLast=W;
            if(hubBrush==1){                   // pencil down: continue the open fence, or start a new one
                if(!joinable) fenceChainOpen=false;
                if(placeFencePost(W)) Audio::get().play("melee_hit",0.4f,0.9f);
            } else if(hubBrush==2){
                if(paintMines(W,mineBrushR)){ Audio::get().play("melee_hit",0.45f,0.7f); lastPaintSfx=ImGui::GetTime(); }
            } }
        if(dragging){
            if(hubBrush==1){                   // pencil: a post every span the stroke travels; each joins the last
                glm::vec2 e{fenceChainEnd.x,fenceChainEnd.z};
                while(fenceChainOpen && glm::length(W-e)>=FENCE_SPAN){
                    e+=glm::normalize(W-e)*FENCE_SPAN; if(!placeFencePost(e)) break; }
                if(fenceChainOpen){ ImVec2 a=w2m(fenceChainEnd.x,fenceChainEnd.z); d->AddLine(a,mouse,IM_COL32(200,220,120,200),2.f); }
            } else if(hubBrush==2){            // brush: sweep along the mouse path so fast strokes leave no gaps
                glm::vec2 seg=W-strokeLast; float L=glm::length(seg);
                int n=(int)std::ceil(L/(mineBrushR*0.5f)), placed=0;
                for(int i=1;i<=n;i++) placed+=paintMines(strokeLast+seg*((float)i/n),mineBrushR);
                strokeLast=W;
                if(placed && ImGui::GetTime()-lastPaintSfx>0.12){ Audio::get().play("melee_hit",0.35f,0.7f); lastPaintSfx=ImGui::GetTime(); }
            } else if(hubBrush==3) d->AddRect(dragStart,mouse,IM_COL32(200,220,120,230),0,0,2.f);
            else d->AddLine(dragStart,mouse,IM_COL32(200,220,120,220),3.f);
        }
        if(hov && hubBrush==2){                // brush outline (radius on the map)
            float rp=mineBrushR/(2*mapHalf)*MS;
            d->AddCircle(mouse,rp,IM_COL32(255,140,90,dragging?255:170),32,1.5f);
        }
        if(hov && hubBrush==1 && !dragging && fenceChainOpen){   // where the next click joins from
            ImVec2 a=w2m(fenceChainEnd.x,fenceChainEnd.z);
            d->AddCircle(a,4.f,IM_COL32(255,230,120,230),12,1.5f);
            if(joinable) d->AddLine(a,mouse,IM_COL32(255,230,120,150),1.5f);
        }
        if(dragging && ImGui::IsItemDeactivated()){
            dragging=false;
            float dpix=std::sqrt((mouse.x-dragStart.x)*(mouse.x-dragStart.x)+(mouse.y-dragStart.y)*(mouse.y-dragStart.y));
            glm::vec2 A=m2w(dragStart), B=W;
            if(hubBrush==0){            // March robots to the target
                glm::vec3 tgt{std::round(B.x/GRID)*GRID,0.f,std::round(B.y/GRID)*GRID};
                for(auto&bar:machines) if(bar.type==BARRACKS)
                    while(bar.in[ROBOT_ITEM]>0){ bar.in[ROBOT_ITEM]--; Deployable rb{}; rb.kind=DK_ROBOT; rb.pos=bar.pos; rb.goal=tgt; rb.walking=true; deploys.push_back(rb); }
                for(auto&d2:deploys) if(d2.kind==DK_ROBOT){ d2.goal=tgt; d2.walking=true; }
                Audio::get().play("interact",0.6f,1.3f);
            } else if(hubBrush==1){    // pencil up: a last post where the stroke ended, then solidify
                if(fenceChainOpen && glm::length(B-glm::vec2(fenceChainEnd.x,fenceChainEnd.z))>1.f) placeFencePost(B);
                syncCollision();
            } else if(hubBrush==3){    // Turrets — drag a box, gridded
                float SP=9.f;
                glm::vec2 lo{std::min(A.x,B.x),std::min(A.y,B.y)}, hi{std::max(A.x,B.x),std::max(A.y,B.y)};
                if(dpix<8.f){ hi=lo+glm::vec2(SP,SP); }   // a tap drops a single one
                int placed=0;
                for(float x=lo.x;x<=hi.x && turretBank>0;x+=SP)
                for(float z=lo.y;z<=hi.y && turretBank>0;z+=SP){
                    Deployable dd{}; dd.kind=DK_TURRET; dd.pos={std::round(x/GRID)*GRID,0.f,std::round(z/GRID)*GRID};
                    dd.walking=false; deploys.push_back(dd); turretBank--; placed++; }
                if(placed) Audio::get().play("melee_hit",0.5f,1.0f);
            }
        }
        if(ImGui::Button("Close")) cmdMapOpen=false;
        ImGui::End();
    }

    ImDrawList* dl=ImGui::GetForegroundDrawList();
    int inv[ITEM_N]; totals(inv);
    float y=16.f; dl->AddText({18,y}, IM_COL32(210,230,235,255), "THROUGHPUT"); y+=20;
    for(int i=0;i<ITEM_N;i++){ if(i==PART) continue; char b[48]; snprintf(b,sizeof(b),"%-7s %d",INAME[i],inv[i]);
        dl->AddText({18,y}, IM_COL32(190,205,220,230), b); y+=17; }
    { char pb[48]; snprintf(pb,sizeof(pb),"PARTS   %d  (upgrades)",partsBank);
      dl->AddText({18,y}, IM_COL32(160,255,180,235), pb); y+=17; }
    int rob=0,trap=0; for(auto&d:deploys){ if(d.kind==DK_ROBOT)rob++; else trap++; }
    int stored=0; for(auto&mm:machines) if(mm.type==BARRACKS) stored+=mm.in[ROBOT_ITEM];
    int rivalBots=0; for(auto&o:opps) if(o.alive) rivalBots+=(int)o.units.size();
    char mb[110];
    if(netActive) snprintf(mb,sizeof(mb),"Stored %d  Deployed %d  Traps %d   RIVAL BOTS %d",stored,rob,trap,rivalBots);
    else          snprintf(mb,sizeof(mb),"Stored %d  Deployed %d  Traps %d",stored,rob,trap);
    dl->AddText({18,y+4}, IM_COL32(150,165,185,200), mb);

    // Enemy base objective bar (top centre)
    if(ebaseAlive){ float f=ebaseHp/ebaseMax; f=f<0?0:f;
        float bw2=300.f,bx=winW*0.5f-bw2*0.5f,by2=14.f;
        const char* t="ENEMY BASE"; ImVec2 ts=ImGui::CalcTextSize(t);
        dl->AddText({winW*0.5f-ts.x*0.5f,by2-2}, IM_COL32(255,150,140,255), t);
        dl->AddRectFilled({bx,by2+16},{bx+bw2,by2+28}, IM_COL32(0,0,0,160),3.f);
        dl->AddRectFilled({bx,by2+16},{bx+bw2*f,by2+28}, IM_COL32(200,60,55,240),3.f);
        // Peace-phase countdown: build freely until the opponent attacks
        if(gameClock<peaceTime){ char c[64]; snprintf(c,sizeof(c),"Opponent building up — first attack in %ds",(int)(peaceTime-gameClock));
            ImVec2 cs=ImGui::CalcTextSize(c);
            dl->AddText({winW*0.5f-cs.x*0.5f,by2+32}, IM_COL32(150,220,160,230), c); }
    }
    // Rival hubs: name, health and distance floating over each one, so you can
    // find them past hills and trees (and see what your robots are hitting).
    if(netActive && phase==P_PLAY){ int k=0;
        for(auto&o:opps){ k++; if(!o.alive) continue;
            glm::vec4 c=lastVP*glm::vec4(o.hub.x,baseY(o.hub,4.f,4.f)+7.5f,o.hub.z,1.f);
            if(c.w<=0.1f) continue;                                  // behind the camera
            float sx=(c.x/c.w*0.5f+0.5f)*winW, sy=(1.f-(c.y/c.w*0.5f+0.5f))*winH;
            if(sx<-60||sx>winW+60||sy<-30||sy>winH+30) continue;
            float f=glm::clamp(o.hp/hubMax,0.f,1.f), dist=glm::length(glm::vec2(o.hub.x-playerCam.x,o.hub.z-playerCam.z));
            char lb[48]; snprintf(lb,sizeof(lb),"RIVAL %d  %dm",k,(int)dist);
            ImVec2 ts=ImGui::CalcTextSize(lb);
            dl->AddText({sx-ts.x*0.5f,sy-18},IM_COL32(255,150,140,240),lb);
            dl->AddRectFilled({sx-40,sy},{sx+40,sy+6},IM_COL32(0,0,0,170),2.f);
            dl->AddRectFilled({sx-40,sy},{sx-40+80*f,sy+6},IM_COL32(220,70,60,240),2.f); } }
    if(rivalLeftT>0.f){ const char* t="A rival left the match";
        ImVec2 ts=ImGui::CalcTextSize(t);
        dl->AddText({winW*0.5f-ts.x*0.5f,36.f},IM_COL32(255,220,150,(int)(255*std::min(1.f,rivalLeftT))),t); }
    // Net free-for-all banner: rivals still standing
    if(netActive){ int alive=0; for(auto&o:opps) if(o.alive) alive++;
        char c[64]; snprintf(c,sizeof(c), net.connected()? "FREE-FOR-ALL — %d rival(s) standing" : "waiting for players…", alive);
        ImVec2 cs=ImGui::CalcTextSize(c);
        dl->AddText({winW*0.5f-cs.x*0.5f, 16.f}, IM_COL32(255,180,120,235), c);
    }

    // ── Live minimap (top-right): updates continuously, incl. other players ──
    if(phase==P_PLAY){
        float MM=180.f, ox=winW-MM-16.f, oy=16.f;
        auto w2mm=[&](float wx,float wz){ return ImVec2(ox+(wx+mapHalf)/(2*mapHalf)*MM, oy+(wz+mapHalf)/(2*mapHalf)*MM); };
        dl->AddRectFilled({ox,oy},{ox+MM,oy+MM}, IM_COL32(12,16,14,200));
        float cs=MM/(float)gridN;
        for(int cz=0;cz<gridN;cz++)for(int cx=0;cx<gridN;cx++) if(explored[cz*gridN+cx]){
            ImVec2 a{ox+cx*cs,oy+cz*cs}; dl->AddRectFilled(a,{a.x+cs+1,a.y+cs+1},IM_COL32(34,50,40,255)); }
        for(auto&n:nodes){ ImVec2 p=w2mm(n.pos.x,n.pos.z); dl->AddCircleFilled(p,1.4f,IM_COL32(60,200,190,220)); }
        for(auto&mm2:machines){ ImVec2 p=w2mm(mm2.pos.x,mm2.pos.z); dl->AddRectFilled({p.x-1.5f,p.y-1.5f},{p.x+1.5f,p.y+1.5f},IM_COL32(150,170,255,255)); }
        for(auto&dp:deploys){ ImVec2 p=w2mm(dp.pos.x,dp.pos.z);
            if(dp.kind==DK_FENCE){ if(dp.linked) dl->AddLine(p,w2mm(dp.linkPos.x,dp.linkPos.z),IM_COL32(220,210,150,230),1.5f); continue; }
            dl->AddCircleFilled(p,1.4f, dp.kind==DK_ROBOT?IM_COL32(120,200,255,255):IM_COL32(230,180,80,220)); }
        for(auto&en:enemies){ ImVec2 p=w2mm(en.pos.x,en.pos.z); dl->AddCircleFilled(p,1.6f,IM_COL32(240,70,60,255)); }
        // Opponents (live): their hub, robots, and avatar
        for(auto&o:opps) if(o.alive){ ImVec2 p=w2mm(o.hub.x,o.hub.z);
            dl->AddRectFilled({p.x-3,p.y-3},{p.x+3,p.y+3},IM_COL32(235,70,60,255));
            for(auto&u:o.units){ ImVec2 q=w2mm(u.x,u.y); dl->AddCircleFilled(q,1.4f,IM_COL32(240,90,120,240)); }
            ImVec2 ap=w2mm(o.avatarPos.x,o.avatarPos.z); dl->AddCircleFilled(ap,3.f,IM_COL32(90,160,255,255)); }
        if(hubAlive){ ImVec2 p=w2mm(hubPos.x,hubPos.z); dl->AddRectFilled({p.x-3,p.y-3},{p.x+3,p.y+3},IM_COL32(170,130,240,255)); }
        // The player
        { ImVec2 p=w2mm(playerCam.x,playerCam.z); dl->AddCircleFilled(p,3.f,IM_COL32(255,255,255,255));
          dl->AddCircle(p,3.f,IM_COL32(0,0,0,180)); }
        dl->AddRect({ox,oy},{ox+MM,oy+MM}, IM_COL32(90,120,100,255));
    }
    // Your Hub health (top-left, under the stockpile)
    if(hubAlive){ float f=hubHp/hubMax; if(f<0)f=0;
        float hx=18.f, hy=(float)(y+26);
        dl->AddText({hx,hy-2}, IM_COL32(180,160,255,255), "HUB");
        dl->AddRectFilled({hx+40,hy},{hx+240,hy+12}, IM_COL32(0,0,0,160),3.f);
        dl->AddRectFilled({hx+40,hy},{hx+40+200*f,hy+12}, IM_COL32(140,110,230,240),3.f);
        dl->AddText({hx,hy+16}, IM_COL32(120,140,170,200), "[E] at the Hub = top-down view"); }
    if(won){ const char* t=netActive? "LAST HUB STANDING — VICTORY" : "ENEMY BASE DESTROYED — VICTORY";
        ImVec2 ts=ImGui::CalcTextSize(t);
        dl->AddText({winW*0.5f-ts.x*0.5f,winH*0.35f}, IM_COL32(120,255,150,255), t); }
    if(lost){
        dl->AddRectFilled({0,0},{(float)winW,(float)winH}, IM_COL32(30,0,0,120));   // red vignette
        const char* t="YOU DIED — DEFEAT";
        ImVec2 ts=ImGui::CalcTextSize(t);
        dl->AddRectFilled({winW*0.5f-ts.x*0.5f-16,winH*0.4f-8},{winW*0.5f+ts.x*0.5f+16,winH*0.4f+22},IM_COL32(50,0,0,220),4.f);
        dl->AddText({winW*0.5f-ts.x*0.5f,winH*0.4f}, IM_COL32(255,90,80,255), t);
        const char* r="Press ENTER to play again";
        ImVec2 rs=ImGui::CalcTextSize(r);
        dl->AddText({winW*0.5f-rs.x*0.5f,winH*0.4f+34}, IM_COL32(210,200,200,230), r);
    }

    // Deploy-targeting banner
    if(deployTarget>=0){
        const char* t="Click the ground to set the DEPLOY LOCATION";
        ImVec2 sz=ImGui::CalcTextSize(t); float cx=winW*0.5f;
        dl->AddRectFilled({cx-sz.x*0.5f-10,winH*0.5f+26},{cx+sz.x*0.5f+10,winH*0.5f+48},IM_COL32(60,50,10,220),4.f);
        dl->AddText({cx-sz.x*0.5f,winH*0.5f+29}, IM_COL32(255,230,120,255), t);
    }
    // Aimed-machine tooltip
    else if(aimedMachine>=0 && !menuOpen){ const Machine&m=machines[aimedMachine];
        const Recipe&r=mrec(m.type,m.recipe);
        char t[96]; snprintf(t,sizeof(t),"%s:  %s", TNAME[m.type], r.name);
        ImVec2 sz=ImGui::CalcTextSize(t); float cx=winW*0.5f;
        dl->AddRectFilled({cx-sz.x*0.5f-8,winH*0.5f+24},{cx+sz.x*0.5f+8,winH*0.5f+62},IM_COL32(20,26,30,200),4.f);
        dl->AddText({cx-sz.x*0.5f, winH*0.5f+28}, IM_COL32(220,235,235,255), t);
        dl->AddText({cx-40, winH*0.5f+44}, IM_COL32(150,255,180,230), "[E] open menu");
    }

    float by=winH-70.f, x0=winW*0.5f;
    // The build palette only shows while actually building.
    if(buildMode){
        float bw=82.f,gap=5.f,total=TOOL_N*bw+(TOOL_N-1)*gap; x0=winW*0.5f-total*0.5f;
        for(int i=0;i<TOOL_N;i++){ float x=x0+i*(bw+gap); bool sel=(i==selType);
            ImU32 bg=sel?IM_COL32(60,120,90,225):IM_COL32(30,34,40,180);
            dl->AddRectFilled({x,by},{x+bw,by+44},bg,4.f);
            if(sel) dl->AddRect({x,by},{x+bw,by+44},IM_COL32(120,255,170,255),4.f,0,2.f);
            char b[32]; snprintf(b,sizeof(b),"%d %s",i+1,TNAME[i]);
            dl->AddText({x+9,by+14}, IM_COL32(220,230,235,255), b); }
        const char* hint = selType==CONVEYOR
            ? "CONVEYOR  LMB source, LMB ground for corners, LMB destination   RMB undo/cancel"
            : "BUILD  [1-7]/scroll pick   [R] rotate   LMB place   RMB dismantle   [B] exit";
        dl->AddText({x0, by+50}, IM_COL32(160,200,175,220), hint);
    } else {
        dl->AddText({winW*0.5f-140, by+50}, IM_COL32(140,160,180,180),
                    "[B] build     [E] use a machine you look at");
    }
    dl->AddCircleFilled({winW*0.5f, winH*0.5f}, 3.f, buildMode?IM_COL32(120,255,150,220):IM_COL32(255,255,255,150));
}
