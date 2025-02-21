#include "stdint.h"
#include "stdio.h"
#include "math_helper.h"

#define ARRAY_LEN(a) (sizeof(a)/sizeof*(a))

#if __cplusplus
extern "C" {
#endif

typedef struct buffer {
    char* ptr;
    size_t len;
} buffer, string;
#define S(str_literal) ((string){str_literal, sizeof(str_literal)-1})

typedef union rgba32 {
    uint32_t u32;
    struct { uint8_t r, g, b, a; };
} rgba32;

typedef struct texture {
    uint32_t id;
    int w, h;
} texture, image;

typedef struct font {
    float size;
    texture tex;
    void* font_data;
} font;

struct font_quad {
    vec2 p_min, p_max; // quad
    vec4 tex_coord;
    vec2 p_next; // position of next char
};

enum Asset_Type {
    Asset_Image,
    Asset_Font,
};
struct Asset {
    Asset_Type type;
    string name;
    union {
        image i;
        font f;
    };
};

typedef struct game_data game_data;

typedef struct game_inputs {
    int mouse_x, mouse_y;
    bool mouse_down;
} game_inputs;

#define FN_get_random(fn_name) void* fn_name(size_t num_bytes)
typedef FN_get_random(fn_get_random);

#define FN_get_asset(fn_name) Asset* fn_name(Asset_Type type, const char* name, void* params)
typedef FN_get_asset(fn_get_asset);

#define FN_font_get_quad(fn_name) font_quad fn_name(font f, char c, vec2 pos)
typedef FN_font_get_quad(fn_font_get_quad);

#define FN_on_load(fn_name) void fn_name(game_data* data)
typedef FN_on_load(fn_on_load);

#define FN_draw_frame(fn_name) void fn_name(float dt, int win_w, int win_h, game_inputs inputs)
typedef FN_draw_frame(fn_draw_frame);

typedef enum quad_type {
    QUAD_rect    = 0,
    QUAD_char    = 1,
    QUAD_image   = 2,
    QUAD_ellipse = 3,
    QUAD_arc     = 4,
} quad_type;

#define FN_push_quad(fn_name) void fn_name(quad_type type, mat3 transform, uint32_t texture_id, \
                                           rgba32 c0, rgba32 c1, rgba32 c2, rgba32 c3, vec4 params)
typedef FN_push_quad(fn_push_quad);

// data that needs to be shared with dll
struct game_data {
    // dll provided
    fn_draw_frame* draw_frame;

    // platform provided
    fn_get_random* get_random;
    fn_get_asset* get_asset;
    fn_font_get_quad* font_get_quad;
    fn_push_quad* push_quad;
};


#if __cplusplus
}
#endif
