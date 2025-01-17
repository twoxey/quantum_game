#include "utils.h"

rgba32 Color(float r, float g, float b, float a = 1);
font* Font(const char* file_name, float size);
texture Image(const char* file_name);
void reset_transform();
void set_transform(mat3 transform);
void draw_rect(float w, float h, mat3 transform, rgba32 color);
void draw_rect(float x, float y, float w, float h, rgba32 color);
void draw_ellipse(float x, float y, float w, float h, rgba32 color);
void draw_circle(float x, float y, float r, rgba32 color);
void draw_line(float x0, float y0, float x1, float y1, float width, rgba32 color);
void stroke_rect(float x, float y, float w, float h, float width, rgba32 color);
void draw_texture(float x, float y, float scale_factor, texture tex);
void draw_text(const char* text, float x, float y, float size, rgba32 c, font* font);

#include "stdio.h"

font* text_font;
vec2 pos, vel;
float t;

void on_load() {
    text_font = Font("assets/Lato-Regular.ttf", 50);
    t = 0;
    pos = v2(0);
    vel = v2(250);
}

bool check_range_and_clamp(float* val, float lo, float hi) {
    if (*val < lo || *val > hi) {
        *val = clamp(*val, lo, hi);
        return true;
    }
    return false;
}

extern "C" FN_draw_frame(draw_frame) {
    (void)inputs;

    t += dt;
    reset_transform();

    vec2 view = v2(800, 600);
    vec2 top_left = 0.5*(v2(window_width, window_height)-view);

    stroke_rect(top_left.x, top_left.y, view.x, view.y, 5, Color(1, 1, 1));

    float size = 30;
    pos = pos + dt * vel;
    if (check_range_and_clamp(&pos.x, -.5 * view.x + .5 * size, .5 * view.x - .5 * size))
        vel.x = -vel.x;
    if (check_range_and_clamp(&pos.y, -.5 * view.y + .5 * size, .5 * view.y - .5 * size))
        vel.y = -vel.y;
    mat3 transform = m3_translation(.5 * v2(window_width, window_height));
    mat3 obj_transform = m3_translation(pos) * m3_rotation(3 * t * M_PI);
    set_transform(transform * obj_transform);
    draw_ellipse(0, 0, size * 2, size, Color(1, 0, 1));
    reset_transform();

    char buff[1024];
    float text_size = 50;
    snprintf(buff, sizeof(buff), "Time: %.2fs", t);
    draw_text(buff, 0, text_size, text_size, Color(1, 1, 1), text_font);
    snprintf(buff, sizeof(buff), "screen: %d X %d", window_width, window_height);
    draw_text(buff, 0, 2*text_size, text_size, Color(1, 1, 1), text_font);
}
