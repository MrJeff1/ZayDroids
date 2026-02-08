import math
import random
from pyray import *
import sys
import os

def resource_path(relative_path: str) -> str:
    if getattr(sys, "frozen", False):
        # PyInstaller extracts to this temp folder
        base_path = sys._MEIPASS
    else:
        # Normal Python execution
        base_path = os.path.abspath(".")
    _return = os.path.join(base_path, relative_path)
    return _return

SCREEN_WIDTH = 900
SCREEN_HEIGHT = 650

SHIP_RADIUS = 12
SHIP_TURN_SPEED = 3.5  # radians/sec
SHIP_ACCEL = 260
SHIP_FRICTION = 0.98
SHIP_MAX_SPEED = 360

BULLET_SPEED = 520
BULLET_LIFETIME = 1.2
BULLET_COOLDOWN = 0.18

ASTEROID_BASE_SPEED = 40
ASTEROID_SIZES = [3, 2, 1]  # 3=large,2=med,1=small
ASTEROID_RADII = {3: 42, 2: 26, 1: 14}

LIVES_START = 3


def wrap_position(pos: Vector2) -> Vector2:
    if pos.x < 0:
        pos.x += SCREEN_WIDTH
    elif pos.x > SCREEN_WIDTH:
        pos.x -= SCREEN_WIDTH
    if pos.y < 0:
        pos.y += SCREEN_HEIGHT
    elif pos.y > SCREEN_HEIGHT:
        pos.y -= SCREEN_HEIGHT
    return pos


def vec_from_angle(angle: float) -> Vector2:
    return Vector2(math.cos(angle), math.sin(angle))


def vec_len(v: Vector2) -> float:
    return math.hypot(v.x, v.y)


def vec_scale(v: Vector2, s: float) -> Vector2:
    return Vector2(v.x * s, v.y * s)


def vec_add(a: Vector2, b: Vector2) -> Vector2:
    return Vector2(a.x + b.x, a.y + b.y)


def vec_sub(a: Vector2, b: Vector2) -> Vector2:
    return Vector2(a.x - b.x, a.y - b.y)


def vec_clamp_length(v: Vector2, max_len: float) -> Vector2:
    length = vec_len(v)
    if length > max_len and length > 0:
        scale = max_len / length
        return vec_scale(v, scale)
    return v


def circle_collision(p1: Vector2, r1: float, p2: Vector2, r2: float) -> bool:
    dx = p1.x - p2.x
    dy = p1.y - p2.y
    return (dx * dx + dy * dy) <= (r1 + r2) * (r1 + r2)


def random_asteroid_velocity(size: int) -> Vector2:
    angle = random.uniform(0, math.tau)
    speed = ASTEROID_BASE_SPEED + random.uniform(0, 40) + (3 - size) * 20
    return vec_scale(vec_from_angle(angle), speed)


class Player:
    def __init__(self):
        self.pos = Vector2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2)
        self.vel = Vector2(0, 0)
        self.angle = -math.pi / 2
        self.cooldown = 0.0
        self.invuln = 0.0
        self.alive = True

    def reset(self):
        self.pos = Vector2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2)
        self.vel = Vector2(0, 0)
        self.angle = -math.pi / 2
        self.cooldown = 0.0
        self.invuln = 2.0
        self.alive = True

    def update(self, dt: float):
        if is_key_down(KEY_LEFT) or is_key_down(KEY_A):
            self.angle -= SHIP_TURN_SPEED * dt
        if is_key_down(KEY_RIGHT) or is_key_down(KEY_D):
            self.angle += SHIP_TURN_SPEED * dt

        if is_key_down(KEY_UP) or is_key_down(KEY_W):
            thrust = vec_scale(vec_from_angle(self.angle), SHIP_ACCEL * dt)
            self.vel = vec_add(self.vel, thrust)

        self.vel = vec_scale(self.vel, SHIP_FRICTION ** (dt * 60))
        self.vel = vec_clamp_length(self.vel, SHIP_MAX_SPEED)

        self.pos = vec_add(self.pos, vec_scale(self.vel, dt))
        self.pos = wrap_position(self.pos)

        if self.cooldown > 0:
            self.cooldown -= dt
        if self.invuln > 0:
            self.invuln -= dt

    def shoot(self):
        if self.cooldown <= 0:
            dir_vec = vec_from_angle(self.angle)
            bullet_pos = vec_add(self.pos, vec_scale(dir_vec, SHIP_RADIUS + 6))
            bullet_vel = vec_add(self.vel, vec_scale(dir_vec, BULLET_SPEED))
            self.cooldown = BULLET_COOLDOWN
            return Bullet(bullet_pos, bullet_vel)
        return None

    def draw(self):
        dir_vec = vec_from_angle(self.angle)
        right_vec = vec_from_angle(self.angle + 2.5)
        left_vec = vec_from_angle(self.angle - 2.5)

        p1 = vec_add(self.pos, vec_scale(dir_vec, SHIP_RADIUS + 8))
        p2 = vec_add(self.pos, vec_scale(right_vec, SHIP_RADIUS))
        p3 = vec_add(self.pos, vec_scale(left_vec, SHIP_RADIUS))

        color = WHITE
        if self.invuln > 0 and int(self.invuln * 10) % 2 == 0:
            color = Color(255, 255, 255, 80)

        draw_triangle(p1, p2, p3, color)
        draw_triangle_lines(p1, p2, p3, SKYBLUE)


