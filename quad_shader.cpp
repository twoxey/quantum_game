struct quad_shader_program {
    GLuint program;
    GLuint u_screen_size;
    GLuint u_transform;
    GLuint u_type;
    GLuint u_params;
    GLuint vertex_buffer;
};

struct quad_vertex_data {
    vec2 uv;
    rgba32 color;
};
struct quad_instance_data {
    mat3 transform;
    uint32_t type;
    uint32_t texture_id;
    vec4 params;
};
struct quad_data_buffer {
    int quad_count;
    quad_instance_data instances[1024];
    quad_vertex_data vertices[ARRAY_LEN(((quad_data_buffer*)0)->instances)];
};

static quad_data_buffer quad_data;
static quad_shader_program quad_program;

bool init_quad_program() {
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
            vec2 uv = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);
            vec3 pos = vec3(uv, 1);
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
        uniform vec4 u_params;

        in vec4 frag_color;
        in vec2 frag_uv;

        out vec4 out_color;
        void main() {
            out_color = frag_color;
            switch (u_type) {
                case 0: { // QUAD_rect
                } break;
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
                case 4: { // QUAD_arc
                    float min_d = u_params.x;
                    float max_d = u_params.y;
                    vec2 diff = frag_uv - vec2(0.5);
                    float dist = length(diff);
                    if (dist < min_d || dist > max_d) {
                        out_color.a = 0;
                        return;
                    }
                    float min_a = u_params.z;
                    float max_a = u_params.w;
                    float angle = atan(diff.y, diff.x);
                    bool in_range = false;
                    if (min_a < max_a) in_range =   angle > min_a && angle < max_a;
                    if (min_a > max_a) in_range = !(angle > max_a && angle < min_a);
                    if (!in_range) {
                        out_color.a = 0;
                        return;
                    }
                } break;
            }
        }

        )fragment_shader"
    );

    if (!program) return false;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    quad_shader_program* p = &quad_program;

    p->program = program;
    p->vertex_buffer = vertex_buffer;

    p->u_screen_size = glGetUniformLocation(program, "u_screen_size");
    p->u_transform = glGetUniformLocation(program, "u_transform");
    p->u_type = glGetUniformLocation(program, "u_type");
    p->u_params = glGetUniformLocation(program, "u_params");

    return true;
}

FN_push_quad(push_quad) {
    quad_data_buffer* d = &quad_data;
    assert((size_t)d->quad_count < ARRAY_LEN(d->instances));
    quad_instance_data* inst = d->instances + d->quad_count;
    quad_vertex_data* vertex = d->vertices + d->quad_count * 4;
    ++d->quad_count;

    inst->type = type;
    inst->texture_id = texture_id;
    inst->transform = transform;
    inst->params = params;

    vec2 uv0 = v2(0);
    vec2 uv1 = v2(1);
    vertex[0] = {uv0, c0};
    vertex[1] = {v2(uv1.s, uv0.t), c1};
    vertex[2] = {v2(uv0.s, uv1.t), c2};
    vertex[3] = {uv1, c3};
}

void flush_quads(int win_w, int win_h) {
    quad_shader_program* p = &quad_program;
    quad_data_buffer* d = &quad_data;

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
        vec4 v = inst->params;
        glUniform4f(p->u_params, v.x, v.y, v.z, v.w);
        glBindTexture(GL_TEXTURE_2D, inst->texture_id);
        glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4);
    }
    // glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, quad_count);

    d->quad_count = 0;
}
