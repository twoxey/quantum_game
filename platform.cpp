#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "utils.h"

#include "GL/gl.h"
#include "GL/glext.h"

#include "stdio.h"

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

#define GL_PROCS \
XXX(PFNGLCREATESHADERPROC,              glCreateShader) \
XXX(PFNGLSHADERSOURCEPROC,              glShaderSource) \
XXX(PFNGLCOMPILESHADERPROC,             glCompileShader) \
XXX(PFNGLGETSHADERIVPROC,               glGetShaderiv) \
XXX(PFNGLGETSHADERINFOLOGPROC,          glGetShaderInfoLog) \
XXX(PFNGLDELETESHADERPROC,              glDeleteShader) \
XXX(PFNGLCREATEPROGRAMPROC,             glCreateProgram) \
XXX(PFNGLATTACHSHADERPROC,              glAttachShader) \
XXX(PFNGLLINKPROGRAMPROC,               glLinkProgram) \
XXX(PFNGLGETPROGRAMIVPROC,              glGetProgramiv) \
XXX(PFNGLGETPROGRAMINFOLOGPROC,         glGetProgramInfoLog) \
XXX(PFNGLDELETEPROGRAMPROC,             glDeleteProgram) \
XXX(PFNGLUSEPROGRAMPROC,                glUseProgram) \
XXX(PFNGLGETUNIFORMLOCATIONPROC,        glGetUniformLocation) \
XXX(PFNGLUNIFORM1IPROC,                 glUniform1i) \
XXX(PFNGLUNIFORM2FPROC,                 glUniform2f) \
XXX(PFNGLUNIFORM4FPROC,                 glUniform4f) \
XXX(PFNGLUNIFORMMATRIX2FVPROC,          glUniformMatrix2fv) \
XXX(PFNGLUNIFORMMATRIX3FVPROC,          glUniformMatrix3fv) \
XXX(PFNGLACTIVETEXTUREPROC,             glActiveTexture) \
XXX(PFNGLGENBUFFERSPROC,                glGenBuffers) \
XXX(PFNGLBINDBUFFERPROC,                glBindBuffer) \
XXX(PFNGLBUFFERDATAPROC,                glBufferData) \
XXX(PFNGLGETATTRIBLOCATIONPROC,         glGetAttribLocation) \
XXX(PFNGLENABLEVERTEXATTRIBARRAYPROC,   glEnableVertexAttribArray) \
XXX(PFNGLVERTEXATTRIBPOINTERPROC,       glVertexAttribPointer) \
XXX(PFNGLVERTEXATTRIBIPOINTERPROC,      glVertexAttribIPointer) \
XXX(PFNGLVERTEXATTRIBDIVISORPROC,       glVertexAttribDivisor) \
XXX(PFNGLDRAWARRAYSINSTANCEDPROC,       glDrawArraysInstanced) \
// GL_PROCS

#define XXX(type, name) type name = NULL;
GL_PROCS
#undef XXX

bool load_gl_procs(void* GetProcAddress_proc(const char*)) {
#define XXX(type, name) \
if (name = (type)GetProcAddress_proc(# name), !name) { \
    fprintf(stderr, "Failed to load GL proc: %s\n", # name); \
    return false; \
}
GL_PROCS
#undef XXX
    return true;
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
FN_load_font(load_font) {
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

void* alloc_size(arena* a, size_t size) {
    assert(a->used + size <= a->capacity);
    void* result = a->base + a->used;
    a->used += size;
    return result;
}

string copy_string(arena* a, const char* s) {
    const char* p = s;
    while (*p) ++p;
    size_t l = p - s;
    return {(char*)memcpy(alloc_size(a, l), s, l), l};
}
