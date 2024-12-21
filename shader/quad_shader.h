

//
// quad shader
//

typedef struct {
    GLuint program;
    GLuint u_transform;
    GLuint u_color;
    GLuint u_type;
} quad_program;

bool init_quad_program(quad_program* p) {
    GLuint program = load_shader_from_file("shader/vert.glsl", "shader/frag.glsl");
    if (!program) return false;

    p->program = program;
    p->u_transform = glGetUniformLocation(program, "transform");
    p->u_color = glGetUniformLocation(program, "color");
    p->u_type = glGetUniformLocation(program, "type");

    return true;
}

#include "shader_def.h"
void render_quad(quad_program* qp, mat3 view_transform, int win_w, int win_h, int type, float x, float y, float w, float h, color c) {
    mat3 transform = identity3();

    // object transform
    transform = mat3xmat3(scale_2d(w/2.0, h/2.0), transform);
    transform = mat3xmat3(translate_2d(x, y), transform);

    // view transform
    transform = mat3xmat3(view_transform, transform);

    // screen projection
    transform = mat3xmat3(scale_2d(2.0/win_w, -2.0/win_h), transform);
    transform = mat3xmat3(translate_2d(-1, 1), transform);

    glUseProgram(qp->program);
    glUniformMatrix3fv(qp->u_transform, 1, GL_TRUE, transform.f);
    glUniform4f(qp->u_color, c.r, c.g, c.b, c.a);
    glUniform1i(qp->u_type, type);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
