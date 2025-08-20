#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "assert.h"
#include "stdbool.h"
#include "stdio.h"

DWORD run_command(char* command_line) {
    fprintf(stderr, "[INFO] Running command: %s\n", command_line);

    STARTUPINFOA startup_info = {};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info;
    if (!CreateProcessA(0, command_line, 0, 0, TRUE, 0, 0, 0, &startup_info, &process_info))
        return GetLastError();

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code;
    assert(GetExitCodeProcess(process_info.hProcess, &exit_code));
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    if (exit_code != 0) fprintf(stderr, "[ERROR] Command '%s' failed with exit code: %lu\n", command_line, exit_code);
    return exit_code;
}

DWORD run_command_redirect_stdout(char* command_line, const char* out_file_name) {
    fprintf(stderr, "[INFO] Running command: '%s', stdout redirected to: %s\n", command_line, out_file_name);

    SECURITY_ATTRIBUTES security_attributes = {};
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.bInheritHandle = TRUE;
    HANDLE out_file_handle = CreateFileA(out_file_name, GENERIC_WRITE, 0, &security_attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (out_file_handle == INVALID_HANDLE_VALUE) return GetLastError();

    STARTUPINFOA startup_info = {};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = out_file_handle;
    startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process_info;
    BOOL success = CreateProcessA(0, command_line, 0, 0, TRUE, 0, 0, 0, &startup_info, &process_info);
    if (!success) return GetLastError();

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code;
    assert(GetExitCodeProcess(process_info.hProcess, &exit_code));
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    CloseHandle(out_file_handle);
    if (exit_code != 0) fprintf(stderr, "[ERROR] Command failed with exit code: %lu\n", exit_code);
    return exit_code;
}

DWORD rename_file(const char* existing_file_name, const char* new_file_name) {
    DWORD error = 0;
    if (MoveFileExA(existing_file_name, new_file_name, MOVEFILE_REPLACE_EXISTING)) {
        fprintf(stderr, "[INFO] Renamed %s -> %s\n", existing_file_name, new_file_name);
    } else {
        fprintf(stderr, "[ERROR] Failed to rename %s -> %s\n", existing_file_name, new_file_name);
        error = GetLastError();
    }
    return error;
}

bool create_directory(const char* path_name) {
    BOOL success = CreateDirectoryA(path_name, 0);
    if (success) {
        fprintf(stderr, "[INFO] Created directory: %s\n", path_name);
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            fprintf(stderr, "[ERROR] Failed to created directory, error: %lu\n", error);
            return false;
        }
    }
    return true;
}

FILETIME* get_file_last_write_time(const char* file_path);
#define get_file_last_write_time(file_path) get_file_last_write_time_(file_path, &(FILETIME){})
FILETIME* get_file_last_write_time_(const char* file_path, FILETIME* last_write_time) {
    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(file_path, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) return 0;
        assert(!"Unexpected error");
    }
    *last_write_time = find_data.ftLastWriteTime;
    return last_write_time;
}

bool str_starts_with(const char* s, const char* prefix) {
    const char *p0 = s, *p1 = prefix;
    for (;;) {
        if (!*p0 || !*p1) return !*p1;
        if (*p0 != *p1) return false;
        ++p0, ++p1;
    }
}

bool str_ends_with(const char* s, const char* suffix) {
    const char *p0 = s, *p1 = suffix;
    while (*p0) ++p0;
    while (*p1) ++p1;
    for (;;) {
        if (p0 == s || p1 == suffix) return p1 == suffix;
        if (*p0 != *p1) return false;
        --p0, --p1;
    }
}

char* chop_end(char* s, size_t count) {
    size_t len = strlen(s);
    assert(count <= len);
    s[len - count] = 0;
    return s;
}

typedef struct {
    char* base;
    size_t cap;
    size_t used;
} Arena;

#define arena_on_stack(size) ((Arena){(char[size]){}, size, 0})

char* copy_string(Arena* a, const char* s) {
    size_t len = strlen(s);
    assert(a->used + len < a->cap);
    char* res = memcpy(a->base + a->used, s, len + 1);
    a->used += len + 1;
    return res;
}

char* format_string(Arena* a, const char* fmt, ...) {
    char* s = a->base + a->used;
    va_list args;
    va_start(args, fmt);
    a->used += vsnprintf(a->base + a->used, a->cap - a->used, fmt, args) + 1;
    va_end(args);
    assert(a->used < a->cap); // this may overflow
    return s;
}

char* join_args(Arena* a, int argc, char* const argv[]) {
    char* res = a->base + a->used;
    for (int i = 0; i < argc; ++i) {
        copy_string(a, argv[i]);
        a->base[a->used - 1] = ' '; // overwrite the null terminator
    }
    a->base[a->used - 1] = 0;
    return res;
}

int va_to_array_of_strings(Arena* a, va_list va, char*** array) {
    int count = 0;
    char** ptr = (char**)(a->base + a->used);
    for (char* arg; arg = va_arg(va, char*);) {
        ptr[count++] = arg;
        a->used += sizeof(arg);
    }
    *array = ptr;
    return count;
}

