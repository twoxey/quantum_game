#define STB_TRUETYPE_IMPLEMENTATION

#define RGFW_IMPLEMENTATION
#include "RGFW.h"

#include "utils.h"

#include "platform.cpp"
#include "quad_shader.cpp"

// #include "dynamic.cpp"

fn_draw_frame* draw_frame;

static game_data game_memory;

struct game_dll {
    bool changed;
    HINSTANCE handle;
    FILETIME last_write_time;
};
void check_and_reload_dll(game_dll* dll) {
    const char* file_name = "dynamic.dll";
    const char* tmp_file_name = "dynamic.dll.tmp";

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(file_name, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: failed to find dll\n");
        assert(false);
    }

    bool changed = CompareFileTime(&dll->last_write_time, &find_data.ftLastWriteTime) != 0;
    if (changed) {
        HINSTANCE handle = LoadLibraryA(file_name);
        if (handle) {
            FreeLibrary(handle);
            if (dll->handle) FreeLibrary(dll->handle);

            if(!CopyFileA(file_name, tmp_file_name, FALSE)) {
                fprintf(stderr, "Error: failed to copy file\n");
                assert(false);
            }

            HINSTANCE handle = LoadLibraryA(tmp_file_name);
            assert(handle);
            fn_on_load* on_load = (fn_on_load*)(void*)GetProcAddress(handle, "on_load");
            assert(on_load);
            draw_frame = (fn_draw_frame*)(void*)GetProcAddress(handle, "draw_frame");
            assert(draw_frame);
            on_load(&game_memory);

            dll->last_write_time = find_data.ftLastWriteTime;
            dll->changed = false;
            dll->handle = handle;
        }
    }
}

int main() {
    RGFW_window* win = RGFW_createWindow("my window", RGFW_RECT(0, 0, 800, 600), RGFW_CENTER);

    assert(load_gl_procs(RGFW_getProcAddress));

    quad_shader_program quad_program;
    assert(init_quad_program(&quad_program));

    game_memory.static_arena.capacity = 40960;
    game_memory.static_arena.base = (char*)malloc(game_memory.static_arena.capacity);
    assert(game_memory.static_arena.base);
    game_memory.load_image = load_image;
    game_memory.load_font = load_font;

    u64 start_time = RGFW_getTimeNS();
    float t0 = 0;

    game_dll dll = {};
    game_inputs inputs = {};

    while(!RGFW_window_shouldClose(win)) {
        while(RGFW_window_checkEvent(win)) {
            switch (win->event.type) {
                case RGFW_mousePosChanged:
                    inputs.mouse_x = win->event.point.x;
                    inputs.mouse_y = win->event.point.y;
                    break;
                case RGFW_mouseButtonPressed:
                    inputs.mouse_down = true;
                    break;
                case RGFW_mouseButtonReleased:
                    inputs.mouse_down = false;
                    break;
            }
        }

        check_and_reload_dll(&dll);

        float t = (RGFW_getTimeNS() - start_time) * 1e-9;
        float dt = t - t0;

        glViewport(0, 0, win->r.w, win->r.h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        draw_frame(dt, win->r.w, win->r.h, inputs);

        flush_quads(&quad_program, &game_memory.quad_data, win->r.w, win->r.h);
        RGFW_window_swapBuffers(win);

        RGFW_window_checkFPS(win, 120);
        t0 = t;
    }

    RGFW_window_close(win);

    return 0;
}