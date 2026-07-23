#include "Rts.h"
#include "../platform/Audio.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include <random>
#include <cmath>
#include <algorithm>

// ── minimal solid-cube renderer ───────────────────────────────────────────
static const char* CUBE_VS = R"(#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uMVP; uniform mat4 uModel;
out vec3 vN; out vec3 vW;
void main(){ vN = mat3(uModel)*aNormal; vW = vec3(uModel*vec4(aPos,1.0));
             gl_Position = uMVP*vec4(aPos,1.0); })";
static const char* CUBE_FS = R"(#version 410 core
in vec3 vN; in vec3 vW; out vec4 F;
uniform vec3 uColor; uniform vec3 uSun; uniform float uFog; uniform vec3 uCam;
void main(){ float d = max(dot(normalize(vN), normalize(uSun)), 0.0);
             vec3 c = uColor*(0.45+0.55*d);
             float dist = length(vW-uCam);
             float f = exp(-uFog*dist*dist);
             vec3 sky = vec3(0.55,0.62,0.72);
             F = vec4(mix(sky, c, clamp(f,0.0,1.0)), 1.0); })";

static GLuint compile(GLenum t, const char* s){ GLuint sh=glCreateShader(t);
    glShaderSource(sh,1,&s,nullptr); glCompileShader(sh); return sh; }