char* format_build_command(Arena* a, const char* cmd, const char* target, int num_deps, char* const deps[]) {
    char* res = a->base + a->used;
    for (const char* p = cmd; *p;) {
        if (*p == '%') {
            const char fmt_target[] = "%target";
            const char fmt_deps[] = "%deps";
            const char fmt_first_dep[] = "%first_dep";
            if (str_starts_with(p, fmt_target)) {
                copy_string(a, target);
                --a->used; // remove the null terminator
                p += sizeof(fmt_target) - 1;
            } else if (str_starts_with(p, fmt_deps)) {
                join_args(a, num_deps, deps);
                --a->used; // remove the null terminator
                p += sizeof(fmt_deps) - 1;
            } else if (str_starts_with(p, fmt_first_dep)) {
                assert(num_deps >= 1);
                copy_string(a, deps[0]);
                --a->used; // remove the null terminator
                p += sizeof(fmt_first_dep) - 1;
            } else {
                fprintf(stderr, "[ERROR]: Invalid format argument in command: '%s'\n", cmd);
                assert(!"Invalid format argument");
            }
        } else {
            assert(a->used < a->cap);
            a->base[a->used++] = *p++;
        }
    }
    a->base[a->used] = 0;
    return res;
}

bool target_needs_rebuild_(const char* target_file_path, int deps_count, char* const deps[]) {
    bool need_rebuild = false;

    FILETIME* target_write_time = get_file_last_write_time(target_file_path);
    if(!target_write_time) need_rebuild = true;

    for (int i = 0; i < deps_count; ++i) {
        FILETIME* src_write_time = get_file_last_write_time(deps[i]);
        assert(src_write_time && "Source file should exist");
        if (!need_rebuild && CompareFileTime(src_write_time, target_write_time) > 0) {
            need_rebuild = true;
        }
    }

    return need_rebuild;
}

#define target_needs_rebuild(target, ...) target_needs_rebuild_v_(target, __VA_ARGS__, NULL)
bool target_needs_rebuild_v_(const char* target_file_path, ...) {
    va_list args;
    va_start(args, target_file_path);

    Arena a = arena_on_stack(4096);
    char** deps = (char**)(a.base + a.used);
    int deps_count = va_to_array_of_strings(&a, args, &deps);
    bool result = target_needs_rebuild_(target_file_path, deps_count, deps);

    va_end(args);

    return result;
}

#define run_build_command(cmd, target, ...) run_build_command_(cmd, target, ##__VA_ARGS__, NULL)
DWORD run_build_command_(const char* cmd, const char* target, ...) {
    DWORD result = 0;

    va_list args;
    va_start(args, target);

    Arena a = arena_on_stack(4096);
    char** deps;
    int deps_count = va_to_array_of_strings(&a, args, &deps);
    char* build_command = format_build_command(&a, cmd, target, deps_count, deps);

    if (!target || target_needs_rebuild_(target, deps_count, deps)) {
        result = run_command(build_command);
    }

    va_end(args);

    return result;
}

void check_and_rebuild(int argc, char* argv[]) {
    assert(argv && argc >= 1);
    Arena a = arena_on_stack(4096);

    char* arg0 = argv[0];
    char *base_name, *prog_name;
#define extension ".exe"
    if (str_ends_with(arg0, extension)) {
        prog_name = arg0;
        base_name = chop_end(copy_string(&a, prog_name), sizeof(extension)-1);
    } else {
        base_name = arg0;
        prog_name = format_string(&a, "%s" extension, base_name);
    }

    // This prevents running the program recursivly
    if (!target_needs_rebuild(prog_name, __FILE__)) return;

    char* temp_name = format_string(&a, "%s-old.exe", base_name);
    DWORD err = 0;
    err = rename_file(prog_name, temp_name);
    if (err) goto exit_process;

    err = run_command(format_string(&a, "gcc -o %s %s", base_name, __FILE__));
    if (err) {
        rename_file(temp_name, prog_name);
        goto exit_process;
    }
    err = run_command(join_args(&a, argc, argv));

exit_process:
    if (err) fprintf(stderr, "[ERROR] Build failed with error code: %lu\n", err);
    ExitProcess(err);
}

int main(int argc, char* argv[]) {
    check_and_rebuild(argc, argv);
    DWORD err = 0;

    // oklab
    // err = run_build_command("g++ -o %target %deps -lgdi32 -lopengl32", "oklab.exe", "oklab.cpp");
    // if (err) return 1;

    // get dependency
#define dep_path "thirdparty"
    assert(create_directory(dep_path));

    // the command will download the dependent header if it is not exist
    err = run_build_command("curl -o %target https://raw.githubusercontent.com/twoxey/create_opengl_window/refs/heads/master/create_opengl_window.h",
                                dep_path"/create_opengl_window.h");
    // err = run_build_command("cp -v %first_dep %target", dep_path"/create_opengl_window.h", "D:/code/opengl-windows/create_opengl_window.h");
    if (err) return 1;

#define out_path "out"
    assert(create_directory(out_path));

    err = run_build_command("g++ -I "dep_path" -Wall -Wextra -o %target main.cpp -lgdi32 -lopengl32 -lbcrypt", out_path"/main.exe",
                                "main.cpp", "quad_shader.cpp", "utils.h", "math_helper.h", dep_path"/create_opengl_window.h");
    if (err) return 1;

    err = run_build_command("g++ -Wall -Wextra -shared -o %target dynamic.cpp", out_path"/dynamic.dll",
                                "dynamic.cpp", "draw.cpp", "utils.h", "math_helper.h", "utf8.h");
    if (err) return 1;

    return 0;
}
