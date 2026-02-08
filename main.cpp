// Required includes
#include "raylib.h"
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdlib>
#include <map>

// Platform-specific includes for resource path handling
#ifdef _WIN32
#include <windows.h>
#endif

// Function to get resource path (equivalent to Python's resource_path function)
std::string resource_path(const std::string &relative_path)
{
    std::string base_path;

#ifdef _WIN32
    // Check if running as a bundled executable
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string exe_path(buffer);
    size_t pos = exe_path.find_last_of("\\/");
    if (pos != std::string::npos)
    {
        base_path = exe_path.substr(0, pos);
    }
    else
    {
        base_path = ".";
    }
#else
    // For other platforms, use current directory
    base_path = ".";
#endif

    // Combine base path with relative path
    std::string result = base_path;
    if (!result.empty() && result.back() != '/' && result.back() != '\\')
    {
        result += "/";
    }
    result += relative_path;
    return result;
}

// Constants
const int SCREEN_WIDTH = 900;
const int SCREEN_HEIGHT = 650;

const float SHIP_RADIUS = 12.0f;
const float SHIP_TURN_SPEED = 3.5f; // radians/sec
const float SHIP_ACCEL = 260.0f;
const float SHIP_FRICTION = 0.98f;
const float SHIP_MAX_SPEED = 360.0f;

const float BULLET_SPEED = 520.0f;
const float BULLET_LIFETIME = 1.2f;
const float BULLET_COOLDOWN = 0.18f;

const float ASTEROID_BASE_SPEED = 40.0f;
const std::vector<int> ASTEROID_SIZES = {3, 2, 1}; // 3=large,2=med,1=small
const std::map<int, float> ASTEROID_RADII = {{3, 42.0f}, {2, 26.0f}, {1, 14.0f}};

const int LIVES_START = 3;

// Random number generator
std::random_device rd;
std::mt19937 gen(rd());

// Utility functions
Vector2 wrap_position(Vector2 pos)
{
    if (pos.x < 0)
    {
        pos.x += SCREEN_WIDTH;
    }
    else if (pos.x > SCREEN_WIDTH)
    {
        pos.x -= SCREEN_WIDTH;
    }
    if (pos.y < 0)
    {
        pos.y += SCREEN_HEIGHT;
    }
    else if (pos.y > SCREEN_HEIGHT)
    {
        pos.y -= SCREEN_HEIGHT;
    }
    return pos;
}

Vector2 vec_from_angle(float angle)
{
    return Vector2{std::cos(angle), std::sin(angle)};
}

float vec_len(Vector2 v)
{
    return std::hypot(v.x, v.y);
}

Vector2 vec_scale(Vector2 v, float s)
{
    return Vector2{v.x * s, v.y * s};
}

Vector2 vec_add(Vector2 a, Vector2 b)
{
    return Vector2{a.x + b.x, a.y + b.y};
}

Vector2 vec_sub(Vector2 a, Vector2 b)
{
    return Vector2{a.x - b.x, a.y - b.y};
}

Vector2 vec_clamp_length(Vector2 v, float max_len)
{
    float length = vec_len(v);
    if (length > max_len && length > 0)
    {
        float scale = max_len / length;
        return vec_scale(v, scale);
    }
    return v;
}

bool circle_collision(Vector2 p1, float r1, Vector2 p2, float r2)
{
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return (dx * dx + dy * dy) <= (r1 + r2) * (r1 + r2);
}

Vector2 random_asteroid_velocity(int size)
{
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * M_PI);
    std::uniform_real_distribution<float> speed_dist(0.0f, 40.0f);

    float angle = angle_dist(gen);
    float speed = ASTEROID_BASE_SPEED + speed_dist(gen) + (3 - size) * 20.0f;
    return vec_scale(vec_from_angle(angle), speed);
}

// Forward declarations
class Bullet;
class Asteroid;

// Player class
class Player
{
public:
    Vector2 pos;
    Vector2 vel;
    float angle;
    float cooldown;
    float invuln;
    bool alive;