void Rts::initCube() {
    GLuint v=compile(GL_VERTEX_SHADER,CUBE_VS), f=compile(GL_FRAGMENT_SHADER,CUBE_FS);
    prog=glCreateProgram(); glAttachShader(prog,v); glAttachShader(prog,f);
    glLinkProgram(prog); glDeleteShader(v); glDeleteShader(f);
    // unit cube [-0.5,0.5]^3, per-face normals
    struct V{ float p[3], n[3]; };
    const glm::vec3 nrm[6]={{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    const int idx[6][4]={{4,5,6,7},{1,0,3,2},{5,1,2,6},{0,4,7,3},{7,6,2,3},{0,1,5,4}};
    const glm::vec3 corner[8]={{-.5,-.5,.5},{-.5,-.5,-.5},{.5,-.5,-.5},{.5,-.5,.5},
                               {-.5,.5,.5},{-.5,.5,-.5},{.5,.5,-.5},{.5,.5,.5}};
    std::vector<V> verts;
    for(int fi=0;fi<6;fi++){ int a=idx[fi][0],b=idx[fi][1],c=idx[fi][2],d=idx[fi][3];
        int tri[6]={a,b,c,a,c,d};
        for(int k=0;k<6;k++){ glm::vec3 p=corner[tri[k]];
            verts.push_back({{p.x,p.y,p.z},{nrm[fi].x,nrm[fi].y,nrm[fi].z}}); } }
    glGenVertexArrays(1,&cubeVao); glGenBuffers(1,&cubeVbo);
    glBindVertexArray(cubeVao); glBindBuffer(GL_ARRAY_BUFFER,cubeVbo);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(V),verts.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

Rts::~Rts() {
    if(cubeVao) glDeleteVertexArrays(1,&cubeVao);
    if(cubeVbo) glDeleteBuffers(1,&cubeVbo);
    if(prog)    glDeleteProgram(prog);
}

void Rts::drawCube(const glm::mat4& VP, glm::vec3 c, glm::vec3 h,
                   glm::vec3 col, glm::vec3 sun, float fog) {
    glm::mat4 M = glm::translate(glm::mat4(1.f), c)
                * glm::scale(glm::mat4(1.f), h*2.f);
    glm::mat4 MVP = VP*M;
    glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),1,GL_FALSE,glm::value_ptr(MVP));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"),1,GL_FALSE,glm::value_ptr(M));
    glUniform3fv(glGetUniformLocation(prog,"uColor"),1,glm::value_ptr(col));
    glDrawArrays(GL_TRIANGLES,0,36);
}

// ── match setup ────────────────────────────────────────────────────────────
void Rts::start(unsigned seed) {
    if(!prog) initCube();
    units.clear(); buildings.clear(); resources.clear(); rocks.clear();
    metalStore = 20.f; started = true;
    camTarget = {0.f, 0.f};

    std::mt19937 rng(seed ? seed : std::random_device{}());
    std::uniform_real_distribution<float> U(-mapHalf+8.f, mapHalf-8.f);
    std::uniform_real_distribution<float> A(0.f, 6.2831853f);

    // Starting Hub near the bottom of the map
    buildings.push_back({{0.f, 0.f, mapHalf*0.55f}, {4.f,4.f}, 0, 300.f, 0.f});

    // Procedural metal nodes (clustered) and rock obstacles (scattered)
    std::uniform_int_distribution<int> NN(7, 11);
    int nNodes = NN(rng);
    for(int i=0;i<nNodes;i++){
        glm::vec3 c{U(rng), 0.f, U(rng)};
        int cluster = 2 + (rng()%3);
        for(int k=0;k<cluster;k++){
            float a=A(rng), r=(rng()%40)/10.f;
            resources.push_back({c+glm::vec3(std::cos(a)*r,0,std::sin(a)*r), 500.f});
        }
    }
    std::uniform_int_distribution<int> RR(40, 70);
    int nRocks = RR(rng);
    for(int i=0;i<nRocks;i++){
        glm::vec3 p{U(rng),0.f,U(rng)};
        if(glm::length(glm::vec2(p.x,p.z)-glm::vec2(0,mapHalf*0.55f))<12.f) continue;
        rocks.push_back({p, 1.2f + (rng()%20)/10.f});
    }

    // A starting squad of robots by the hub
    for(int i=0;i<6;i++){
        Unit u; u.pos = {(-3.f + i*1.2f), 0.f, mapHalf*0.55f - 6.f};
        units.push_back(u);
    }
}

// ── camera ──────────────────────────────────────────────────────────────────
glm::vec3 Rts::eye() const {
    glm::vec3 t{camTarget.x, 0.f, camTarget.y};
    glm::vec3 dir{ std::cos(camPitch)*std::sin(camYaw),
                   std::sin(camPitch),
                   std::cos(camPitch)*std::cos(camYaw) };
    return t + dir*camDist;
}
glm::mat4 Rts::view() const {
    return glm::lookAt(eye(), glm::vec3(camTarget.x,0.f,camTarget.y), {0,1,0});
}
glm::mat4 Rts::proj(float aspect) const {
    return glm::perspective(glm::radians(45.f), aspect, 0.5f, 800.f);
}

bool Rts::screenToGround(float sx, float sy, glm::vec3& out) const {
    float ndcX = (sx / winSize.x) * 2.f - 1.f;
    float ndcY = 1.f - (sy / winSize.y) * 2.f;
    glm::mat4 inv = glm::inverse(proj(lastAspect) * view());
    glm::vec4 pNear = inv * glm::vec4(ndcX, ndcY, -1.f, 1.f);
    glm::vec4 pFar  = inv * glm::vec4(ndcX, ndcY,  1.f, 1.f);
    pNear /= pNear.w; pFar /= pFar.w;
    glm::vec3 ro = glm::vec3(pNear);
    glm::vec3 rd = glm::normalize(glm::vec3(pFar) - ro);
    if (std::abs(rd.y) < 1e-5f) return false;
    float t = -ro.y / rd.y;
    if (t < 0.f) return false;
    out = ro + rd*t;
    return true;
}

// ── input ─────────────────────────────────────────────────────────────────
void Rts::handleInput(GLFWwindow* win, float dt, float aspect) {
    lastAspect = aspect;
    int ww, wh; glfwGetWindowSize(win, &ww, &wh);
    winSize = {(float)ww, (float)wh};

    // Camera pan on the ground plane (WASD / arrows), aligned to camera yaw
    glm::vec2 fwd{ std::sin(camYaw), std::cos(camYaw) };
    glm::vec2 rgt{ fwd.y, -fwd.x };
    float pan = camDist * 0.9f * dt;
    auto down=[&](int k){ return glfwGetKey(win,k)==GLFW_PRESS; };
    if(down(GLFW_KEY_W)||down(GLFW_KEY_UP))    camTarget -= fwd*pan;
    if(down(GLFW_KEY_S)||down(GLFW_KEY_DOWN))  camTarget += fwd*pan;
    if(down(GLFW_KEY_A)||down(GLFW_KEY_LEFT))  camTarget -= rgt*pan;
    if(down(GLFW_KEY_D)||down(GLFW_KEY_RIGHT)) camTarget += rgt*pan;
    camTarget = glm::clamp(camTarget, glm::vec2(-mapHalf), glm::vec2(mapHalf));

    // Zoom: scroll (fed via addScroll) or -/= keys
    if(down(GLFW_KEY_EQUAL))  camDist -= 30.f*dt;
    if(down(GLFW_KEY_MINUS))  camDist += 30.f*dt;
    camDist = glm::clamp(camDist - scrollAccum*3.f, 14.f, 85.f);
    scrollAccum = 0.f;

    double mx,my; glfwGetCursorPos(win,&mx,&my);
    bool lmb = glfwGetMouseButton(win,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
    bool rmb = glfwGetMouseButton(win,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;

    // Left button: drag-select
    if(lmb && !lmbPrev){ dragging=true; dragStart={(float)mx,(float)my}; dragCur=dragStart; }
    if(lmb && dragging) dragCur={(float)mx,(float)my};
    if(!lmb && lmbPrev && dragging){
        dragging=false;
        glm::vec2 a=glm::min(dragStart,dragCur), b=glm::max(dragStart,dragCur);
        bool box = glm::length(dragCur-dragStart) > 6.f;   // drag vs. click
        glm::mat4 VP = proj(lastAspect)*view();
        int hits=0;
        for(auto& u:units){
            glm::vec4 c = VP*glm::vec4(u.pos+glm::vec3(0,0.6f,0),1.f);
            if(c.w<=0){ u.selected=false; continue; }        // behind camera
            glm::vec2 sp{ (c.x/c.w*0.5f+0.5f)*winSize.x,
                          (1.f-(c.y/c.w*0.5f+0.5f))*winSize.y };
            bool in = box ? (sp.x>=a.x&&sp.x<=b.x&&sp.y>=a.y&&sp.y<=b.y)
                          : (glm::length(sp-dragStart) < 22.f);
            u.selected = in;                                 // fresh selection
            if(in) hits++;
        }
        if(hits) Audio::get().play("interact",0.4f,1.2f);
    }

    // Right button: command selected units to the ground point
    if(rmb && !rmbPrev){
        glm::vec3 g;
        if(screenToGround((float)mx,(float)my,g)){
            // spread the group around the click so they don't stack
            int n=0; for(auto&u:units) if(u.selected) n++;
            int i=0;
            for(auto& u:units) if(u.selected){
                float a = (n>1)? (6.2831853f*i/n):0.f;
                float r = (n>1)? 1.2f*std::sqrt((float)n):0.f;
                u.goal = {g.x+std::cos(a)*r, g.z+std::sin(a)*r};
                u.hasGoal=true; i++;
            }
            if(n) Audio::get().play("interact",0.5f,0.8f);
        }
    }

    lmbPrev=lmb; rmbPrev=rmb;
}

// ── simulation ───────────────────────────────────────────────────────────────
void Rts::update(float dt) {
    if(!started) return;

    // Units steer toward goals, nudged away from rocks and each other
    const float SPD = 8.f;
    for(auto& u:units){
        if(u.hasGoal){
            glm::vec2 to = u.goal - glm::vec2(u.pos.x,u.pos.z);
            float d = glm::length(to);
            if(d < 0.4f){ u.hasGoal=false; }
            else {
                glm::vec2 dir = to/d;
                // rock avoidance
                for(auto& r:rocks){
                    glm::vec2 rp{r.pos.x,r.pos.z};
                    glm::vec2 away = glm::vec2(u.pos.x,u.pos.z)-rp;
                    float rd=glm::length(away);
                    if(rd < r.r+1.6f && rd>0.01f) dir += (away/rd)*(1.4f);
                }
                dir = glm::normalize(dir);
                glm::vec2 np = glm::vec2(u.pos.x,u.pos.z) + dir*SPD*dt;
                u.pos.x=np.x; u.pos.z=np.y;
            }
        }
    }

    // Miners (buildings kind==1) generate metal near a resource node
    for(auto& b:buildings){
        if(b.kind!=1) continue;
        b.mineCd -= dt;
        if(b.mineCd<=0.f){ b.mineCd=1.f; metalStore += 2.f; }
    }
    // Hub passive trickle so the counter lives before the economy exists
    metalStore += dt*0.5f;
}

// ── render ───────────────────────────────────────────────────────────────────
void Rts::render(const glm::mat4& VP, glm::vec3 sunDir, float fog) {
    if(!started||!prog) return;
    glUseProgram(prog);
    glUniform3fv(glGetUniformLocation(prog,"uSun"),1,glm::value_ptr(sunDir));
    glUniform1f (glGetUniformLocation(prog,"uFog"),fog);
    glm::vec3 cam = eye();
    glUniform3fv(glGetUniformLocation(prog,"uCam"),1,glm::value_ptr(cam));
    glBindVertexArray(cubeVao);

    // Ground slab (dusty tone)
    drawCube(VP, {0,-0.5f,0}, {mapHalf,0.5f,mapHalf}, {0.34f,0.30f,0.26f}, sunDir, fog);
    // Resource nodes (glowing teal metal)
    for(auto& r:resources)
        drawCube(VP, r.pos+glm::vec3(0,0.4f,0), {0.7f,0.4f,0.7f}, {0.2f,0.8f,0.75f}, sunDir, fog);
    // Rocks
    for(auto& r:rocks)
        drawCube(VP, r.pos+glm::vec3(0,r.r*0.5f,0), {r.r,r.r*0.6f,r.r}, {0.30f,0.28f,0.30f}, sunDir, fog);
    // Buildings
    for(auto& b:buildings){
        glm::vec3 col = b.kind==0? glm::vec3(0.55f,0.45f,0.75f)   // hub — wizard purple
                      : b.kind==1? glm::vec3(0.6f,0.55f,0.3f)     // miner
                                 : glm::vec3(0.7f,0.4f,0.35f);    // turret
        drawCube(VP, b.pos+glm::vec3(0,1.2f,0), {b.size.x*0.5f,1.2f,b.size.y*0.5f}, col, sunDir, fog);
    }
    // Units (robots) — selection ring drawn as a flat green pad underneath
    for(auto& u:units){
        if(u.selected)
            drawCube(VP, u.pos+glm::vec3(0,0.05f,0), {0.9f,0.05f,0.9f}, {0.3f,1.f,0.4f}, sunDir, fog);
        drawCube(VP, u.pos+glm::vec3(0,0.6f,0), {0.4f,0.6f,0.4f}, {0.65f,0.68f,0.72f}, sunDir, fog);
        drawCube(VP, u.pos+glm::vec3(0,1.35f,0), {0.28f,0.22f,0.28f}, {0.5f,0.53f,0.58f}, sunDir, fog);
    }
    glBindVertexArray(0);
}

void Rts::renderHud() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    // Drag-select rectangle
    if(dragging){
        glm::vec2 a=glm::min(dragStart,dragCur), b=glm::max(dragStart,dragCur);
        dl->AddRectFilled({a.x,a.y},{b.x,b.y}, IM_COL32(90,220,120,40));
        dl->AddRect      ({a.x,a.y},{b.x,b.y}, IM_COL32(120,255,150,200));
    }
    // Resource + selection readout, top-left
    int sel=0; for(auto&u:units) if(u.selected) sel++;
    char buf[64];
    snprintf(buf,sizeof(buf),"METAL  %d", (int)metalStore);
    dl->AddText({20,18}, IM_COL32(180,230,220,255), buf);
    snprintf(buf,sizeof(buf),"Selected: %d robots", sel);
    dl->AddText({20,38}, IM_COL32(200,220,235,220), buf);
    dl->AddText({20,62}, IM_COL32(150,160,180,200),
                "WASD pan  ScrollZoom  LMB select  RMB move");
}
