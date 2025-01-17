#include "utils.h"

#ifndef GL_PROCS
#define GL_GLEXT_PROTOTYPES
#endif
#include "GL/glext.h"

struct quad_shader_program {
    GLuint program;
    GLuint u_screen_size;
    GLuint u_transform;
    GLuint u_type;
    GLuint vertex_buffer;
};

bool init_quad_program(quad_shader_program* p) {
    GLuint program = create_shader_program(
        "#version 330\n",

        R"vertex_shader(

        uniform vec2 u_screen_size;
        uniform mat3 u_transform;

        in vec2 a_uv;
        in vec4 a_color;
        out vec4 frag_color;
        out vec2 frag_uv;

        void main() {
            vec3 pos = vec3(gl_VertexID & 1, (gl_VertexID >> 1) & 1, 1);
            pos.xy -= 0.5;
            pos = u_transform * pos;
            vec2 scale = 2 / u_screen_size;
            pos.xy *= scale;
            pos.y = -pos.y;
            pos.xy += vec2(-1, 1);
            gl_Position = vec4(pos.xy, 0, 1);
            frag_color = a_color;
            frag_uv = a_uv;
        }

        )vertex_shader",

        R"fragment_shader(

        uniform sampler2D tex;   // always be texture unit 0 for now
        uniform int u_type;

        in vec4 frag_color;
        in vec2 frag_uv;

        out vec4 out_color;
        void main() {
            out_color = frag_color;
            switch (u_type) {
            case 1: // QUAD_char
                out_color.a = texture(tex, frag_uv).r;
                break;
            case 2: // QUAD_image
                out_color = texture(tex, frag_uv);
                break;
            case 3: // QUAD_ellipse
                if (length(frag_uv - vec2(0.5)) > 0.5)
                    out_color.a = 0;
                break;
            }
        }

        )fragment_shader"
    );

    if (!program) return false;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint u_screen_size = glGetUniformLocation(program, "u_screen_size");
    GLuint u_transform = glGetUniformLocation(program, "u_transform");
    GLuint u_type = glGetUniformLocation(program, "u_type");

    GLuint buffers[1];
    glGenBuffers(ARRAY_LEN(buffers), buffers);
    GLuint vertex_buffer = buffers[0];
    // GLuint instance_buffer = buffers[1];

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    GLint a_uv = glGetAttribLocation(program, "a_uv");
    glEnableVertexAttribArray(a_uv);
    glVertexAttribPointer(a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(quad_vertex_data), &((quad_vertex_data*)0)->uv);

    GLint a_color = glGetAttribLocation(program, "a_color");
    glEnableVertexAttribArray(a_color);
    glVertexAttribPointer(a_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(quad_vertex_data), &((quad_vertex_data*)0)->color);

    p->program = program;
    p->vertex_buffer = vertex_buffer;
    p->u_screen_size = u_screen_size;
    p->u_transform = u_transform;
    p->u_type = u_type;

    return true;
}

void flush_quads(quad_shader_program* p, quad_data_buffer* d, int win_w, int win_h) {
    glViewport(0, 0, win_w, win_h);

    glUseProgram(p->program);
    glUniform2f(p->u_screen_size, win_w, win_h);

    // glBindBuffer(GL_ARRAY_BUFFER, instance_buff);
    // glBufferData(GL_ARRAY_BUFFER, quad_count*sizeof(quad_instances[0]), quad_instances, GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, p->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, d->quad_count*4*sizeof(d->vertices[0]), d->vertices, GL_STREAM_DRAW);
    for (int i = 0; i < d->quad_count; ++i) {
        quad_instance_data* inst = d->instances + i;
        glUniformMatrix3fv(p->u_transform, 1, GL_TRUE, inst->transform.f);
        glUniform1i(p->u_type, inst->type);
        glBindTexture(GL_TEXTURE_2D, inst->texture_id);
        glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4);
    }
    // glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, quad_count);

    d->quad_count = 0;
}
