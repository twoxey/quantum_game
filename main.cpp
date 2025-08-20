#include "utils.h"

#include "create_opengl_window.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// RNG
#include "Ntdef.h"
#include "bcrypt.h"

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

image load_image(const char* file_name) {
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

//
// Font
//

#define MAX_LOADED_CHAR_COUNT 200
#define LOAD_CHAR_SIZE 128  // must be power of 2

struct loaded_char {
    uint32_t codep;
    uint32_t texture;
    float xoff, yoff, xadvance;
};

struct font {
    stbtt_fontinfo info;
    buffer data;
    int loaded_char_count;
    loaded_char chars[MAX_LOADED_CHAR_COUNT];
};

font* load_font(const char* file_name) {
    buffer buff = read_entire_file_and_null_terminate(file_name);
    if (!buff.len) return NULL;

    unsigned char* fontdata = (unsigned char*)buff.ptr;
    stbtt_fontinfo info;
    int success = stbtt_InitFont(&info, fontdata, stbtt_GetFontOffsetForIndex(fontdata, 0));
    if (!success) {
        free(buff.ptr);
        return NULL;
    }

    font* f = (font*)malloc(sizeof(font));
    assert(f);

    f->info = info;
    f->data = buff;
    f->loaded_char_count = 0;
    return f;
}

void unload_font(font* f) {
    free(f->data.ptr);
    free(f);
}

FN_font_get_quad(font_get_quad) {
    loaded_char* chardata = NULL;
    for (int i = 0; i < f->loaded_char_count; ++i) {
        if (f->chars[i].codep == codep) {
            chardata = &f->chars[i];
        }
    }

    if (!chardata) {
        assert(f->loaded_char_count < MAX_LOADED_CHAR_COUNT);
        chardata = &f->chars[f->loaded_char_count++];

        float scale = stbtt_ScaleForPixelHeight(&f->info, LOAD_CHAR_SIZE);

        int advanceWidth, leftSideBearing;
        stbtt_GetCodepointHMetrics(&f->info, codep, &advanceWidth, &leftSideBearing);

        chardata->codep = codep;
        chardata->texture = 0;
        chardata->xoff = leftSideBearing * scale;
        chardata->xadvance = advanceWidth * scale;

        int ix0, iy0, ix1, iy1;
        stbtt_GetCodepointBitmapBox(&f->info, codep, scale, scale, &ix0, &iy0, &ix1, &iy1);
        if (ix1-ix0 > 0 && iy1-iy0 > 0) {
            unsigned char bitmap[LOAD_CHAR_SIZE*LOAD_CHAR_SIZE];
            stbtt_MakeCodepointBitmap(&f->info, bitmap, LOAD_CHAR_SIZE, LOAD_CHAR_SIZE, LOAD_CHAR_SIZE, scale, scale, codep);

            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, LOAD_CHAR_SIZE, LOAD_CHAR_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            chardata->texture = tex;
            chardata->yoff = iy0;
        }

        // printf("loaded char count: %d\n", font->loaded_char_count);
        // printf("x0: %d, y0: %d, x1: %d, y1: %d\n", ix0, iy0, ix1, iy1);
    }

    float scale = size / LOAD_CHAR_SIZE;
    font_quad result;
    result.texture = chardata->texture;
    result.p_min = scale * v2(chardata->xoff, chardata->yoff);
    result.p_max = result.p_min + v2(size);
    result.p_next = v2(chardata->xadvance * scale, 0);

    return result;
}

struct Arena {
    char* base;
    size_t cap;
    size_t used;
};

string copy_string(Arena* a, const char* s) {
    size_t len = strlen(s);
    size_t bytes_to_copy = len + 1;
    assert(a->used + bytes_to_copy <= a->cap);
    string result = {a->base + a->used, len};
    if (len) {
        memcpy(result.ptr, s, bytes_to_copy);
        a->used += bytes_to_copy;
    }
    return result;
}

bool string_eq(string s0, const char* s1) {
    size_t len = strlen(s1);
    return len == s0.len && memcmp(s0.ptr, s1, len) == 0;
}

string string_concat(Arena* a, const char* s0, const char* s1) {
    string result = {a->base + a->used, 0};
    result.len += copy_string(a, s0).len;
    --a->used; // drop the null terminator
    result.len += copy_string(a, s1).len;
    return result;
}

//
// Assets
//

struct asset_storage {
    int count;
    Asset assets[100];
    // storage for asset names
    Arena arena;
    char arena_storage[10240];
};
static asset_storage game_assets;

Asset* push_asset(Asset_Type type, const char* name) {
    assert((size_t)game_assets.count < ARRAY_LEN(game_assets.assets));
    Asset* result = &game_assets.assets[game_assets.count++];
    result->type = type;
    result->name = copy_string(&game_assets.arena, name);
    return result;
}

FN_get_asset(get_asset) {
    for (int i = 0; i < game_assets.count; ++i) {
        Asset* a = &game_assets.assets[i];
        if (string_eq(a->name, name)) {
            assert(a->type == type);
            return a;
        }
    }

#define ASSET_DIR "../assets/"
    char tmp_storage[1024];
    Arena tmp = {tmp_storage, sizeof(tmp_storage), 0};
    string file_path = string_concat(&tmp, ASSET_DIR, name);
    switch (type) {
        case Asset_Image: {
            image img = load_image(file_path.ptr);
            if (img.id) {
                Asset* a = push_asset(type, name);
                a->i = img;
                return a;
            }
        } break;

        case Asset_Font: {
            font* f = load_font(file_path.ptr);
            if (f) {
                Asset* a = push_asset(type, name);
                a->f = f;
                return a;
            }
         } break;

        default: assert(!"Unreachable");
    }
    return 0;
}

//
// Random
//

static unsigned char rand_buffer[1024*1024*4];
static size_t rand_idx = 0;

FN_get_random(get_random) {
    assert(num_bytes <= sizeof(rand_buffer));
    if (rand_idx + num_bytes > sizeof(rand_buffer)) rand_idx = 0;
    void* result = rand_buffer + rand_idx;
    rand_idx += num_bytes;
    return result;
}

//
//  Shader
//

#include "quad_shader.cpp"

//
// Reload
//

struct game_dll {
    HINSTANCE handle;
    FILETIME last_write_time;
};

#define DLL_NAME "dynamic.dll"

bool need_reloading(game_dll* dll) {
    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(DLL_NAME, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: failed to find dll\n");
        // assert(false);
        return false;
    }

    if (!dll->handle || CompareFileTime(&dll->last_write_time, &find_data.ftLastWriteTime) < 0) {
        Sleep(800); // gcc writes the dll twice, wait for it to finish writing
        dll->last_write_time = find_data.ftLastWriteTime;
        return true;
    }
    return false;
}

void reload_dll(game_dll* dll, game_data* game) {
#define TMP_FILE_NAME "dynamic.dll.tmp"
    HINSTANCE handle = LoadLibraryA(DLL_NAME);
    if (handle) {
        printf("tried to load\n");
        FreeLibrary(handle);

        game->draw_frame = 0;
        if (dll->handle) FreeLibrary(dll->handle);

        if(!CopyFileA(DLL_NAME, TMP_FILE_NAME, FALSE)) {
            fprintf(stderr, "Error: failed to copy file\n");
            assert(false);
        }

        HINSTANCE handle = LoadLibraryA(TMP_FILE_NAME);
        assert(handle);
        fn_on_load* on_load = (fn_on_load*)(void*)GetProcAddress(handle, "on_load");
        assert(on_load);
        on_load(game);

        dll->handle = handle;
    }
}

uint64_t query_performance_frequency() {
    LARGE_INTEGER frequency;
    assert(QueryPerformanceFrequency(&frequency));
    return frequency.QuadPart;
}
uint64_t query_performance_counter() {
    LARGE_INTEGER ticks;
    assert(QueryPerformanceCounter(&ticks));
    return ticks.QuadPart;
}

int find_last_index(const char* s, char c) {
    size_t len = strlen(s);
    for (const char* p = s + len - 1; p != s; --p) {
        if (*p == c) return p - s;
    }
    return -1;
}

static game_data game;

int main(int argc, char* argv[]) {
    char* prog_path = argv[0];
    int idx = find_last_index(prog_path, '\\');
    if (idx < 0) idx = find_last_index(prog_path, '/');
    if (idx >= 0) {
        char* dir_path = (prog_path[idx] = 0, prog_path);
        printf("Changing directory to: %s\n", dir_path);
        assert(SetCurrentDirectoryA(dir_path));
    }

    Window* win = create_window("Quantum Game");
    gl_swap_interval(1); // enable vsync

    assert(init_quad_program());

    // initialize random numbers
    NTSTATUS nt_status = BCryptGenRandom(0, rand_buffer, sizeof(rand_buffer), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    assert(NT_SUCCESS(nt_status));

    // storage for asset names
    game_assets.arena.base = game_assets.arena_storage;
    game_assets.arena.cap = sizeof(game_assets.arena_storage);

    game.get_random = get_random;
    game.get_asset = get_asset;
    game.font_get_quad = font_get_quad;
    game.push_quad = push_quad;

    uint64_t perf_freq = query_performance_frequency();
    uint64_t start_time = query_performance_counter();

    float t0 = 0;
    game_dll dll = {};
    game_inputs inputs = {};
    for (bool running = true; running;) {
        bool request_reload = false;
        for (Event event; window_check_event(win, &event);) {
            if (window_should_close(win) || (event.type == Event_key_down && event.key == VK_ESCAPE))
                running = false;
            if (event.type == Event_key_down && event.key == 'R')
                request_reload = true;
            if (event.type == Event_mouse_button_down && event.button == Button_left)
                inputs.mouse_down = true;
            if (event.type == Event_mouse_button_up && event.button == Button_left)
                inputs.mouse_down = false;
        }
        inputs.mouse_x = win->mouse_x;
        inputs.mouse_y = win->mouse_y;

        if (request_reload || need_reloading(&dll)) reload_dll(&dll, &game);

        float t = (float)(query_performance_counter() - start_time) / perf_freq;
        float dt = t - t0;

        if (game.draw_frame) {
            glViewport(0, 0, win->width, win->height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            game.draw_frame(dt, win->width, win->height, inputs);

            flush_quads(win->width, win->height);
            window_swap_buffers(win);
        }

        t0 = t;
    }

    destroy_window(win);

    return 0;
}
