#ifndef __MATH_HELPER_H__

#include "assert.h"
#include "math.h"

float min(float a, float b) { return a < b ? a : b; }
float max(float a, float b) { return a > b ? a : b; }
float clamp(float f, float lo, float hi) {
    assert(lo < hi);
    return min(hi, max(f, lo));
}

typedef union vec2 {
    float f[2];
    struct { float x, y; };
    struct { float s, t; };
} vec2;

vec2 v2(float x, float y) { return (vec2){{x, y}}; }

vec2 v2_add(vec2 a, vec2 b) {
    return v2(a.x + b.x, a.y + b.y);
}

vec2 v2_sub(vec2 a, vec2 b) {
    return v2(a.x - b.x, a.y - b.y);
}

float v2_dot(vec2 a, vec2 b) {
    return a.x * b.x + a.y * b.y;
}

typedef union vec3 {
    float f[3];
    struct { float x, y, z; };
    struct { float r, g, b; };
    struct { float s, t, p; };
    vec2 xy;
    vec2 st;
} vec3;

vec3 v3(float x, float y, float z) { return (vec3){{x, y, z}}; }

float v3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 v3_cross(vec3 a, vec3 b) {
    return v3(a.y * b.z - a.z * b.y,
              a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x);
}

typedef union vec4 {
    float f[4];
    struct { float x, y, z, w; };
    struct { float r, g, b, a; };
    struct { float s, t, p, q; };
    struct { vec2 xy, zw; };
    struct { vec2 st, pq; };
    vec3 xyz;
} vec4;

vec4 v4(float x, float y, float z, float w) { return (vec4){{x, y, z, w}}; }

typedef union mat3 {
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
    vec3 rows[3];
} mat3;

mat3 m3(float _11, float _12, float _13,
        float _21, float _22, float _23,
        float _31, float _32, float _33) {
    mat3 result = {{
        _11, _12, _13,
        _21, _22, _23,
        _31, _32, _33,
    }};
    return result;
}

mat3 m3_from_rows(vec3 r1, vec3 r2, vec3 r3) {
    mat3 result;
    result.r1 = r1;
    result.r2 = r2;
    result.r3 = r3;
    return result;
}

mat3 m3_transpose(mat3 m) {
    return m3(m._11, m._21, m._31,
              m._12, m._22, m._32,
              m._13, m._23, m._33);
}

mat3 m3_identity() {
    return m3(1, 0, 0,
              0, 1, 0,
              0, 0, 1);
}

mat3 m3_rotation(float angle) {
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    return m3(cos_a, -sin_a,  0,
              sin_a,  cos_a,  0,
                  0,      0,  1);
}

mat3 m3_scale(float x, float y) {
    return m3(x, 0, 0,
              0, y, 0,
              0, 0, 1);
}

mat3 m3_translation(float x, float y) {
    return m3(1, 0, x,
              0, 1, y,
              0, 0, 1);
}


#ifdef __cplusplus
vec2 v2(float x) { return v2(x, x); }
vec2 operator + (vec2 a, vec2 b)  { return v2_add(a, b); }
vec2 operator - (vec2 a, vec2 b)  { return v2_sub(a, b); }
vec2 operator * (float a, vec2 v) { return v2(a * v.x, a * v.y); }
vec2 operator += (vec2 a, vec2 b) { return a + b; }

vec3 operator * (float a, vec3 v) { return v3(a * v.x, a * v.y, a * v.z); }

vec4 v4(vec2 xy, vec2 zw) { return v4(xy.x, xy.y, zw.x, zw.y); }

mat3 operator * (float a, mat3 m) {
    for (size_t i = 0; i < 9; ++i) m.f[i] *= a;
    return m;
}

mat3 m3(float a) {
    return m3(a, 0, 0,
              0, a, 0,
              0, 0, a);
}

float dot(vec2 a, vec2 b) { return v2_dot(a, b); }
float dot(vec3 a, vec3 b) { return v3_dot(a, b); }

vec3 cross(vec3 a, vec3 b) { return v3_cross(a, b); }

vec3 operator * (mat3 m, vec3 v) {
    return v3(dot(m.r1, v),
              dot(m.r2, v),
              dot(m.r3, v));
}

mat3 operator * (mat3 a, mat3 b) {
    b = m3_transpose(b);
    mat3 result = m3_from_rows(
        a * b.r1,
        a * b.r2,
        a * b.r3
    );
    return m3_transpose(result);
}

mat3 m3_inverse(mat3 m) {
    m = m3_transpose(m);
    vec3 x0 = m.r1;
    vec3 x1 = m.r2;
    vec3 x2 = m.r3;
    float det = 1 / v3_dot(x0, cross(x1, x2));
    m = m3_from_rows(cross(x1, x2),
                     cross(x2, x0),
                     cross(x0, x1));
    return det * m;
}

mat3 m3_translation(vec2 offset) { return m3_translation(offset.x, offset.y); }
mat3 m3_scale(vec2 scale) { return m3_scale(scale.x, scale.y); }

#endif

#define __MATH_HELPER_H__
#endif // __MATH_HELPER_H__
