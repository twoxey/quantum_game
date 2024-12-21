
typedef struct {
    char* ptr;
    size_t len;
} buffer;

typedef buffer string;

typedef union {
    float f[3];
    struct { float x, y, z; };
    struct { float r, g, b; };
} vec3;

typedef union {
    float f[4];
    struct { float x, y, z, w; };
    struct { float r, g, b, a; };
    vec3 xyz;
    vec3 rgb;
} vec4;

typedef union {
    float f[9];
    struct {
        float _11, _12, _13;
        float _21, _22, _23;
        float _31, _32, _33;
    };
    struct {
        vec3 r1;
        vec3 r2;
        vec3 r3;
    };
} mat3;

typedef vec4 color;
#define COLOR(r, g, b, a) ((color){{r, g, b, a}})

typedef struct renderer renderer;
renderer* init_renderer();
void set_viewport(renderer* re, int win_w, int win_h);

mat3 set_transform(renderer* re, mat3 transform);
mat3 reset_transform(renderer* re);

mat3 rotate(renderer* re, float angle);
mat3 scale(renderer* re, float x, float y);
mat3 translate(renderer* re, float x, float y);

void draw_rect(renderer* re, float x, float y, float w, float h, color c);
void draw_circle(renderer* re, float x, float y, float r, color c);
void draw_ellipse(renderer* re, float x, float y, float w, float h, color c);

float vec3_dot(vec3 a, vec3 b);
mat3 mat3_transpose(mat3 m);

vec3 mat3xvec3(mat3 m, vec3 v);
mat3 mat3xmat3(mat3 a, mat3 b);

mat3 identity3();
mat3 rotate_2d(float angle);
mat3 scale_2d(float x, float y);
mat3 translate_2d(float x, float y);




//
// impl
//

#include "windows.h"
#include "stdlib.h"

buffer read_entire_file_and_null_terminate(const char* file_path) {
    buffer result = {0};

    HANDLE file = CreateFileA(file_path, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return result;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size)) goto defer;

    char* mem = malloc(file_size.QuadPart + 1);
    if (!mem) goto defer;

    DWORD bytes_read;
    if (ReadFile(file, mem, file_size.QuadPart, &bytes_read, 0) && bytes_read == file_size.QuadPart) {
        result.len = bytes_read;
        result.ptr = mem;
        result.ptr[result.len] = 0;
    } else free(mem);

defer:
    CloseHandle(file);
    return result;
}


//
// Math
//

float vec3_dot(vec3 a, vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

vec3 mat3xvec3(mat3 m, vec3 v) {
    vec3 result = {
        .x = vec3_dot(m.r1, v),
        .y = vec3_dot(m.r2, v),
        .z = vec3_dot(m.r3, v),
    };
    return result;
}

mat3 mat3_transpose(mat3 m) {
    mat3 result = {{
        m._11, m._21, m._31,
        m._12, m._22, m._32,
        m._13, m._23, m._33,
    }};
    return result;
}

mat3 mat3xmat3(mat3 a, mat3 b) {
    b = mat3_transpose(b);
    mat3 result = {
        .r1 = mat3xvec3(a, b.r1),
        .r2 = mat3xvec3(a, b.r2),
        .r3 = mat3xvec3(a, b.r3),
    };
    return mat3_transpose(result);
}

mat3 identity3() {
    mat3 result = {{
        1, 0, 0,
        0, 1, 0,
        0, 0, 1,
    }};
    return result; 
}

mat3 rotate_2d(float angle) {
    float cos_r = cos(angle);
    float sin_r = sin(angle);
    mat3 result = {{
        cos_r, -sin_r,  0,
        sin_r,  cos_r,  0,
            0,      0,  1,
    }};
    return result;
}

mat3 scale_2d(float x, float y) {
    mat3 result = {{
        x, 0, 0,
        0, y, 0,
        0, 0, 1,
    }};
    return result;
}

mat3 translate_2d(float x, float y) {
    mat3 result = {{
        1, 0, x,
        0, 1, y,
        0, 0, 1,
    }};
    return result;
}


//
// OpenGL
//

// #define GL_GLEXT_PROTOTYPES
#include "GL/glext.h"

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
// GL_PROCS

#define XXX(type, name) type name = NULL;
GL_PROCS
#undef XXX

bool load_gl_procs(void* GetProcAddress_proc(const char*)) {
#define XXX(type, name) \
if (name = GetProcAddress_proc(# name), !name) { \
    fprintf(stderr, "Failed to load GL proc: %s\n", # name); \
    return false; \
}
GL_PROCS
#undef XXX
    return true;
}

GLuint compile_shader(GLenum type, const char* shader_src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shader_src, 0);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char buff[512];
        glGetShaderInfoLog(shader, sizeof(buff), NULL, buff);
        fprintf(stderr, "Shader compile status: %s\n", buff);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint create_shader_program(const char* vert_src, const char* frag_src) {
    GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vert_shader || !frag_shader) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
    glLinkProgram(program);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char buff[512];
        glGetProgramInfoLog(program, sizeof(buff), NULL, buff);
        fprintf(stderr, "Program link status: %s\n", buff);

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

GLuint load_shader_from_file(const char* vert_file_path, const char* frag_file_path) {
    string vert_src = read_entire_file_and_null_terminate(vert_file_path);
    string frag_src = read_entire_file_and_null_terminate(frag_file_path);
    if (vert_src.len == 0 || frag_src.len == 0) {
        fprintf(stderr, "Failed to load shader files\n");
        return false;
    }

    GLuint program = create_shader_program(vert_src.ptr, frag_src.ptr);

    free(vert_src.ptr);
    free(frag_src.ptr);

    return program;
}

#include "shader/quad_shader.h"

struct renderer {
    mat3 transform;
    int win_w, win_h;
    quad_program qp;
};

renderer* init_renderer() {
    if (!load_gl_procs(RGFW_getProcAddress)) return NULL;

    static renderer re;
    if (!init_quad_program(&re.qp)) return NULL;

    reset_transform(&re);

    return &re;
}

void set_viewport(renderer* re, int win_w, int win_h) {
    re->win_w = win_w;
    re->win_h = win_h;
    glViewport(0, 0, win_w, win_h);
}

mat3 set_transform(renderer* re, mat3 transform) {
    return re->transform = transform;
}

mat3 reset_transform(renderer* re) {
    return re->transform = identity3();
}

mat3 rotate(renderer* re, float angle) {
    // we will flip y coordinate in when doing screen projection,
    // by doing so we changed from right-handed coordinate system to a
    // left-handed one, so we have to flip the rotation when rotating
    // along the view plane
    return re->transform = mat3xmat3(rotate_2d(-angle), re->transform);
}
mat3 scale(renderer* re, float x, float y) {
    return re->transform = mat3xmat3(scale_2d(x, y), re->transform);
}
mat3 translate(renderer* re, float x, float y) {
    return re->transform = mat3xmat3(translate_2d(x, y), re->transform);
}

void draw_rect(renderer* re, float x, float y, float w, float h, color c) {
    render_quad(&re->qp, re->transform, re->win_w, re->win_h, QT_rect, x+w/2, y+h/2, w, h, c);
}

void draw_circle(renderer* re, float x, float y, float r, color c) {
    render_quad(&re->qp, re->transform, re->win_w, re->win_h, QT_ellipse, x, y, r*2, r*2, c);
}

void draw_ellipse(renderer* re, float x, float y, float w, float h, color c) {
    render_quad(&re->qp, re->transform, re->win_w, re->win_h, QT_ellipse, x, y, w, h, c);
}