    Player()
    {
        pos = Vector2{SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
        vel = Vector2{0.0f, 0.0f};
        angle = -M_PI / 2.0f;
        cooldown = 0.0f;
        invuln = 0.0f;
        alive = true;
    }

    void reset()
    {
        pos = Vector2{SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
        vel = Vector2{0.0f, 0.0f};
        angle = -M_PI / 2.0f;
        cooldown = 0.0f;
        invuln = 2.0f;
        alive = true;
    }

    void update(float dt)
    {
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
        {
            angle -= SHIP_TURN_SPEED * dt;
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
        {
            angle += SHIP_TURN_SPEED * dt;
        }

        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        {
            Vector2 thrust = vec_scale(vec_from_angle(angle), SHIP_ACCEL * dt);
            vel = vec_add(vel, thrust);
        }

        vel = vec_scale(vel, std::pow(SHIP_FRICTION, dt * 60.0f));
        vel = vec_clamp_length(vel, SHIP_MAX_SPEED);

        pos = vec_add(pos, vec_scale(vel, dt));
        pos = wrap_position(pos);

        if (cooldown > 0)
        {
            cooldown -= dt;
        }
        if (invuln > 0)
        {
            invuln -= dt;
        }
    }

    Bullet *shoot(); // Forward declaration, implemented after Bullet class

    void draw()
    {
        Vector2 dir_vec = vec_from_angle(angle);
        Vector2 right_vec = vec_from_angle(angle + 2.5f);
        Vector2 left_vec = vec_from_angle(angle - 2.5f);

        Vector2 p1 = vec_add(pos, vec_scale(dir_vec, SHIP_RADIUS + 8.0f));
        Vector2 p2 = vec_add(pos, vec_scale(right_vec, SHIP_RADIUS));
        Vector2 p3 = vec_add(pos, vec_scale(left_vec, SHIP_RADIUS));

        Color color = WHITE;
        if (invuln > 0 && static_cast<int>(invuln * 10.0f) % 2 == 0)
        {
            color = Color{255, 255, 255, 80};
        }

        DrawTriangle(p1, p2, p3, color);
        DrawTriangleLines(p1, p2, p3, SKYBLUE);
    }
};

// Bullet class
class Bullet
{
public:
    Vector2 pos;
    Vector2 vel;
    float life;

    Bullet(Vector2 pos_, Vector2 vel_)
    {
        pos = Vector2{pos_.x, pos_.y};
        vel = Vector2{vel_.x, vel_.y};
        life = BULLET_LIFETIME;
    }

    void update(float dt)
    {
        pos = vec_add(pos, vec_scale(vel, dt));
        pos = wrap_position(pos);
        life -= dt;
    }

    void draw()
    {
        DrawCircleV(pos, 2.0f, YELLOW);
    }
};

// Implement Player::shoot() after Bullet class definition
Bullet *Player::shoot()
{
    if (cooldown <= 0)
    {
        Vector2 dir_vec = vec_from_angle(angle);
        Vector2 bullet_pos = vec_add(pos, vec_scale(dir_vec, SHIP_RADIUS + 6.0f));
        Vector2 bullet_vel = vec_add(vel, vec_scale(dir_vec, BULLET_SPEED));
        cooldown = BULLET_COOLDOWN;
        return new Bullet(bullet_pos, bullet_vel);
    }
    return nullptr;
}

// Asteroid class
class Asteroid
{
public:
    Vector2 pos;
    int size;
    float radius;
    Vector2 vel;
    std::vector<Vector2> points;

    Asteroid(Vector2 pos_, int size_)
    {
        pos = Vector2{pos_.x, pos_.y};
        size = size_;
        radius = ASTEROID_RADII.at(size);
        vel = random_asteroid_velocity(size);
        points = _generate_shape();
    }

    std::vector<Vector2> _generate_shape()
    {
        std::uniform_int_distribution<int> count_dist(10, 14);
        std::uniform_real_distribution<float> radius_dist(0.7f, 1.1f);

        int count = count_dist(gen);
        std::vector<Vector2> pts;
        for (int i = 0; i < count; i++)
        {
            float angle = (static_cast<float>(i) / static_cast<float>(count)) * 2.0f * M_PI;
            float r = radius * radius_dist(gen);
            pts.push_back(Vector2{std::cos(angle) * r, std::sin(angle) * r});
        }
        return pts;
    }

    void update(float dt)
    {
        pos = vec_add(pos, vec_scale(vel, dt));
        pos = wrap_position(pos);
    }

    void draw()
    {
        int count = points.size();
        for (int i = 0; i < count; i++)
        {
            Vector2 a = points[i];
            Vector2 b = points[(i + 1) % count];
            Vector2 pa = vec_add(pos, a);
            Vector2 pb = vec_add(pos, b);
            DrawLineV(pa, pb, LIGHTGRAY);
        }
    }
};

// Game class
class Game
{
public:
    Player player;
    std::vector<Bullet *> bullets;
    std::vector<Asteroid *> asteroids;
    int score;
    int lives;
    int wave;
    bool game_over;

    Game()
    {
        score = 0;
        lives = LIVES_START;
        wave = 1;
        game_over = false;
        spawn_wave(wave);
    }

    ~Game()
    {
        // Clean up dynamically allocated bullets and asteroids
        for (Bullet *bullet : bullets)
        {
            delete bullet;
        }
        for (Asteroid *asteroid : asteroids)
        {
            delete asteroid;
        }
    }

    void spawn_wave(int wave_num)
    {
        // Clean up existing asteroids
        for (Asteroid *asteroid : asteroids)
        {
            delete asteroid;
        }
        asteroids.clear();

        int count = 3 + wave_num;
        std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(SCREEN_WIDTH));
        std::uniform_real_distribution<float> y_dist(0.0f, static_cast<float>(SCREEN_HEIGHT));

        for (int i = 0; i < count; i++)
        {
            Vector2 pos;
            while (true)
            {
                pos = Vector2{x_dist(gen), y_dist(gen)};
                if (!circle_collision(pos, 80.0f, player.pos, 120.0f))
                {
                    break;
                }
            }
            asteroids.push_back(new Asteroid(pos, 3));
        }
    }

    void reset()
    {
        score = 0;
        lives = LIVES_START;
        wave = 1;
        game_over = false;
        player.reset();

        // Clean up bullets
        for (Bullet *bullet : bullets)
        {
            delete bullet;
        }
        bullets.clear();

        spawn_wave(wave);
    }

    void update(float dt)
    {
        if (game_over)
        {
            if (IsKeyPressed(KEY_ENTER))
            {
                reset();
            }
            return;
        }

        player.update(dt);

        if (IsKeyDown(KEY_SPACE) || IsMouseButtonPressed(0))
        {
            Bullet *bullet = player.shoot();
            if (bullet)
            {
                bullets.push_back(bullet);
            }
        }

        for (Bullet *bullet : bullets)
        {
            bullet->update(dt);
        }

        // Remove bullets with life <= 0
        bullets.erase(
            std::remove_if(bullets.begin(), bullets.end(),
                           [](Bullet *b)
                           {
                               if (b->life <= 0)
                               {
                                   delete b;
                                   return true;
                               }
                               return false;
                           }),
            bullets.end());

        for (Asteroid *asteroid : asteroids)
        {
            asteroid->update(dt);
        }

        handle_collisions();

        if (asteroids.empty())
        {
            wave++;
            player.invuln = 2.0f;
            spawn_wave(wave);
        }
    }

    void handle_collisions()
    {
        // Bullets vs Asteroids
        std::vector<Asteroid *> new_asteroids;
        std::vector<Asteroid *> remaining_asteroids;

        for (Asteroid *asteroid : asteroids)
        {
            bool hit = false;
            for (Bullet *bullet : bullets)
            {
                if (circle_collision(bullet->pos, 2.0f, asteroid->pos, asteroid->radius))
                {
                    bullet->life = 0;
                    hit = true;
                    score += 10 * asteroid->size;
                    if (asteroid->size > 1)
                    {
                        for (int i = 0; i < 2; i++)
                        {
                            Asteroid *child = new Asteroid(asteroid->pos, asteroid->size - 1);
                            child->vel = random_asteroid_velocity(asteroid->size - 1);
                            new_asteroids.push_back(child);
                        }
                    }
                    break;
                }
            }
            if (!hit)
            {
                remaining_asteroids.push_back(asteroid);
            }
            else
            {
                delete asteroid;
            }
        }

        asteroids = remaining_asteroids;
        asteroids.insert(asteroids.end(), new_asteroids.begin(), new_asteroids.end());

        // Remove bullets with life <= 0
        bullets.erase(
            std::remove_if(bullets.begin(), bullets.end(),
                           [](Bullet *b)
                           {
                               if (b->life <= 0)
                               {
                                   delete b;
                                   return true;
                               }
                               return false;
                           }),
            bullets.end());

        // Player vs Asteroids
        if (player.invuln <= 0)
        {
            for (Asteroid *asteroid : asteroids)
            {
                if (circle_collision(player.pos, SHIP_RADIUS, asteroid->pos, asteroid->radius))
                {
                    lives--;
                    player.reset();
                    if (lives <= 0)
                    {
                        game_over = true;
                    }
                    break;
                }
            }
        }
    }

    void draw()
    {
        for (Asteroid *asteroid : asteroids)
        {
            asteroid->draw();
        }
        for (Bullet *bullet : bullets)
        {
            bullet->draw();
        }
        if (!game_over || player.invuln > 0)
        {
            player.draw();
        }

        DrawText(TextFormat("Score: %d", score), 20, 20, 20, RAYWHITE);
        DrawText(TextFormat("Lives: %d", lives), 20, 45, 20, RAYWHITE);
        DrawText(TextFormat("Wave: %d", wave), 20, 70, 20, RAYWHITE);

        if (game_over)
        {
            const char *text = "GAME OVER";
            const char *sub = "Press ENTER to restart";
            int w = MeasureText(text, 48);
            int sw = MeasureText(sub, 20);
            DrawText(text, (SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 - 40, 48, RED);
            DrawText(sub, (SCREEN_WIDTH - sw) / 2, SCREEN_HEIGHT / 2 + 20, 20, RAYWHITE);
        }
    }
};

// Main function
int main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "ZayfireStudios - ZayDroids");
    SetTargetFPS(60);

    // Load and set window icon
    std::string icon_path = resource_path("favicon.png");
    Image icon = LoadImage(icon_path.c_str());
    SetWindowIcon(icon);
    UnloadImage(icon);

    Game game;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        BeginDrawing();
        ClearBackground(Color{10, 12, 20, 255});

        game.update(dt);
        game.draw();

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
