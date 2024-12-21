#version 330

uniform mat3 transform;
out vec2 uv;
void main() {
    uv = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);
    uv = uv * 2.0 - 1.0;
    vec3 pos = transform * vec3(uv, 1);
    gl_Position = vec4(pos.xy, 0, 1);
}