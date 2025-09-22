#include <GL/glew.h>
#include <GLFW/glfw3.h> 

#include <windows.h>
#include <vector>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <iostream>


// ---------- Simple math helpers ----------
struct Vec2 { float x, y; Vec2():x(0),y(0){} Vec2(float _x,float _y):x(_x),y(_y){}
    Vec2 operator+(const Vec2&o)const{return{ x+o.x, y+o.y };}
    Vec2 operator-(const Vec2&o)const{return{ x-o.x, y-o.y };}
    Vec2 operator*(float s)const{return{ x*s, y*s };}
};

float length(const Vec2 &v){return std::sqrt(v.x*v.x+v.y*v.y);} 
Vec2 normalize(const Vec2 &v){float l=length(v); if(l==0) return {0,0}; return {v.x/l, v.y/l};}

// ---------- Game objects ----------
struct Enemy {
    Vec2 pos;
    float radius;
    float hp;
    Enemy(Vec2 p):pos(p),radius(18.0f),hp(1.0f){}
};

struct Player {
    Vec2 pos;
    float radius;
    float speed;
    Player():pos{400,300},radius(12.0f),speed(280.0f){}
} player;

// ---------- Globals ----------
int WINDOW_W = 800;
int WINDOW_H = 600;
std::vector<Enemy> enemies;

bool mouseLeftDown = false;
double mouseX=0, mouseY=0;
float fireCooldown = 0.0f; // seconds

// ---------- Utility: Ray (p + t*d, t>=0) vs Circle test ----------
// returns intersection t (distance along ray) or -1 if none
float ray_vs_circle(const Vec2& p, const Vec2& d, const Vec2& c, float r){
    // Solve |p + t d - c|^2 = r^2  --> quadratic in t: (d.d) t^2 + 2 d.(p-c) t + (p-c).(p-c) - r^2 = 0
    float dx = d.x, dy = d.y;
    float fx = p.x - c.x;
    float fy = p.y - c.y;
    float a = dx*dx + dy*dy;
    float b = 2*(dx*fx + dy*fy);
    float cterm = fx*fx + fy*fy - r*r;
    float disc = b*b - 4*a*cterm;
    if(disc < 0) return -1.0f;
    disc = std::sqrt(disc);
    float t1 = (-b - disc) / (2*a);
    float t2 = (-b + disc) / (2*a);
    float t = -1.0f;
    if(t1 >= 0) t = t1; else if(t2 >= 0) t = t2;
    return t;
}

// ---------- Spawn helper ----------
void spawnEnemy(){
    float x = (float)(std::rand() % WINDOW_W);
    float y = (float)(std::rand() % WINDOW_H);
    enemies.emplace_back(Vec2{x,y});
}

// ---------- Input callbacks ----------
void mouse_button_callback(GLFWwindow* wnd, int button, int action, int mods){
    if(button == GLFW_MOUSE_BUTTON_LEFT){
        mouseLeftDown = (action == GLFW_PRESS);
    }
}

void cursor_position_callback(GLFWwindow* wnd, double x, double y){
    mouseX = x; mouseY = y;
}

// ---------- Render helpers (legacy immediate mode for simplicity) ----------
void drawCircle(const Vec2 &c, float r, int segments=28){
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(c.x, c.y);
    for(int i=0;i<=segments;i++){
        float a = (float)i/segments * 2.0f * 3.14159265f;
        glVertex2f(c.x + std::cos(a)*r, c.y + std::sin(a)*r);
    }
    glEnd();
}

void drawLine(const Vec2 &a, const Vec2 &b){
    glBegin(GL_LINES);
    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glEnd();
}

