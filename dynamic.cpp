#include "stdint.h"
#include "stdio.h"

#define ARRAY_LEN(a) (sizeof(a)/sizeof*(a))

typedef struct buffer {
    char* ptr;
    size_t len;
} buffer, string;

typedef union rgba32 {
    uint32_t u32;
    struct { uint8_t r, g, b, a; };
} rgba32;

typedef struct texture {
    uint32_t id;
    int w, h;
} texture;

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define FONT_CHAR_START ' '
typedef struct font {
    texture tex;
    float size;
    stbtt_bakedchar backed_chars['~'-FONT_CHAR_START]; // printable ascii chars
} font;

typedef struct game_inputs {
    int mouse_x, mouse_y;
    bool mouse_down;
} game_inputs;

#include "math_helper.h"
#include "draw.cpp"

#include "create_opengl_window.h"

static mat3 global_transform;

buffer read_entire_file_and_null_terminate(const char* file_path) {
    buffer result = {};

    HANDLE file = CreateFileA(file_path, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return result;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size)) goto defer;

{   // c++ wtf ?
    char* mem = (char*)malloc(file_size.QuadPart + 1);
    if (!mem) goto defer;

    DWORD bytes_read;
    if (ReadFile(file, mem, file_size.QuadPart, &bytes_read, 0) && bytes_read == file_size.QuadPart) {
        result.len = bytes_read;
        result.ptr = mem;
        result.ptr[result.len] = 0;
    }
    else free(mem);
}

defer:
    CloseHandle(file);
    return result;
}

GLuint create_shader_program(const char* header, const char* vertex_source_string, const char* fragment_source_string) {
    char buff[512];
    GLint success;

    const GLchar* vertex_shader_source[] = {header, vertex_source_string};
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 2, vertex_shader_source, 0);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, sizeof(buff), NULL, buff);
        fprintf(stderr, "Error: vertex shader:\n%s\n", buff);
        return 0;
    }

    const GLchar* fragment_shader_source[] = {header, fragment_source_string};
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 2, fragment_shader_source, 0);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, sizeof(buff), NULL, buff);
        fprintf(stderr, "Error: fragment shader:\n%s\n", buff);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(buff), NULL, buff);
        fprintf(stderr, "Error: linking program:\n%s\n", buff);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
texture load_image(const char* file_name) {
    texture result = {};

    int x, y, n;
    unsigned char *data = stbi_load(file_name, &x, &y, &n, 0);
    if (!data) return result;
    assert(n == 3);

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    if (!tex_id) goto defer;

    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    result.id = tex_id;
    result.w = x;
    result.h = y;

defer:
    stbi_image_free(data);

    return result;
}

#define FONT_BITMAP_W 512
#define FONT_BITMAP_H 512
font* load_font(const char* file_name, float font_size) {
    buffer font_data = read_entire_file_and_null_terminate(file_name);
    if (!font_data.len) return 0;

    font* f = (font*)malloc(sizeof(font));
    if (!f) return 0;

    unsigned char temp_bitmap[FONT_BITMAP_W*FONT_BITMAP_H];
    int ret = stbtt_BakeFontBitmap((unsigned char*)font_data.ptr, 0, font_size, temp_bitmap, FONT_BITMAP_W, FONT_BITMAP_H,
                                   FONT_CHAR_START, ARRAY_LEN(f->backed_chars), f->backed_chars);
    free(font_data.ptr);

    if (ret == 0) goto error;
    else if (ret < 0) {
        fprintf(stderr, "Warning: fonts not fit entirely, ret: %d\n", ret);
    }

    glGenTextures(1, &f->tex.id);
    if (!f->tex.id) {
        fprintf(stderr, "Error: failed to gen tex id for font\n");
        goto error;
    }
    glBindTexture(GL_TEXTURE_2D, f->tex.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_BITMAP_W, FONT_BITMAP_H, 0, GL_RED, GL_UNSIGNED_BYTE, temp_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    f->tex.w = FONT_BITMAP_W;
    f->tex.h = FONT_BITMAP_H;
    f->size = font_size;

    return f;

error:
    free(f);
    return 0;
}

rgba32 Color(float r, float g, float b, float a) {
    rgba32 result;
    result.r = clamp(r, 0, 1) * 255;
    result.g = clamp(g, 0, 1) * 255;
    result.b = clamp(b, 0, 1) * 255;
    result.a = clamp(a, 0, 1) * 255;
    return result;
}

font* Font(const char* file_name, float size) {
    font* f = load_font(file_name, size);
    assert(f);
    return f;
}

texture Image(const char* file_name) {
    texture tex = load_image(file_name);
    assert(tex.id);
    return tex;
}

void reset_transform() {
    global_transform = m3_identity();
}

void set_transform(mat3 transform) {
    global_transform = transform;
}

#include "quad_shader.cpp"

void push_quad(uint32_t type, mat3 transform, rgba32 c) {
    push_quad(type, transform, 0, c, c, c, c);
}

void draw_rect(float w, float h, mat3 transform, rgba32 color) {
    push_quad(QUAD_rect, transform * m3_scale(w, h), color);
}

void draw_rect(float x, float y, float w, float h, rgba32 color) {
    draw_rect(w, h, m3_translation(v2(x, y) + 0.5 * v2(w, h)), color);
}

void draw_ellipse(float x, float y, float w, float h, rgba32 color) {
    push_quad(QUAD_ellipse, m3_translation(x, y) * m3_scale(w, h), color);
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
    push_quad(QUAD_image, transform, tex.id, {}, {}, {}, {});
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
        push_quad(QUAD_char, transform, font->tex.id,
                  c, c, c, c, v4(q.s0, q.t0, q.s1, q.t1));
    }
}

// TODO: fix angle representation
void draw_arc(float x, float y, float r, float min_angel, float max_angle, float width, rgba32 color) {
    float size = 2 * r + width;
    mat3 transform = m3_translation(x, y) * m3_scale(v2(size));
    vec4 params;
    params.f[0] = (size - 2 * width) / size * .5; // outline min_threshold
    params.f[1] = 0.5;                            // outline max_threshold
    // arc min angel and max angle, range from [-PI, PI],
    params.f[2] = min_angel;
    params.f[3] = max_angle;
    push_quad(QUAD_arc, transform, 0, color, color, color, color, params);
}

void stroke_circle(float x, float y, float r, float width, rgba32 color) {
    draw_arc(x, y, r, -INFINITY, INFINITY, width, color);
}

// data that needs to be preserved between reload
struct game_data {
    Window* win;
};
static game_data* game;
extern "C" void on_load(char game_memory[]) {
    game = (game_data*)game_memory;
    if (!game->win) {
        game->win = create_window();
    } else {
        load_gl_procs();
    }

    assert(init_quad_program());

    on_load();
}

game_inputs inputs = {};

extern "C" draw_frame(float dt) {
    Window* win = game->win;
    assert(win);

    for(Event event; window_check_event(win, &event);) {
        if (event.type == Event_close || (event.type == Event_key_down && event.key == VK_ESCAPE))
            return false;
        if (event.type == Event_mouse_button_down && event.button == Button_left)
            inputs.mouse_down = true;
        if (event.type == Event_mouse_button_up && event.button == Button_left)
            inputs.mouse_down = false;
    }
    inputs.mouse_x = win->mouse_x;
    inputs.mouse_y = win->mouse_y;

    glViewport(0, 0, win->width, win->height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_frame(dt, win->width, win->height, inputs);

    flush_quads(win->width, win->height);
    window_swap_buffers(win);

    return true;
}
