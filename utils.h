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

#define FN_load_font(fn_name) font* fn_name(const char* file_name, float font_size)
typedef FN_load_font(fn_load_font);

#define FN_load_image(fn_name) texture fn_name(const char* file_name)
typedef FN_load_image(fn_load_image);


#define __UTILES_H__
#endif// __UTILES_H__
