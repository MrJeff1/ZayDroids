#include "raylib.h"
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#ifndef PLATFORM_WEB
#include "favicon.h"
#endif
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

// --------------------------------------------------
// Constants
// --------------------------------------------------

const int SCREEN_WIDTH = 900;
const int SCREEN_HEIGHT = 650;

const float SHIP_RADIUS = 12.0f;
const float SHIP_TURN_SPEED = 3.5f;
const float SHIP_ACCEL = 260.0f;
const float SHIP_FRICTION = 0.98f;
const float SHIP_MAX_SPEED = 360.0f;

const float BULLET_SPEED = 520.0f;
const float BULLET_LIFETIME = 1.2f;
const float BULLET_COOLDOWN = 0.18f;

const float ASTEROID_BASE_SPEED = 40.0f;

const int LIVES_START = 3;

// --------------------------------------------------
// Utility
// --------------------------------------------------

Vector2 WrapPosition(Vector2 pos)
{
    if (pos.x < 0)
        pos.x += SCREEN_WIDTH;
    else if (pos.x > SCREEN_WIDTH)
        pos.x -= SCREEN_WIDTH;

    if (pos.y < 0)
        pos.y += SCREEN_HEIGHT;
    else if (pos.y > SCREEN_HEIGHT)
        pos.y -= SCREEN_HEIGHT;

    return pos;
}

Vector2 VecFromAngle(float angle)
{
    return {cosf(angle), sinf(angle)};
}

