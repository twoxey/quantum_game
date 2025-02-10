#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "stdint.h"
#include "stdio.h"
#include "assert.h"

#define FN_on_load(fn_name) void fn_name(char game_memory[])
typedef FN_on_load(fn_on_load);

#define FN_draw_frame(fn_name) bool fn_name(float dt)
typedef FN_draw_frame(fn_draw_frame);

fn_draw_frame* draw_frame;

static char game_memory[40960000];

struct game_dll {
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
        // assert(false);
        return;
    }

    if (CompareFileTime(&dll->last_write_time, &find_data.ftLastWriteTime) != 0) {
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
            on_load(game_memory);

            FreeLibrary(dll->handle);

            dll->last_write_time = find_data.ftLastWriteTime;
            dll->handle = handle;
        }
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

int main(int argc, char* argv[]) {
    int idx = find_last_index(argv[0], '\\');
    if (idx >= 0) {
        argv[0][idx] = 0;
        printf("Changing directory to: %s\n", argv[0]);
        assert(SetCurrentDirectoryA(argv[0]));
    }

    uint64_t perf_freq = query_performance_frequency();
    uint64_t start_time = query_performance_counter();

    float t0 = 0;
    game_dll dll = {};
    for (;;) {
        check_and_reload_dll(&dll);

        float t = (float)(query_performance_counter() - start_time) / perf_freq;
        float dt = t - t0;

        if (!draw_frame(dt)) break;

        t0 = t;
    }

    return 0;
}
