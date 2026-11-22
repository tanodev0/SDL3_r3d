/*
  SDL3_r3d: math tests.

  Each CHECK tests ONE thing against a value computed by hand.
  They all fail at first (the functions are empty): that is expected.
  Fill in functions until the output says "0 failed".

  Run:  ctest --test-dir build --output-on-failure
   or:  ./build/testmath
*/

#include <SDL3_r3d/SDL_r3d.h>
#include <stdio.h>

static int num_tests = 0;
static int num_failed = 0;

#define EPS 1e-4f

static bool near_eq(float a, float b) { return SDL_fabsf(a - b) < EPS; }

#define CHECK(cond, name)                                              \
    do {                                                               \
        num_tests++;                                                   \
        if (cond) { printf("  ok    %s\n", name); }                    \
        else      { printf("  FAIL  %s  (%s:%d)\n", name, __FILE__, __LINE__); num_failed++; } \
    } while (0)

#define CHECK_VEC3(v, ex, ey, ez, name) \
    CHECK(near_eq((v).x, ex) && near_eq((v).y, ey) && near_eq((v).z, ez), name)

static void test_vec3(void)
{
    printf("Vec3\n");
    R3D_Vec3 a = { 1, 2, 3 };
    R3D_Vec3 b = { 4, 5, 6 };

    CHECK_VEC3(R3D_Vec3Add(a, b), 5, 7, 9, "Add");
    CHECK_VEC3(R3D_Vec3Sub(b, a), 3, 3, 3, "Sub");
    CHECK_VEC3(R3D_Vec3Scale(a, 2), 2, 4, 6, "Scale");
    CHECK(near_eq(R3D_Vec3Dot(a, b), 32), "Dot: 1*4 + 2*5 + 3*6 = 32");

    /* X x Y = Z (right-hand rule) */
    CHECK_VEC3(R3D_Vec3Cross((R3D_Vec3){ 1, 0, 0 }, (R3D_Vec3){ 0, 1, 0 }), 0, 0, 1, "Cross: X x Y = Z");
    CHECK_VEC3(R3D_Vec3Cross((R3D_Vec3){ 0, 1, 0 }, (R3D_Vec3){ 1, 0, 0 }), 0, 0, -1, "Cross: Y x X = -Z");

    CHECK(near_eq(R3D_Vec3Length((R3D_Vec3){ 3, 4, 0 }), 5), "Length (3,4,0) = 5");
    CHECK_VEC3(R3D_Vec3Normalize((R3D_Vec3){ 0, 0, 10 }), 0, 0, 1, "Normalize (0,0,10)");
    CHECK_VEC3(R3D_Vec3Normalize((R3D_Vec3){ 0, 0, 0 }), 0, 0, 0, "Normalize zero vector -> zero (no NaN)");
    CHECK_VEC3(R3D_Vec3Lerp(a, b, 0.5f), 2.5f, 3.5f, 4.5f, "Lerp halfway");
}