// ---------- Main ----------
int main(){
    std::srand((unsigned int)std::time(nullptr));

    if(!glfwInit()){
        std::cerr<<"Failed to init GLFW\n"; return -1;
    }

    GLFWwindow* window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Top-down Hitscan Shooter", NULL, NULL);
    if(!window){ glfwTerminate(); std::cerr<<"Failed to create window\n"; return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if(glewInit() != GLEW_OK){ std::cerr<<"Failed to init GLEW\n"; return -1; }

    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Setup orthographic projection matching pixel coordinates
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_W, WINDOW_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // initial enemies
    for(int i=0;i<6;i++) spawnEnemy();

    double last = glfwGetTime();

    while(!glfwWindowShouldClose(window)){
        double now = glfwGetTime();
        float dt = (float)(now - last);
        last = now;

        // Input: WASD
        float vx=0, vy=0;
        if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) vy -= 1.0f;
        if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) vy += 1.0f;
        if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) vx -= 1.0f;
        if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) vx += 1.0f;
        Vec2 v(vx, vy);
        if(length(v) > 0.0001f) v = normalize(v);
        player.pos = player.pos + v * (player.speed * dt);

        // clamp to window
        if(player.pos.x < 0) player.pos.x = 0;
        if(player.pos.y < 0) player.pos.y = 0;
        if(player.pos.x > WINDOW_W) player.pos.x = WINDOW_W;
        if(player.pos.y > WINDOW_H) player.pos.y = WINDOW_H;

        // Enemies move toward player
        for(auto &e : enemies){
            Vec2 dir = normalize(player.pos - e.pos);
            e.pos = e.pos + dir * (60.0f * dt);
        }

        // Firing (hitscan)
        if(fireCooldown > 0) fireCooldown -= dt;
        if(mouseLeftDown && fireCooldown <= 0.0f){
            fireCooldown = 0.12f; // 8.3 shots / sec
            Vec2 p = player.pos;
            Vec2 mouse{(float)mouseX, (float)mouseY};
            Vec2 d = normalize(mouse - p);

            // trace far enough
            float farT = 2000.0f;
            float bestT = farT+1.0f;
            int bestIdx = -1;
            for(size_t i=0;i<enemies.size();++i){
                float t = ray_vs_circle(p, d, enemies[i].pos, enemies[i].radius);
                if(t >= 0.0f && t < bestT){ bestT = t; bestIdx = (int)i; }
            }
            if(bestIdx != -1){
                // hit enemy
                enemies[bestIdx].hp -= 1.0f; // one-hit
            }

            // simple recoil/kick - not implemented graphically
        }

        // remove dead enemies and occasionally spawn
        for(int i=(int)enemies.size()-1;i>=0;--i){ if(enemies[i].hp <= 0) enemies.erase(enemies.begin()+i); }
        if((int)enemies.size() < 6 && (std::rand()%100) < 10) spawnEnemy();

        // render
        glClearColor(0.09f,0.09f,0.12f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // draw player
        glColor3f(0.2f,0.7f,0.2f);
        drawCircle(player.pos, player.radius);

        // draw gun direction
        Vec2 aimDir = normalize(Vec2((float)mouseX, (float)mouseY) - player.pos);
        Vec2 gunTip = player.pos + aimDir * (player.radius + 10.0f);
        glLineWidth(3.0f);
        glColor3f(1.0f,1.0f,0.2f);
        drawLine(player.pos, gunTip + aimDir * 20.0f);

        // draw enemies
        for(auto &e : enemies){
            glColor3f(0.8f,0.25f,0.25f);
            drawCircle(e.pos, e.radius);
            // health bar
            glColor3f(0.0f,0.0f,0.0f);
            glBegin(GL_QUADS);
            glVertex2f(e.pos.x - e.radius, e.pos.y - e.radius - 8);
            glVertex2f(e.pos.x + e.radius, e.pos.y - e.radius - 8);
            glVertex2f(e.pos.x + e.radius, e.pos.y - e.radius - 4);
            glVertex2f(e.pos.x - e.radius, e.pos.y - e.radius - 4);
            glEnd();
            glColor3f(0.0f,1.0f,0.2f);
            glBegin(GL_QUADS);
            glVertex2f(e.pos.x - e.radius, e.pos.y - e.radius - 8);
            glVertex2f(e.pos.x - e.radius + e.hp * (e.radius*2.0f), e.pos.y - e.radius - 8);
            glVertex2f(e.pos.x - e.radius + e.hp * (e.radius*2.0f), e.pos.y - e.radius - 4);
            glVertex2f(e.pos.x - e.radius, e.pos.y - e.radius - 4);
            glEnd();
        }

        // Draw aiming debug ray when firing or hovering
        if(true){
            Vec2 p = player.pos;
            Vec2 mouse{(float)mouseX, (float)mouseY};
            Vec2 d = normalize(mouse - p);
            Vec2 end = p + d * 2000.0f;
            glLineWidth(1.0f);
            glColor3f(0.5f,0.5f,0.6f);
            drawLine(p, end);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

/* ===== Build & Run Instructions =====

Dependencies:
 - GLFW (https://www.glfw.org)  (or your package manager)
 - GLEW (http://glew.sourceforge.net/) OR you can use glad for GL loader
 - OpenGL (comes with Windows via opengl32.lib)

Windows (Visual Studio) steps (simple):
 1. Create a new Visual C++ project (Empty project) or use your existing toolchain.
 2. Add main.cpp file contents into project.
 3. Add include directories for GLFW and GLEW, and library directories for the .lib files.
 4. Link with: glfw3.lib, glew32.lib (or glew32s if using static), opengl32.lib, and any dependencies (gdi32, user32 are linked automatically by linker usually).
 5. Copy the required DLLs (glfw3.dll, glew32.dll) to same folder as exe or add to PATH.

Using MinGW (example):
 g++ main.cpp -o topdown.exe -Ipath\to\glfw\include -Ipath\to\glew\include -Lpath\to\glfw\lib -Lpath\to\glew\lib -lglew32 -lglfw3 -lopengl32 -lgdi32

Notes & Improvements you can make:
 - This example uses legacy immediate-mode OpenGL for simplicity. For production, use modern OpenGL with shaders (GLSL) and VAOs/VBOs.
 - Add proper collision resolution so player cannot walk through enemies.
 - Add different enemy types, animations, sound effects (use e.g., OpenAL or SDL_mixer), particle effects.
 - Add nicer UI, score, and levels.
 - Replace the debug ray with actual muzzle flash and impact effects. The hitscan logic is implemented in ray_vs_circle.

Controls:
 - WASD to move
 - Mouse to aim
 - Left click to fire (hitscan)

*/

