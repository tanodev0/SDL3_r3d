/*
  SDL3_r3d: a small software 3D layer on top of SDL_Renderer.
  Copyright (C) 2026 TanoDev0

  Distributed under the zlib license. See LICENSE.txt.
*/

/* Vectors and matrices. No state, no dynamic memory: just math.
   Matrices are column-major: element (row r, column c) is m[c * 4 + r].

   This file uses only the C standard library (no SDL_ functions), so the
   math layer can be reused and tested on its own. The one exception is
   R3D_Mat4Invert, which reports failure through SDL_SetError like the rest
   of the API. */

#include <math.h>

#include "SDL_r3d_internal.h"

/* Element (row r, column c) of a column-major matrix. */
#define M(mat, r, c) ((mat).m[(c) * 4 + (r)])

/* ----- Helper Functions -------------------------------------------------------------- */

/* Returns x squared. */
static float r3d_sqf(float x)
{
    return x * x;
}

/* ----- Vec3 -------------------------------------------------------------- */

/* Returns a + b, component by component. */
R3D_Vec3 R3D_Vec3Add(R3D_Vec3 a, R3D_Vec3 b)
{
    return (R3D_Vec3){
            a.x+b.x,
            a.y+b.y,
            a.z+b.z
    };
}

/* Returns a - b, component by component. */
R3D_Vec3 R3D_Vec3Sub(R3D_Vec3 a, R3D_Vec3 b)
{
    return (R3D_Vec3){
            a.x-b.x,
            a.y-b.y,
            a.z-b.z
    };
}

/* Returns v with every component multiplied by s. */
R3D_Vec3 R3D_Vec3Scale(R3D_Vec3 v, float s)
{
    return (R3D_Vec3){
            v.x*s,
            v.y*s,
            v.z*s
    };
}

/* Returns the dot product a·b: how much a and b point the same way. */
float R3D_Vec3Dot(R3D_Vec3 a, R3D_Vec3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

/* Returns the cross product a x b: a vector perpendicular to both. */
R3D_Vec3 R3D_Vec3Cross(R3D_Vec3 a, R3D_Vec3 b)
{
    return (R3D_Vec3){
            (a.y*b.z) - (a.z*b.y),
            (a.z*b.x) - (a.x*b.z),
            (a.x*b.y) - (a.y*b.x)
    };
}

/* Returns the length (magnitude) of v. */
float R3D_Vec3Length(R3D_Vec3 v)
{
    return sqrtf(r3d_sqf(v.x) + r3d_sqf(v.y) + r3d_sqf(v.z));
}

/* Returns v scaled to length 1 (or the zero vector if v has length 0). */
R3D_Vec3 R3D_Vec3Normalize(R3D_Vec3 v)
{
    float len = R3D_Vec3Length(v);
    if (len < 1e-12f) {
        return (R3D_Vec3){ 0.0f, 0.0f, 0.0f };
    }
    return R3D_Vec3Scale(v, 1.0f / len);
}

/* Returns the point a fraction t of the way from a to b. */
R3D_Vec3 R3D_Vec3Lerp(R3D_Vec3 a, R3D_Vec3 b, float t)
{
    return R3D_Vec3Add(a, R3D_Vec3Scale(R3D_Vec3Sub(b, a), t));
}

/* ----- Mat4 -------------------------------------------------------------- */

/* Returns the identity matrix (transforms nothing). */
R3D_Mat4 R3D_Mat4Identity(void)
{
    R3D_Mat4 r = { { 0 } };
    M(r, 0, 0) = 1.0f;
    M(r, 1, 1) = 1.0f;
    M(r, 2, 2) = 1.0f;
    M(r, 3, 3) = 1.0f;
    return r;
}

/* Returns a * b: a matrix that applies b first, then a. */
R3D_Mat4 R3D_Mat4Multiply(R3D_Mat4 a, R3D_Mat4 b)
{
    R3D_Mat4 r = { { 0 } };
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += M(a, row, k) * M(b, k, c);
            }
            M(r, row, c) = sum;
        }
    }
    return r;
}