class Bullet:
    def __init__(self, pos: Vector2, vel: Vector2):
        self.pos = Vector2(pos.x, pos.y)
        self.vel = Vector2(vel.x, vel.y)
        self.life = BULLET_LIFETIME

    def update(self, dt: float):
        self.pos = vec_add(self.pos, vec_scale(self.vel, dt))
        self.pos = wrap_position(self.pos)
        self.life -= dt

    def draw(self):
        draw_circle_v(self.pos, 2, YELLOW)


class Asteroid:
    def __init__(self, pos: Vector2, size: int):
        self.pos = Vector2(pos.x, pos.y)
        self.size = size
        self.radius = ASTEROID_RADII[size]
        self.vel = random_asteroid_velocity(size)
        self.points = self._generate_shape()

    def _generate_shape(self):
        count = random.randint(10, 14)
        points = []
        for i in range(count):
            angle = (i / count) * math.tau
            r = self.radius * random.uniform(0.7, 1.1)
            points.append(Vector2(math.cos(angle) * r, math.sin(angle) * r))
        return points

    def update(self, dt: float):
        self.pos = vec_add(self.pos, vec_scale(self.vel, dt))
        self.pos = wrap_position(self.pos)

    def draw(self):
        count = len(self.points)
        for i in range(count):
            a = self.points[i]
            b = self.points[(i + 1) % count]
            pa = vec_add(self.pos, a)
            pb = vec_add(self.pos, b)
            draw_line_v(pa, pb, LIGHTGRAY)


class Game:
    def __init__(self):
        self.player = Player()
        self.bullets = []
        self.asteroids = []
        self.score = 0
        self.lives = LIVES_START
        self.wave = 1
        self.game_over = False
        self.spawn_wave(self.wave)

    def spawn_wave(self, wave: int):
        self.asteroids = []
        count = 3 + wave
        for _ in range(count):
            while True:
                pos = Vector2(random.uniform(0, SCREEN_WIDTH), random.uniform(0, SCREEN_HEIGHT))
                if not circle_collision(pos, 80, self.player.pos, 120):
                    break
            self.asteroids.append(Asteroid(pos, 3))

    def reset(self):
        self.score = 0
        self.lives = LIVES_START
        self.wave = 1
        self.game_over = False
        self.player.reset()
        self.bullets = []
        self.spawn_wave(self.wave)

    def update(self, dt: float):
        if self.game_over:
            if is_key_pressed(KEY_ENTER):
                self.reset()
            return

        self.player.update(dt)

        if is_key_down(KEY_SPACE) or is_mouse_button_pressed(0):
            bullet = self.player.shoot()
            if bullet:
                self.bullets.append(bullet)

        for bullet in self.bullets:
            bullet.update(dt)
        self.bullets = [b for b in self.bullets if b.life > 0]

        for asteroid in self.asteroids:
            asteroid.update(dt)

        self.handle_collisions()

        if not self.asteroids:
            self.wave += 1
            self.player.invuln = 2.0
            self.spawn_wave(self.wave)

    def handle_collisions(self):
        # Bullets vs Asteroids
        new_asteroids = []
        remaining_asteroids = []
        for asteroid in self.asteroids:
            hit = False
            for bullet in self.bullets:
                if circle_collision(bullet.pos, 2, asteroid.pos, asteroid.radius):
                    bullet.life = 0
                    hit = True
                    self.score += 10 * asteroid.size
                    if asteroid.size > 1:
                        for _ in range(2):
                            child = Asteroid(asteroid.pos, asteroid.size - 1)
                            child.vel = random_asteroid_velocity(asteroid.size - 1)
                            new_asteroids.append(child)
                    break
            if not hit:
                remaining_asteroids.append(asteroid)

        self.asteroids = remaining_asteroids + new_asteroids
        self.bullets = [b for b in self.bullets if b.life > 0]

        # Player vs Asteroids
        if self.player.invuln <= 0:
            for asteroid in self.asteroids:
                if circle_collision(self.player.pos, SHIP_RADIUS, asteroid.pos, asteroid.radius):
                    self.lives -= 1
                    self.player.reset()
                    if self.lives <= 0:
                        self.game_over = True
                    break

    def draw(self):
        for asteroid in self.asteroids:
            asteroid.draw()
        for bullet in self.bullets:
            bullet.draw()
        if not self.game_over or self.player.invuln > 0:
            self.player.draw()

        draw_text(f"Score: {self.score}", 20, 20, 20, RAYWHITE)
        draw_text(f"Lives: {self.lives}", 20, 45, 20, RAYWHITE)
        draw_text(f"Wave: {self.wave}", 20, 70, 20, RAYWHITE)

        if self.game_over:
            text = "GAME OVER"
            sub = "Press ENTER to restart"
            w = measure_text(text, 48)
            sw = measure_text(sub, 20)
            draw_text(text, (SCREEN_WIDTH - w) // 2, SCREEN_HEIGHT // 2 - 40, 48, RED)
            draw_text(sub, (SCREEN_WIDTH - sw) // 2, SCREEN_HEIGHT // 2 + 20, 20, RAYWHITE)


init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "ZayfireStudios - ZayDroids")
set_target_fps(60)
random.seed()

icon_path = resource_path("favicon.png")
icon = load_image(resource_path(icon_path))
set_window_icon(icon)
unload_image(icon)

game = Game()

while not window_should_close():
    dt = get_frame_time()

    begin_drawing()
    clear_background(Color(10, 12, 20, 255))

    game.update(dt)
    game.draw()

    end_drawing()

close_window()
