#include "math.h"

//
// https://bottosson.github.io/posts/oklab/
//

struct Lab {float L; float a; float b;};
struct RGB {float r; float g; float b;};

Lab linear_srgb_to_oklab(RGB c)
{
    float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
    float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
    float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

    float l_ = cbrtf(l);
    float m_ = cbrtf(m);
    float s_ = cbrtf(s);

    return {
        0.2104542553f*l_ + 0.7936177850f*m_ - 0.0040720468f*s_,
        1.9779984951f*l_ - 2.4285922050f*m_ + 0.4505937099f*s_,
        0.0259040371f*l_ + 0.7827717662f*m_ - 0.8086757660f*s_,
    };
}

RGB oklab_to_linear_srgb(Lab c)
{
    float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
    float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
    float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;

    float l = l_*l_*l_;
    float m = m_*m_*m_;
    float s = s_*s_*s_;

    return {
        +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
        -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
        -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
    };
}

//
// https://en.wikipedia.org/wiki/SRGB
// GAMMA = 2.4f, A = 12.92f, C = 0.055f, U = 0.04045f, V = 0.0031308f
//
// Encode liner brightness value to sRGB
float gamma_E(float v) {
    return v <= 0.0031308f ? 12.92f * v : (1.0f + 0.055f) * powf(v, 1.0f / 2.4f) - 0.055f;
}
// Decode sRGB to linear value
float gamme_D(float u) {
    return u <= 0.04045f ? u / 12.92f : powf((u + 0.055f) / (1.0f + 0.055f), 2.4f);
}

float clamp(float f, float min, float max) {
    if (f < min) f = min;
    if (f > max) f = max;
    return f;
}

RGB oklab_to_srgb(Lab c) {
    RGB rgb = oklab_to_linear_srgb(c);
    return {
        clamp(gamma_E(rgb.r), 0, 1),
        clamp(gamma_E(rgb.g), 0, 1),
        clamp(gamma_E(rgb.b), 0, 1),
    };
}

Lab oklch_to_oklab(float L, float c, float h) {
    return {L, c*cos(h), c*sin(h)};
}

RGB oklch_to_srgb(float L, float c, float h) {
    return oklab_to_srgb(oklch_to_oklab(L, c, h));
}

#include "thirdparty/create_opengl_window.h"
#include "stdio.h"

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

GLuint gen_texture() {
    GLuint tex_id;
    glGenTextures(1, &tex_id);
    assert(tex_id);

    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return tex_id;
}

void load_texture_data(GLuint tex_id, int w, int h, const unsigned char* pixels) {
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

GLuint init_shader() {
    GLuint prog = create_shader_program(
        "#version 330\n",
        R"(
        out vec2 frag_uv;
        void main() {
            vec2 uv = vec2(gl_VertexID & 1, gl_VertexID >> 1 & 1);
            gl_Position = vec4(uv * 2 - 1, 0, 1);
            frag_uv = uv;
        }
        )",
        R"(
        uniform sampler2D tex;
        in vec2 frag_uv;
        out vec4 out_color;
        void main() {
            //out_color = vec4(1, 0, 0, 1);
            out_color = texture(tex, frag_uv);
        }
        )"
    );
    assert(prog);

    return prog;
}

int main() {
    Window* win = create_window();
    gl_swap_interval(1);

#define W 100
#define H 100
#define NUM_COMPONENTS 3
    unsigned char pixels[W*H*NUM_COMPONENTS];

    GLuint tex = gen_texture();
    GLuint prog = init_shader();

    float L = 0;
    float h = 0;
    float dL = 0.01;

    while (!window_should_close(win)) {
        for (Event ev; window_check_event(win, &ev);) {
            if (ev.type == Event_mouse_button_down && ev.button == Button_left) {
                printf("%f, %f\n", (float)win->mouse_x / win->width, (float)win->mouse_y / win->height);
            }
        }
        if (L < 0) {
            L = 0;
            dL = 0.01;
        }
        if (L > 1) {
            L = 1;
            dL = -0.01;
        }
        L += dL;
        h += 0.1;

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                RGB rgb = oklch_to_srgb(.9, 1 ? .15 : (float)y/(H-1) * 0.4f, h+((float)x/(W-1)-0.5f)*2*M_PI);
                unsigned char* p = pixels + (y * W + x) * NUM_COMPONENTS;
                *p++ = rgb.r * 255;
                *p++ = rgb.g * 255;
                *p++ = rgb.b * 255;
            }
        }
        load_texture_data(tex, W, H, pixels);

        glViewport(0, 0, win->width, win->height);

        glUseProgram(prog);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        window_swap_buffers(win);
    }

    destroy_window(win);

    return 0;
}