/* Returns m * v: v transformed by m. */
R3D_Vec4 R3D_Mat4MultiplyVec4(R3D_Mat4 m, R3D_Vec4 v)
{
    return (R3D_Vec4){
        M(m, 0, 0) * v.x + M(m, 0, 1) * v.y + M(m, 0, 2) * v.z + M(m, 0, 3) * v.w,
        M(m, 1, 0) * v.x + M(m, 1, 1) * v.y + M(m, 1, 2) * v.z + M(m, 1, 3) * v.w,
        M(m, 2, 0) * v.x + M(m, 2, 1) * v.y + M(m, 2, 2) * v.z + M(m, 2, 3) * v.w,
        M(m, 3, 0) * v.x + M(m, 3, 1) * v.y + M(m, 3, 2) * v.z + M(m, 3, 3) * v.w,
    };
}

/* Returns m * (p, 1) as a 3D point (affine matrices only, no divide by w). */
R3D_Vec3 R3D_Mat4TransformPoint(R3D_Mat4 m, R3D_Vec3 p)
{
    R3D_Vec4 r = R3D_Mat4MultiplyVec4(m, (R3D_Vec4){ p.x, p.y, p.z, 1.0f });
    return (R3D_Vec3){ r.x, r.y, r.z };
}

/* Returns m with rows and columns swapped. */
R3D_Mat4 R3D_Mat4Transpose(R3D_Mat4 m)
{
    R3D_Mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            M(r, row, c) = M(m, c, row);
        }
    }
    return r;
}

/* Computes the inverse of m with cofactors. Fails if m is singular. */
bool R3D_Mat4Invert(R3D_Mat4 m, R3D_Mat4 *out)
{
    if (!out) {
        return SDL_InvalidParamError("out");
    }

    /* Cofactor expansion (works the same for row- and column-major data,
       because inverse(transpose(m)) == transpose(inverse(m))). */
    const float *a = m.m;
    float inv[16];

    inv[0]  =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
    inv[4]  = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
    inv[8]  =  a[4]*a[9]*a[15]  - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
    inv[12] = -a[4]*a[9]*a[14]  + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
    inv[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
    inv[5]  =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
    inv[9]  = -a[0]*a[9]*a[15]  + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
    inv[13] =  a[0]*a[9]*a[14]  - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
    inv[2]  =  a[1]*a[6]*a[15]  - a[1]*a[7]*a[14]  - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7]  - a[13]*a[3]*a[6];
    inv[6]  = -a[0]*a[6]*a[15]  + a[0]*a[7]*a[14]  + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7]  + a[12]*a[3]*a[6];
    inv[10] =  a[0]*a[5]*a[15]  - a[0]*a[7]*a[13]  - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7]  - a[12]*a[3]*a[5];
    inv[14] = -a[0]*a[5]*a[14]  + a[0]*a[6]*a[13]  + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6]  + a[12]*a[2]*a[5];
    inv[3]  = -a[1]*a[6]*a[11]  + a[1]*a[7]*a[10]  + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7]   + a[9]*a[3]*a[6];
    inv[7]  =  a[0]*a[6]*a[11]  - a[0]*a[7]*a[10]  - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7]   - a[8]*a[3]*a[6];
    inv[11] = -a[0]*a[5]*a[11]  + a[0]*a[7]*a[9]   + a[4]*a[1]*a[11] - a[4]*a[3]*a[9]  - a[8]*a[1]*a[7]   + a[8]*a[3]*a[5];
    inv[15] =  a[0]*a[5]*a[10]  - a[0]*a[6]*a[9]   - a[4]*a[1]*a[10] + a[4]*a[2]*a[9]  + a[8]*a[1]*a[6]   - a[8]*a[2]*a[5];

    float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    if (fabsf(det) < 1e-12f) {
        return SDL_SetError("R3D_Mat4Invert: matrix is not invertible");
    }

    float inv_det = 1.0f / det;
    for (int i = 0; i < 16; i++) {
        out->m[i] = inv[i] * inv_det;
    }
    return true;
}

/* Returns a matrix that moves points by t (directions with w = 0 are not moved). */
R3D_Mat4 R3D_Mat4Translate(R3D_Vec3 t)
{
    R3D_Mat4 r = R3D_Mat4Identity();
    M(r, 0, 3) = t.x;
    M(r, 1, 3) = t.y;
    M(r, 2, 3) = t.z;
    return r;
}

/* Returns a matrix that scales by s.x, s.y and s.z along each axis. */
R3D_Mat4 R3D_Mat4Scale(R3D_Vec3 s)
{
    R3D_Mat4 r = R3D_Mat4Identity();
    M(r, 0, 0) = s.x;
    M(r, 1, 1) = s.y;
    M(r, 2, 2) = s.z;
    return r;
}