static void test_mat4(void)
{
    printf("Mat4\n");
    R3D_Vec4 p = { 1, 2, 3, 1 };

    R3D_Vec4 r = R3D_Mat4MultiplyVec4(R3D_Mat4Identity(), p);
    CHECK(near_eq(r.x, 1) && near_eq(r.y, 2) && near_eq(r.z, 3) && near_eq(r.w, 1), "Identity * p = p");

    r = R3D_Mat4MultiplyVec4(R3D_Mat4Translate((R3D_Vec3){ 10, 20, 30 }), p);
    CHECK(near_eq(r.x, 11) && near_eq(r.y, 22) && near_eq(r.z, 33) && near_eq(r.w, 1), "Translate moves points");

    R3D_Vec4 dir = { 1, 2, 3, 0 };   /* w = 0: a direction, not a point */
    r = R3D_Mat4MultiplyVec4(R3D_Mat4Translate((R3D_Vec3){ 10, 20, 30 }), dir);
    CHECK(near_eq(r.x, 1) && near_eq(r.y, 2) && near_eq(r.z, 3), "Translate does NOT move directions (w=0)");

    r = R3D_Mat4MultiplyVec4(R3D_Mat4Scale((R3D_Vec3){ 2, 3, 4 }), p);
    CHECK(near_eq(r.x, 2) && near_eq(r.y, 6) && near_eq(r.z, 12), "Scale");

    /* Rotating X by 90 degrees around Z gives Y */
    r = R3D_Mat4MultiplyVec4(R3D_Mat4RotateZ(SDL_PI_F / 2), (R3D_Vec4){ 1, 0, 0, 1 });
    CHECK(near_eq(r.x, 0) && near_eq(r.y, 1) && near_eq(r.z, 0), "RotateZ 90: X -> Y");

    /* Multiply: scale by 2 first, then translate by +1  =>  (1,2,3) -> (3,5,7) */
    R3D_Mat4 m = R3D_Mat4Multiply(R3D_Mat4Translate((R3D_Vec3){ 1, 1, 1 }),
                                  R3D_Mat4Scale((R3D_Vec3){ 2, 2, 2 }));
    r = R3D_Mat4MultiplyVec4(m, p);
    CHECK(near_eq(r.x, 3) && near_eq(r.y, 5) && near_eq(r.z, 7), "Multiply: T * S applies S first");

    /* Rotating Y by 90 degrees around X gives Z; rotating Z around Y gives X */
    r = R3D_Mat4MultiplyVec4(R3D_Mat4RotateX(SDL_PI_F / 2), (R3D_Vec4){ 0, 1, 0, 1 });
    CHECK(near_eq(r.x, 0) && near_eq(r.y, 0) && near_eq(r.z, 1), "RotateX 90: Y -> Z");
    r = R3D_Mat4MultiplyVec4(R3D_Mat4RotateY(SDL_PI_F / 2), (R3D_Vec4){ 0, 0, 1, 1 });
    CHECK(near_eq(r.x, 1) && near_eq(r.y, 0) && near_eq(r.z, 0), "RotateY 90: Z -> X");

    /* Rotate around an arbitrary axis matches the axis-specific versions */
    R3D_Vec3 q = R3D_Mat4TransformPoint(R3D_Mat4Rotate((R3D_Vec3){ 0, 0, 5 }, SDL_PI_F / 2), (R3D_Vec3){ 1, 0, 0 });
    CHECK_VEC3(q, 0, 1, 0, "Rotate(axis Z, 90): X -> Y (axis not normalized)");
    q = R3D_Mat4TransformPoint(R3D_Mat4Rotate((R3D_Vec3){ 1, 1, 1 }, 2 * SDL_PI_F / 3), (R3D_Vec3){ 1, 0, 0 });
    CHECK_VEC3(q, 0, 1, 0, "Rotate(axis (1,1,1), 120): X -> Y");

    /* Transpose swaps rows and columns */
    R3D_Mat4 t = R3D_Mat4Transpose(R3D_Mat4Translate((R3D_Vec3){ 1, 2, 3 }));
    CHECK(near_eq(t.m[3], 1) && near_eq(t.m[7], 2) && near_eq(t.m[11], 3) && near_eq(t.m[12], 0), "Transpose");

    /* Invert: M * inverse(M) = identity */
    R3D_Mat4 any = R3D_Mat4Multiply(R3D_Mat4Translate((R3D_Vec3){ 3, -2, 5 }),
                   R3D_Mat4Multiply(R3D_Mat4Rotate((R3D_Vec3){ 1, 2, 3 }, 0.7f),
                                    R3D_Mat4Scale((R3D_Vec3){ 2, 0.5f, 3 })));
    R3D_Mat4 inv;
    bool inverted = R3D_Mat4Invert(any, &inv);
    R3D_Mat4 id = R3D_Mat4Multiply(any, inv);
    bool is_identity = true;
    for (int i = 0; i < 16; i++) {
        float expected = (i % 5 == 0) ? 1.0f : 0.0f;
        if (!near_eq(id.m[i], expected)) is_identity = false;
    }
    CHECK(inverted && is_identity, "Invert: M * inverse(M) = I");
    CHECK(!R3D_Mat4Invert(R3D_Mat4Scale((R3D_Vec3){ 1, 0, 1 }), &inv), "Invert fails on a singular matrix");

    /* LookAt: the target ends up straight ahead, on the camera's -Z axis */
    R3D_Mat4 view = R3D_Mat4LookAt((R3D_Vec3){ 10, 0, 0 }, (R3D_Vec3){ 0, 0, 0 }, (R3D_Vec3){ 0, 0, 1 });
    q = R3D_Mat4TransformPoint(view, (R3D_Vec3){ 0, 0, 0 });
    CHECK_VEC3(q, 0, 0, -10, "LookAt: target is 10 units ahead (-Z)");
    q = R3D_Mat4TransformPoint(view, (R3D_Vec3){ 0, 0, 1 });
    CHECK(near_eq(q.x, 0) && near_eq(q.y, 1), "LookAt: world up stays up");

    /* Perspective: points on the near/far planes map to z/w = -1 / +1 */
    R3D_Mat4 proj = R3D_Mat4Perspective(SDL_PI_F / 2, 1.0f, 1.0f, 100.0f);
    R3D_Vec4 pn = R3D_Mat4MultiplyVec4(proj, (R3D_Vec4){ 0, 0, -1, 1 });
    R3D_Vec4 pf = R3D_Mat4MultiplyVec4(proj, (R3D_Vec4){ 0, 0, -100, 1 });
    CHECK(near_eq(pn.z / pn.w, -1) && near_eq(pf.z / pf.w, 1), "Perspective: near -> -1, far -> +1");
    R3D_Vec4 edge = R3D_Mat4MultiplyVec4(proj, (R3D_Vec4){ 0, 5, -5, 1 });
    CHECK(near_eq(edge.y / edge.w, 1), "Perspective: 90 degree fov reaches the top edge at y = distance");
}

int main(void)
{
    test_vec3();
    test_mat4();

    printf("\n%d tests, %d failed\n", num_tests, num_failed);
    return num_failed == 0 ? 0 : 1;
}
