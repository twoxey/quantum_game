static float global_time;
#define INTERVAL(_interval) for (static float last_t; \
                                 !last_t || global_time > last_t + (_interval); \
                                 last_t = global_time)

rgba32 Color(float r, float g, float b, float a = 1);
font Font(const char* file_name, float size = 50);
image Image(const char* file_name);

float random_float_between_0_and_1();

void reset_transform();
void push_transform(mat3 t);
void pop_transform();
mat3 get_transform();

void draw_rect(float x, float y, float w, float h, rgba32 color);
void draw_ellipse(float x, float y, float w, float h, rgba32 color);
void draw_circle(float x, float y, float r, rgba32 color);
void draw_obround(float x, float y, float w, float h, rgba32 color);
void draw_line(float x0, float y0, float x1, float y1, float width, rgba32 color);
void draw_line_round_cap(float x0, float y0, float x1, float y1, float width, rgba32 color);
void draw_arc(float x, float y, float r, float min_angel, float max_angle, float width, rgba32 color);
void stroke_rect(float x, float y, float w, float h, float width, rgba32 color);
void stroke_circle(float x, float y, float r, float width, rgba32 color);
void draw_image(float x, float y, float scale_factor, image img);
void draw_text(const char* text, float x, float y, float size, rgba32 c, font font);

//
// Game code
//

struct FPS_Counter {
    int fps;
    int frame_time_idx;
    float frame_times[10];
};

void update(FPS_Counter* c, float dt) {
    c->frame_times[c->frame_time_idx++] = dt;
    c->frame_time_idx %= ARRAY_LEN(c->frame_times);
    float acc = 0;
    for (size_t i = 0; i < ARRAY_LEN(c->frame_times); ++i) {
        acc += c->frame_times[i];
    }
    c->fps = 1.0 / (acc / ARRAY_LEN(c->frame_times));
}

static FPS_Counter fps_counter;

float v2_len_sqr(vec2 v) {
    return v.x * v.x + v.y * v.y;
}

bool point_in_rect(float x, float y, float min_x, float min_y, float max_x, float max_y) {
    return min_x < x && x < max_x && min_y < y && y < max_y;
}

bool point_in_circle(vec2 point, vec2 center, float radius) {
    return v2_len_sqr(point - center) < radius * radius;
}

vec2 transform_point(vec2 p, mat3 t) {
    return (t * v3(p.x, p.y, 1)).xy;
}

vec2 to_screen_space(vec2 p) {
    return transform_point(p, get_transform());
}

float lerp(float a, float b, float t) {
    return (1 - t) * a + t * b;
}
vec2 lerp(vec2 a, vec2 b, float t) {
    return (1 - t) * a + t * b;
}

float length(vec2 v) {
    return hypotf(v.x, v.y);
}

#include "stdarg.h"
static float text_y = 0;
void println(const char *fmt, ...) {
    float text_size = 20;
    va_list args;
    va_start(args, fmt);
    char text_buff[128];
    vsnprintf(text_buff, sizeof(text_buff), fmt, args);
    draw_text(text_buff, 0, text_y += text_size, text_size, Color(1, 1, 1), Font("Lato-Regular.ttf"));
    va_end(args);
}

struct Particle {
    vec2 pos;
    float pos_max_radius;
    float radius;

    // animation
    bool mouse_is_close;
    vec2 a_pos;
    float dir;

    // particle1
    Particle* parent;
    vec2 d_pos;
};

void update_particle(Particle* p, float dt, vec2 mouse_p, bool mouse_down) {
    bool close =  point_in_circle(mouse_p, to_screen_space(p->pos), p->pos_max_radius);
    p->mouse_is_close = close;
    float d_dir = dt * 5 * (random_float_between_0_and_1());
    if (close) d_dir *= 0.2;
    p->dir += d_dir;
    vec2 move = dt * 200 * v2(cosf(p->dir), sinf(p->dir));
    vec2 diff = p->a_pos - p->pos;
    float dist = length(diff);
    vec2 attract = dt * -100 * diff;
    move = move + dist / p->pos_max_radius * attract;
    p->a_pos = p->a_pos + move;

    bool inside = point_in_circle(mouse_p, to_screen_space(p->a_pos), p->radius);
    rgba32 color = inside ? (mouse_down ? Color(1, 1, 0)
                                        : Color(1, 0, 0))
                          : Color(0, 0, 1);
    draw_circle(p->a_pos.x, p->a_pos.y, p->radius, color);
}

void update_particle1(Particle* p, float dt, vec2 mouse_p, bool mouse_down) {
    bool inside = point_in_circle(mouse_p, to_screen_space(p->a_pos), p->radius);
    bool clicked = inside && mouse_down;
    if (p->parent) {
        float d_dir = dt * M_PI * 1.5;
        if (!clicked && p->parent->mouse_is_close) d_dir *= 0.1;
        p->dir += d_dir;
        vec2 offset = p->pos_max_radius * v2(cosf(p->dir), sinf(p->dir));
        vec2 new_pos = p->parent->a_pos + offset;
        vec2 d_pos = new_pos - p->a_pos;
        if (clicked) {
            p->d_pos = d_pos;
            p->parent = 0;
        } else {
            p->a_pos = new_pos;
        }
    } else {
        p->a_pos = p->a_pos + p->d_pos;
    }
    rgba32 color = inside ? Color(0, 1, 0) : Color(1, 1, 0);
    draw_circle(p->a_pos.x, p->a_pos.y, p->radius, color);
}

static Particle p0;
static Particle p1;
static Particle p2;

void on_load() {
    p0.pos_max_radius = 100;
    p0.radius = 15;

    p1.pos_max_radius = 40;
    p1.radius = 10;
    p1.parent = &p0;

    p2 = p1;
    p2.dir += M_PI;
}

void draw(float dt, int window_width, int window_height, game_inputs inputs) {
    update(&fps_counter, dt);
    text_y = 0;
    vec2 mouse_p = v2(inputs.mouse_x, inputs.mouse_y);

    println("FPS: %d", fps_counter.fps);

    push_transform(m3_translation(window_width / 2, window_height / 2));
    update_particle(&p0, dt, mouse_p, inputs.mouse_down);
    update_particle1(&p1, dt, mouse_p, inputs.mouse_down);
    update_particle1(&p2, dt, mouse_p, inputs.mouse_down);
    // stroke_circle(p0.pos.x, p0.pos.y, max_dist, 2, Color(1, 1, 1));
    pop_transform();

    // println("m_x: %f, m_y: %f", move.x, move.y);
    println("a_x: %.2f, a_y: %.2f", p0.a_pos.x, p0.a_pos.y);
}