/* Returns a matrix that rotates `angle` radians around the X axis. */
R3D_Mat4 R3D_Mat4RotateX(float angle)
{
    float c = cosf(angle), s = sinf(angle);
    R3D_Mat4 r = R3D_Mat4Identity();
    M(r, 1, 1) = c;  M(r, 1, 2) = -s;
    M(r, 2, 1) = s;  M(r, 2, 2) = c;
    return r;
}

/* Returns a matrix that rotates `angle` radians around the Y axis. */
R3D_Mat4 R3D_Mat4RotateY(float angle)
{
    float c = cosf(angle), s = sinf(angle);
    R3D_Mat4 r = R3D_Mat4Identity();
    M(r, 0, 0) = c;  M(r, 0, 2) = s;
    M(r, 2, 0) = -s; M(r, 2, 2) = c;
    return r;
}

/* Returns a matrix that rotates `angle` radians around the Z axis. */
R3D_Mat4 R3D_Mat4RotateZ(float angle)
{
    float c = cosf(angle), s = sinf(angle);
    R3D_Mat4 r = R3D_Mat4Identity();
    M(r, 0, 0) = c;  M(r, 0, 1) = -s;
    M(r, 1, 0) = s;  M(r, 1, 1) = c;
    return r;
}

/* Returns a matrix that rotates `angle` radians around any axis (Rodrigues' formula). */
R3D_Mat4 R3D_Mat4Rotate(R3D_Vec3 axis, float angle)
{
    R3D_Vec3 n = R3D_Vec3Normalize(axis);
    if (n.x == 0.0f && n.y == 0.0f && n.z == 0.0f) {
        return R3D_Mat4Identity();
    }

    float c = cosf(angle), s = sinf(angle), t = 1.0f - c;
    R3D_Mat4 r = R3D_Mat4Identity();
    M(r, 0, 0) = t * n.x * n.x + c;
    M(r, 0, 1) = t * n.x * n.y - s * n.z;
    M(r, 0, 2) = t * n.x * n.z + s * n.y;
    M(r, 1, 0) = t * n.x * n.y + s * n.z;
    M(r, 1, 1) = t * n.y * n.y + c;
    M(r, 1, 2) = t * n.y * n.z - s * n.x;
    M(r, 2, 0) = t * n.x * n.z - s * n.y;
    M(r, 2, 1) = t * n.y * n.z + s * n.x;
    M(r, 2, 2) = t * n.z * n.z + c;
    return r;
}

/* Returns a perspective projection matrix: far things look smaller. */
R3D_Mat4 R3D_Mat4Perspective(float fovy, float aspect, float near_plane, float far_plane)
{
    float f = 1.0f / tanf(fovy * 0.5f);
    R3D_Mat4 r = { { 0 } };
    M(r, 0, 0) = f / aspect;
    M(r, 1, 1) = f;
    M(r, 2, 2) = (far_plane + near_plane) / (near_plane - far_plane);
    M(r, 2, 3) = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    M(r, 3, 2) = -1.0f;   /* w = -z_camera = distance in front of the camera */
    return r;
}

/* Returns a view matrix for a camera at `eye` looking at `target`. */
R3D_Mat4 R3D_Mat4LookAt(R3D_Vec3 eye, R3D_Vec3 target, R3D_Vec3 up)
{
    R3D_Vec3 f = R3D_Vec3Normalize(R3D_Vec3Sub(target, eye));   /* forward */
    R3D_Vec3 s = R3D_Vec3Normalize(R3D_Vec3Cross(f, up));       /* right */
    R3D_Vec3 u = R3D_Vec3Cross(s, f);                           /* real up */

    R3D_Mat4 r = R3D_Mat4Identity();
    M(r, 0, 0) = s.x;  M(r, 0, 1) = s.y;  M(r, 0, 2) = s.z;
    M(r, 1, 0) = u.x;  M(r, 1, 1) = u.y;  M(r, 1, 2) = u.z;
    M(r, 2, 0) = -f.x; M(r, 2, 1) = -f.y; M(r, 2, 2) = -f.z;
    M(r, 0, 3) = -R3D_Vec3Dot(s, eye);
    M(r, 1, 3) = -R3D_Vec3Dot(u, eye);
    M(r, 2, 3) = R3D_Vec3Dot(f, eye);
    return r;
}