float VecLen(Vector2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

Vector2 VecScale(Vector2 v, float s)
{
    return {v.x * s, v.y * s};
}

Vector2 VecAdd(Vector2 a, Vector2 b)
{
    return {a.x + b.x, a.y + b.y};
}

Vector2 VecClampLength(Vector2 v, float maxLen)
{
    float len = VecLen(v);
    if (len > maxLen && len > 0)
    {
        float scale = maxLen / len;
        return VecScale(v, scale);
    }
    return v;
}

bool CircleCollision(Vector2 p1, float r1, Vector2 p2, float r2)
{
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return (dx * dx + dy * dy) <= (r1 + r2) * (r1 + r2);
}

float RandomRange(float min, float max)
{
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

Vector2 RandomAsteroidVelocity(int size)
{
    float angle = RandomRange(0, PI * 2);
    float speed = ASTEROID_BASE_SPEED + RandomRange(0, 40) + (3 - size) * 20;
    return VecScale(VecFromAngle(angle), speed);
}

// --------------------------------------------------
// Bullet
// --------------------------------------------------

struct Bullet
{
    Vector2 pos;
    Vector2 vel;
    float life;

    Bullet(Vector2 p, Vector2 v)
        : pos(p), vel(v), life(BULLET_LIFETIME) {}

    void Update(float dt)
    {
        pos = VecAdd(pos, VecScale(vel, dt));
        pos = WrapPosition(pos);
        life -= dt;
    }

    void Draw() const
    {
        DrawCircleV(pos, 2, YELLOW);
    }
};

// --------------------------------------------------
// Asteroid
// --------------------------------------------------

struct Asteroid
{
    Vector2 pos;
    Vector2 vel;
    int size;
    float radius;
    std::vector<Vector2> points;

    Asteroid(Vector2 p, int s) : pos(p), size(s)
    {
        radius = (s == 3 ? 42 : s == 2 ? 26
                                       : 14);
        vel = RandomAsteroidVelocity(size);
        GenerateShape();
    }

    void GenerateShape()
    {
        int count = GetRandomValue(10, 14);
        points.clear();
        for (int i = 0; i < count; i++)
        {
            float angle = (float)i / count * PI * 2;
            float r = radius * RandomRange(0.7f, 1.1f);
            points.push_back({cosf(angle) * r, sinf(angle) * r});
        }
    }

    void Update(float dt)
    {
        pos = VecAdd(pos, VecScale(vel, dt));
        pos = WrapPosition(pos);
    }

    void Draw() const
    {
        for (size_t i = 0; i < points.size(); i++)
        {
            Vector2 a = VecAdd(pos, points[i]);
            Vector2 b = VecAdd(pos, points[(i + 1) % points.size()]);
            DrawLineV(a, b, LIGHTGRAY);
        }
    }
};

// --------------------------------------------------
// Player
// --------------------------------------------------

struct Player
{
    Vector2 pos;
    Vector2 vel;
    float angle;
    float cooldown;
    float invuln;
    bool alive;

    Player()
    {
        Reset();
    }

    void Reset()
    {
        pos = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
        vel = {0, 0};
        angle = -PI / 2;
        cooldown = 0;
        invuln = 2.0f;
        alive = true;
    }

    void Update(float dt)
    {
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
            angle -= SHIP_TURN_SPEED * dt;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
            angle += SHIP_TURN_SPEED * dt;

        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        {
            Vector2 thrust = VecScale(VecFromAngle(angle), SHIP_ACCEL * dt);
            vel = VecAdd(vel, thrust);
        }

        vel = VecScale(vel, powf(SHIP_FRICTION, dt * 60));
        vel = VecClampLength(vel, SHIP_MAX_SPEED);

        pos = VecAdd(pos, VecScale(vel, dt));
        pos = WrapPosition(pos);

        if (cooldown > 0)
            cooldown -= dt;
        if (invuln > 0)
            invuln -= dt;
    }

    bool CanShoot() const
    {
        return cooldown <= 0;
    }

    Bullet Shoot()
    {
        cooldown = BULLET_COOLDOWN;
        Vector2 dir = VecFromAngle(angle);
        Vector2 p = VecAdd(pos, VecScale(dir, SHIP_RADIUS + 6));
        Vector2 v = VecAdd(vel, VecScale(dir, BULLET_SPEED));
        return Bullet(p, v);
    }

    void Draw() const
    {
        Vector2 dir = VecFromAngle(angle);
        Vector2 right = VecFromAngle(angle + 2.5f);
        Vector2 left = VecFromAngle(angle - 2.5f);

        Vector2 p1 = VecAdd(pos, VecScale(dir, SHIP_RADIUS + 8));
        Vector2 p2 = VecAdd(pos, VecScale(right, SHIP_RADIUS));
        Vector2 p3 = VecAdd(pos, VecScale(left, SHIP_RADIUS));

        Color c = WHITE;
        if (invuln > 0 && ((int)(invuln * 10) % 2 == 0))
            c = Fade(WHITE, 0.3f);

        DrawTriangle(p1, p2, p3, c);
        DrawTriangleLines(p1, p2, p3, SKYBLUE);
    }
};

// --------------------------------------------------
// Game
// --------------------------------------------------

struct Game
{
    Player player;
    std::vector<Bullet> bullets;
    std::vector<Asteroid> asteroids;
    int score = 0;
    int lives = LIVES_START;
    int wave = 1;
    bool gameOver = false;

    Game()
    {
        SpawnWave();
    }

    void SpawnWave()
    {
        asteroids.clear();
        int count = 3 + wave;

        for (int i = 0; i < count; i++)
        {
            Vector2 pos;
            do
            {
                pos = {RandomRange(0, SCREEN_WIDTH), RandomRange(0, SCREEN_HEIGHT)};
            } while (CircleCollision(pos, 80, player.pos, 120));

            asteroids.emplace_back(pos, 3);
        }
    }

    void Reset()
    {
        score = 0;
        lives = LIVES_START;
        wave = 1;
        gameOver = false;
        player.Reset();
        bullets.clear();
        SpawnWave();
    }

    void Update(float dt)
    {
        if (gameOver)
        {
            if (IsKeyPressed(KEY_ENTER))
                Reset();
            return;
        }

        player.Update(dt);

        if ((IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsGestureDetected(GESTURE_TAP)) && player.CanShoot())
            bullets.push_back(player.Shoot());

        for (auto &b : bullets)
            b.Update(dt);
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                                     [](Bullet &b)
                                     { return b.life <= 0; }),
                      bullets.end());

        for (auto &a : asteroids)
            a.Update(dt);

        HandleCollisions();

        if (asteroids.empty())
        {
            wave++;
            player.invuln = 2.0f;
            SpawnWave();
        }
        if (IsKeyPressed(KEY_F))
        {
#ifdef __EMSCRIPTEN__
            emscripten_request_fullscreen("#canvas", EM_FALSE);
#else
            ToggleFullscreen();
#endif
        }
    }

    void HandleCollisions()
    {
        std::vector<Asteroid> newAsteroids;

        for (auto &a : asteroids)
        {
            bool hit = false;
            for (auto &b : bullets)
            {
                if (CircleCollision(b.pos, 2, a.pos, a.radius))
                {
                    b.life = 0;
                    hit = true;
                    score += 10 * a.size;

                    if (a.size > 1)
                    {
                        for (int i = 0; i < 2; i++)
                            newAsteroids.emplace_back(a.pos, a.size - 1);
                    }
                    break;
                }
            }

            if (!hit)
                newAsteroids.push_back(a);
        }

        asteroids = newAsteroids;

        if (player.invuln <= 0)
        {
            for (auto &a : asteroids)
            {
                if (CircleCollision(player.pos, SHIP_RADIUS, a.pos, a.radius))
                {
                    lives--;
                    player.Reset();
                    if (lives <= 0)
                        gameOver = true;
                    break;
                }
            }
        }
    }

    void Draw() const
    {
        for (auto &a : asteroids)
            a.Draw();
        for (auto &b : bullets)
            b.Draw();
        if (!gameOver || player.invuln > 0)
            player.Draw();

        DrawText(TextFormat("Score: %d", score), 20, 20, 20, RAYWHITE);
        DrawText(TextFormat("Lives: %d", lives), 20, 45, 20, RAYWHITE);
        DrawText(TextFormat("Wave: %d", wave), 20, 70, 20, RAYWHITE);

        if (gameOver)
        {
            const char *t = "GAME OVER";
            const char *s = "Press ENTER to restart";
            DrawText(t, SCREEN_WIDTH / 2 - MeasureText(t, 48) / 2, SCREEN_HEIGHT / 2 - 40, 48, RED);
            DrawText(s, SCREEN_WIDTH / 2 - MeasureText(s, 20) / 2, SCREEN_HEIGHT / 2 + 20, 20, RAYWHITE);
        }
    }
};

// --------------------------------------------------
// Main
// --------------------------------------------------
Game game;

void UpdateDrawFrame()
{
    float dt = GetFrameTime();

    BeginDrawing();
    ClearBackground({10, 12, 20, 255});

    game.Update(dt);
    game.Draw();

    EndDrawing();
}

int main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "ZayfireStudios - ZayDroids");
    SetTargetFPS(60);
    srand((unsigned)time(nullptr));

#if defined(PLATFORM_WEB)
    bool rlDisableVao = true; // Force raylib to skip VAO calls
#endif

#ifndef PLATFORM_WEB
    Image icon = LoadImageFromMemory(
        ".png",
        favicon_png,
        favicon_png_len);
    SetWindowIcon(icon);
    UnloadImage(icon);
#endif

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose())
    {
        UpdateDrawFrame();
    }
    CloseWindow();
#endif

    CloseWindow();
    return 0;
}