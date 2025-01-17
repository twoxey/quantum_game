#ifndef __UTILES_H__

#include "stb_truetype.h"

#include "stdint.h"
#include "stdbool.h"

#include "math_helper.h"

#define ARRAY_LEN(a) (sizeof(a)/sizeof*(a))

typedef struct buffer {
    char* ptr;
    size_t len;
} buffer, string;

typedef struct arena {
    char* base;
    size_t capacity;
    size_t used;
} arena;

typedef union rgba32 {
    uint32_t u32;
    struct { uint8_t r, g, b, a; };
} rgba32;

typedef struct texture {
    uint32_t id;
    int w, h;
} texture;

#define FONT_CHAR_START ' '
typedef struct font {
    texture tex;
    float size;
    stbtt_bakedchar backed_chars['~'-FONT_CHAR_START]; // printable ascii chars
} font;

#define QUAD_rect    0
#define QUAD_char    1
#define QUAD_image   2
#define QUAD_ellipse 3

typedef struct quad_vertex_data {
    vec2 uv;
    rgba32 color;
} quad_vertex_data;
typedef struct quad_instance_data {
    mat3 transform;
    uint32_t type;
    uint32_t texture_id;
} quad_instance_data;
typedef struct quad_data_buffer {
    int quad_count;
    quad_instance_data instances[1024];
    quad_vertex_data vertices[ARRAY_LEN(((quad_data_buffer*)0)->instances)];
} quad_data_buffer;

#define FN_load_font(fn_name) font* fn_name(const char* file_name, float font_size)
typedef FN_load_font(fn_load_font);

#define FN_load_image(fn_name) texture fn_name(const char* file_name)
typedef FN_load_image(fn_load_image);

typedef struct game_data {
    arena static_arena;
    fn_load_image* load_image;
    fn_load_font* load_font;

    quad_data_buffer quad_data;
} game_data;

typedef struct game_inputs {
    int mouse_x, mouse_y;
    bool mouse_down;
} game_inputs;

#define FN_on_load(fn_name) void fn_name(game_data* game_memory)
typedef FN_on_load(fn_on_load);

#define FN_draw_frame(fn_name) void fn_name(float dt, int window_width, int window_height, game_inputs inputs)
typedef FN_draw_frame(fn_draw_frame);

#define __UTILES_H__
#endif// __UTILES_H__