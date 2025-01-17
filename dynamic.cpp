#define STB_TRUETYPE_IMPLEMENTATION

#include "utils.h"

#include "draw.cpp"

static game_data* game;
static mat3 global_transform;

rgba32 Color(float r, float g, float b, float a) {
    rgba32 result;
    result.r = clamp(r, 0, 1) * 255;
    result.g = clamp(g, 0, 1) * 255;
    result.b = clamp(b, 0, 1) * 255;
    result.a = clamp(a, 0, 1) * 255;
    return result;
}

bool str_eq(string a, const char* b) {
    char* ap = a.ptr;
    for (size_t i = 0; i < a.len; ++i) {
        if (*b++ != *ap++) return false;
    }
    return *b == 0;
}

font* Font(const char* file_name, float size) {
    font* f = game->load_font(file_name, size);
    assert(f);
    return f;
}

texture Image(const char* file_name) {
    texture tex = game->load_image(file_name);
    assert(tex.id);
    return tex;
}

void reset_transform() {
    global_transform = m3_identity();
}

void set_transform(mat3 transform) {
    global_transform = transform;
}

void push_quad(quad_data_buffer* d, uint32_t type, mat3 transform,
               vec2 uv0, vec2 uv1, uint32_t texture_id,
               rgba32 c0, rgba32 c1, rgba32 c2, rgba32 c3) {
    assert((size_t)d->quad_count < ARRAY_LEN(d->instances));
    quad_instance_data* inst = d->instances + d->quad_count;
    quad_vertex_data* vertex = d->vertices + d->quad_count * 4;
    ++d->quad_count;

    inst->type = type;
    inst->texture_id = texture_id;
    inst->transform = global_transform * transform;

    vertex[0] = {v2(uv0.s, uv0.t), c0};
    vertex[1] = {v2(uv1.s, uv0.t), c1};
    vertex[2] = {v2(uv0.s, uv1.t), c2};
    vertex[3] = {v2(uv1.s, uv1.t), c3};
}

void draw_rect(float w, float h, mat3 transform, rgba32 color) {
    transform = transform * m3_scale(w, h);
    push_quad(&game->quad_data, QUAD_rect, transform,
              v2(0), v2(1), 0, color, color, color, color);
}

void draw_rect(float x, float y, float w, float h, rgba32 color) {
    draw_rect(w, h, m3_translation(v2(x, y) + 0.5 * v2(w, h)), color);
}

void draw_ellipse(float x, float y, float w, float h, rgba32 color) {
    mat3 transform = m3_translation(x, y) * m3_scale(w, h);
    push_quad(&game->quad_data, QUAD_ellipse, transform,
              v2(0), v2(1), 0, color, color, color, color);
}

void draw_circle(float x, float y, float r, rgba32 color) {
    draw_ellipse(x, y, r, r, color);
}

void draw_line(float x0, float y0, float x1, float y1, float width, rgba32 color) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float a = atan2f(dy, dx);
    float len = hypotf(dx, dy);
    mat3 transform = m3_translation(v2(x0, y0) + 0.5 * v2(dx, dy)) *
                     m3_rotation(a);
    draw_rect(len, width, transform, color);
}

void stroke_rect(float x, float y, float w, float h, float width, rgba32 color) {
    draw_line(x - width / 2, y    , x + w + width / 2, y    , width, color);
    draw_line(x - width / 2, y + h, x + w + width / 2, y + h, width, color);
    draw_line(x    , y - width / 2, x    , y + h + width / 2, width, color);
    draw_line(x + w, y - width / 2, x + w, y + h + width / 2, width, color);
}

void draw_texture(float x, float y, float scale_factor, texture tex) {
    mat3 transform = m3_translation(x, y) *
                     m3_scale(v2(scale_factor)) *
                     m3_scale(tex.w, tex.h);
    push_quad(&game->quad_data, QUAD_image, transform, v2(0), v2(1), tex.id, {}, {}, {}, {});
}

void draw_text(const char* text, float x, float y, float size, rgba32 c, font* font) {
    float scale_factor = size / font->size;
    float x0 = 0;
    float y0 = 0;
    for (char ch; (ch = *text); text++) {
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->backed_chars, font->tex.w, font->tex.h, ch - FONT_CHAR_START, &x0, &y0, &q, true);

        vec2 p0 = v2(q.x0, q.y0);
        vec2 p1 = v2(q.x1, q.y1);
        mat3 transform = m3_translation(x, y) *
                         m3_scale(v2(scale_factor)) *
                         m3_translation(0.5 * (p0 + p1)) *
                         m3_scale(p1 - p0);
        push_quad(&game->quad_data, QUAD_char, transform,
                  v2(q.s0, q.t0), v2(q.s1, q.t1), font->tex.id, c, c, c, c);
    }
}


extern "C" void on_load(game_data* game_memory) {
    game = game_memory;

    on_load();
}
