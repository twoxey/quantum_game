#include "utils.h"
#include "utf8.h"
#include "draw.cpp"

static game_data* game;

static size_t transform_count = 0;
static mat3 transform_stack[10];

rgba32 Color(float r, float g, float b, float a) {
    rgba32 result;
    result.r = clamp(r, 0, 1) * 255;
    result.g = clamp(g, 0, 1) * 255;
    result.b = clamp(b, 0, 1) * 255;
    result.a = clamp(a, 0, 1) * 255;
    return result;
}

image Image(const char* file_name) {
    Asset* a = game->get_asset(Asset_Image, file_name);
    assert(a);
    return a->i;
}

font* Font(const char* file_name) {
    Asset* a = game->get_asset(Asset_Font, file_name);
    assert(a);
    return a->f;
}

#define random_value(type) (*(type*)game->get_random(sizeof(type)))

float random_float_between_0_and_1() {
    return (float)random_value(uint32_t) / UINT32_MAX;
}

void reset_transform() {
    transform_count = 0;
}

void push_transform(mat3 t) {
    assert(transform_count < ARRAY_LEN(transform_stack));
    transform_stack[transform_count++] = t;
}

void pop_transform() {
    assert(transform_count > 0);
    --transform_count;
}

mat3 get_transform() {
    mat3 t = m3_identity();
    for (int i = transform_count - 1; i >= 0; --i) t = transform_stack[i] * t;
    return t;
}

void push_quad(quad_type type, mat3 transform, uint32_t texture_id, rgba32 c0, rgba32 c1, rgba32 c2, rgba32 c3, vec4 params) {
    transform = get_transform() * transform;
    game->push_quad(type, transform, texture_id, c0, c1, c2, c3, params);
}

void push_quad(quad_type type, mat3 transform, rgba32 c) {
    push_quad(type, transform, 0, c, c, c, c, {});
}

void draw_rect(float x, float y, float w, float h, rgba32 color) {
    mat3 transform = m3_translation(v2(x, y) + 0.5 * v2(w, h)) * m3_scale(w, h);
    push_quad(QUAD_rect, transform, color);
}

void draw_ellipse(float x, float y, float w, float h, rgba32 color) {
    push_quad(QUAD_ellipse, m3_translation(x, y) * m3_scale(w, h), color);
}

void draw_circle(float x, float y, float r, rgba32 color) {
    draw_ellipse(x, y, r * 2, r * 2, color);
}

void draw_line(float x0, float y0, float x1, float y1, float width, rgba32 color) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float a = atan2f(dy, dx);
    float len = hypotf(dx, dy);
    mat3 transform = m3_translation(v2(x0, y0) + 0.5 * v2(dx, dy)) * m3_rotation(a) * m3_scale(len, width);
    push_quad(QUAD_rect, transform, color);
}

void stroke_rect(float x, float y, float w, float h, float width, rgba32 color) {
    draw_line(x - width / 2, y    , x + w + width / 2, y    , width, color);
    draw_line(x - width / 2, y + h, x + w + width / 2, y + h, width, color);
    draw_line(x    , y - width / 2, x    , y + h + width / 2, width, color);
    draw_line(x + w, y - width / 2, x + w, y + h + width / 2, width, color);
}

void draw_arc(float x, float y, float r, float min_angle, float max_angle, float width, rgba32 color) {
    float size = 2 * r + width;
    mat3 transform = m3_translation(x, y) * m3_scale(v2(size));
    vec4 params;
    params.f[0] = (size - 2 * width) / size * .5; // outline min_threshold
    params.f[1] = 0.5;                            // outline max_threshold
    if (!isinf(min_angle)) {
        while (min_angle < -M_PI) min_angle += 2 * M_PI;
        while (min_angle >  M_PI) min_angle -= 2 * M_PI;;
    }
    if (!isinf(max_angle)) {
        while (max_angle < -M_PI) max_angle += 2 * M_PI;
        while (max_angle >  M_PI) max_angle -= 2 * M_PI;
    }
    // arc min angle and max angle, range from [-PI, PI],
    params.f[2] = min_angle;
    params.f[3] = max_angle;
    push_quad(QUAD_arc, transform, 0, color, color, color, color, params);
}

void stroke_circle(float x, float y, float r, float width, rgba32 color) {
    draw_arc(x, y, r, -INFINITY, INFINITY, width, color);
}

void draw_image(float x, float y, float scale_factor, image img) {
    mat3 transform = m3_translation(x, y) *
                     m3_scale(v2(scale_factor)) *
                     m3_scale(img.w, img.h);
    push_quad(QUAD_image, transform, img.id, {}, {}, {}, {}, {});
}

void draw_text(const char* text, float x, float y, float size, rgba32 c, font* f) {
    UTF8_Decoder dec = utf8_decode(text);
    vec2 pos = v2(x, y);
    for (uint32_t codep; (codep = utf8_next(&dec));) {
        font_quad q = game->font_get_quad(f, codep, size);
        mat3 transform = m3_translation(pos) *
                         m3_translation(.5 * (q.p_min + q.p_max)) *
                         m3_scale(q.p_max - q.p_min);
        push_quad(QUAD_char, transform, q.texture, c, c, c, c, {});
        pos = pos + q.p_next;
    }
}

void draw_obround(float x, float y, float w, float h, rgba32 color) {
    vec2 p0, p1;
    float r;
    if (w > h) {
        r = h / 2;
        w -= h;
        p0 = v2(x - w / 2, y);
        p1 = v2(x + w / 2, y);
    } else {
        r = w / 2;
        h -= w;
        p0 = v2(x, y - w / 2);
        p1 = v2(y, y + w / 2);
    }
    push_quad(QUAD_rect, m3_translation(x, y) * m3_scale(w, h), color);
    draw_circle(p0.x, p0.y, r, color);
    draw_circle(p1.x, p1.y, r, color);
}

void draw_line_round_cap(float x0, float y0, float x1, float y1, float width, rgba32 color) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float a = atan2f(dy, dx);
    float len = hypotf(dx, dy);
    push_transform(m3_translation(v2(x0, y0) + 0.5 * v2(dx, dy)) * m3_rotation(a));
    draw_obround(0, 0, len + width, width, color);
    pop_transform();
}


extern "C" FN_draw_frame(draw_frame) {
    reset_transform();
    global_time += dt;
    draw(dt, win_w, win_h, inputs);
}

extern "C" FN_on_load(on_load) {
    game = data;
    assert(game->get_random);
    assert(game->get_asset);
    assert(game->font_get_quad);
    assert(game->push_quad);

    game->draw_frame = draw_frame;

    on_load();
}
