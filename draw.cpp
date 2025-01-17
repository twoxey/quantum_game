#include "utils.h"

rgba32 Color(float r, float g, float b, float a = 1);
font* Font(const char* file_name, float size);
texture Image(const char* file_name);
void reset_transform();
void set_transform(mat3 transform);
void draw_rect(float x, float y, float w, float h, rgba32 color);
void draw_ellipse(float x, float y, float w, float h, rgba32 color);
void draw_circle(float x, float y, float r, rgba32 color);
void draw_line(float x0, float y0, float x1, float y1, float width, rgba32 color);
void stroke_rect(float x, float y, float w, float h, float width, rgba32 color);
void draw_texture(float x, float y, float scale_factor, texture tex);
void draw_text(const char* text, float x, float y, float size, rgba32 c, font* font);

void on_load() {
}

float a = 0;
float x = -1;
float y = -1;
bool selected = false;

rgba32 Red = Color(1, 0.5, 0.4);

bool point_in_rect(float x, float y, float min_x, float min_y, float max_x, float max_y) {
    return min_x < x && x < max_x && min_y < y && y < max_y;
}

extern "C" FN_draw_frame(draw_frame) {
    reset_transform();

    draw_rect(0, 0, window_width, window_height, Color(0.3, 0.35, 0.3));

    if (x < 0) {
        x = window_width * 0.5;
        y = window_height * 0.5;
    }

    float size1 = 100;

    if (!selected && inputs.mouse_down && 
        point_in_rect(inputs.mouse_x, inputs.mouse_y,
                      x - size1 * 0.5, y - size1 * 0.5,
                      x + size1 * 0.5, y + size1 * 0.5)) {
        selected = true;
    }

    if (selected) {
        if (!inputs.mouse_down) {
            selected = false;
        }
        x = inputs.mouse_x;
        y = inputs.mouse_y;
    }

    mat3 transform = m3_translation(x, y) * m3_rotation(2 * -a);
    set_transform(transform);
    draw_rect(-0.5 * size1, -0.5 * size1, size1, size1, Red);

    a += dt * 0.5 * 2 * M_PI;
    float size2 = 50;
    set_transform(
        m3_translation(x, y) *
        m3_rotation(a) *
        m3_translation(250, 0)
    );
    draw_rect(- 0.5 * size2, - 0.5 * size2, size2, size2, Color(1, 1, 0));
}
