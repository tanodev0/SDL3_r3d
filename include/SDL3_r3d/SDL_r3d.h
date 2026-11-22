/*
  SDL3_r3d: a small software 3D layer on top of SDL_Renderer.
  Copyright (C) 2026 TanoDev0

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/**
 * # CategorySDLR3D
 *
 * Header file for SDL3_r3d.
 *
 * SDL3_r3d draws simple 3D scenes (lines, points, filled triangles, meshes,
 * grids, surfaces and text labels) using only the 2D SDL_Renderer API: all
 * transforms, clipping, lighting and depth sorting are done on the CPU.
 * No shaders, no GPU API.
 *
 * Conventions:
 *
 * - Right-handed coordinate system. Which axis is "up" is up to you
 *   (R3D_Camera::up); the examples use Z up.
 * - Angles are in radians.
 * - Matrices are column-major (see R3D_Mat4) and multiply column vectors:
 *   `p' = M * p`.
 * - Triangles whose corners appear counter-clockwise when seen from the
 *   front are front faces (used by back-face culling and lighting).
 *
 * Usage:
 *
 * ```c
 * R3D_BeginFrame(ctx, &camera);
 * R3D_DrawGrid(ctx, 20, 1.0f);
 * R3D_DrawMesh(ctx, cube, &model);
 * R3D_EndFrame(ctx);          // sorts and draws everything
 * SDL_RenderPresent(renderer);
 * ```
 *
 * All drawing calls are queued and drawn back to front in R3D_EndFrame(),
 * so lines, points and triangles hide each other correctly (per primitive,
 * not per pixel).
 *
 * This is an unofficial third-party library; it is not part of SDL.
 */

#ifndef SDL_R3D_H_
#define SDL_R3D_H_

#include <SDL3/SDL.h>
#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* Version                                                                  */
/* ------------------------------------------------------------------------ */

#define SDL_R3D_MAJOR_VERSION   0
#define SDL_R3D_MINOR_VERSION   1
#define SDL_R3D_MICRO_VERSION   0

/** This is the version number macro for the current SDL3_r3d version. */
#define SDL_R3D_VERSION \
    SDL_VERSIONNUM(SDL_R3D_MAJOR_VERSION, SDL_R3D_MINOR_VERSION, SDL_R3D_MICRO_VERSION)

/** This macro will evaluate to true if compiled with SDL3_r3d at least X.Y.Z. */
#define SDL_R3D_VERSION_ATLEAST(X, Y, Z) \
    ((SDL_R3D_MAJOR_VERSION >= X) && \
     (SDL_R3D_MAJOR_VERSION > X || SDL_R3D_MINOR_VERSION >= Y) && \
     (SDL_R3D_MAJOR_VERSION > X || SDL_R3D_MINOR_VERSION > Y || SDL_R3D_MICRO_VERSION >= Z))

/**
 * Get the version of the dynamically linked SDL3_r3d library.
 *
 * \returns SDL3_r3d version (see SDL_VERSIONNUM).
 */
extern SDL_DECLSPEC int SDLCALL R3D_Version(void);

/* ------------------------------------------------------------------------ */
/* Math types                                                               */
/* ------------------------------------------------------------------------ */

/** A 3D vector or point. */
typedef struct R3D_Vec3
{
    float x, y, z;
} R3D_Vec3;

/** A 4D vector, used for homogeneous coordinates (clip space). */
typedef struct R3D_Vec4
{
    float x, y, z, w;
} R3D_Vec4;

/**
 * A 4x4 matrix stored in column-major order:
 * element (row r, column c) is m[c * 4 + r].
 */
typedef struct R3D_Mat4
{
    float m[16];
} R3D_Mat4;

/* ------------------------------------------------------------------------ */
/* Vector functions                                                         */
/* ------------------------------------------------------------------------ */

/** \returns a + b. */
extern SDL_DECLSPEC R3D_Vec3 SDLCALL R3D_Vec3Add(R3D_Vec3 a, R3D_Vec3 b);

/** \returns a - b. */
extern SDL_DECLSPEC R3D_Vec3 SDLCALL R3D_Vec3Sub(R3D_Vec3 a, R3D_Vec3 b);

/** \returns v * s. */
extern SDL_DECLSPEC R3D_Vec3 SDLCALL R3D_Vec3Scale(R3D_Vec3 v, float s);

/** \returns the dot product of a and b. */
extern SDL_DECLSPEC float SDLCALL R3D_Vec3Dot(R3D_Vec3 a, R3D_Vec3 b);

/** \returns the cross product a x b. */
extern SDL_DECLSPEC R3D_Vec3 SDLCALL R3D_Vec3Cross(R3D_Vec3 a, R3D_Vec3 b);

/** \returns the length (magnitude) of v. */
extern SDL_DECLSPEC float SDLCALL R3D_Vec3Length(R3D_Vec3 v);

/** \returns v scaled to length 1, or a zero vector if v has length 0. */
extern SDL_DECLSPEC R3D_Vec3 SDLCALL R3D_Vec3Normalize(R3D_Vec3 v);

/** \returns the linear interpolation a + (b - a) * t (t = 0 gives a, t = 1 gives b). */
extern SDL_DECLSPEC R3D_Vec3 SDLCALL R3D_Vec3Lerp(R3D_Vec3 a, R3D_Vec3 b, float t);

/* ------------------------------------------------------------------------ */
/* Matrix functions                                                         */
/* ------------------------------------------------------------------------ */

/** \returns the identity matrix. */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4Identity(void);

/** \returns a * b (apply b first, then a). */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4Multiply(R3D_Mat4 a, R3D_Mat4 b);

/** \returns m * v. */
extern SDL_DECLSPEC R3D_Vec4 SDLCALL R3D_Mat4MultiplyVec4(R3D_Mat4 m, R3D_Vec4 v);

/**
 * Transform a point by an affine matrix (translation, rotation, scale).
 *
 * \returns m * (p, 1), without the perspective divide.
 */
extern SDL_DECLSPEC R3D_Vec3 SDLCALL R3D_Mat4TransformPoint(R3D_Mat4 m, R3D_Vec3 p);

/** \returns the transpose of m (rows become columns). */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4Transpose(R3D_Mat4 m);

/**
 * Invert a matrix.
 *
 * \param m the matrix to invert.
 * \param out receives the inverse.
 * \returns true on success, or false if m cannot be inverted; call
 *          SDL_GetError() for more information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_Mat4Invert(R3D_Mat4 m, R3D_Mat4 *out);

/** \returns a translation matrix by t. */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4Translate(R3D_Vec3 t);

/** \returns a scale matrix by s. */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4Scale(R3D_Vec3 s);

/** \returns a rotation matrix around the X axis (radians, right-handed). */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4RotateX(float angle);

/** \returns a rotation matrix around the Y axis (radians, right-handed). */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4RotateY(float angle);

/** \returns a rotation matrix around the Z axis (radians, right-handed). */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4RotateZ(float angle);

/**
 * \returns a rotation matrix of `angle` radians around any axis (the axis
 *          does not need to be normalized; a zero axis gives the identity).
 */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4Rotate(R3D_Vec3 axis, float angle);

/**
 * Create a perspective projection matrix (OpenGL-style clip space,
 * camera looking down its -Z axis, depth mapped to [-1, 1]).
 *
 * \param fovy vertical field of view, in radians.
 * \param aspect width / height of the viewport.
 * \param near_plane distance to the near clipping plane (> 0).
 * \param far_plane distance to the far clipping plane (> near_plane).
 * \returns the projection matrix.
 */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4Perspective(float fovy, float aspect, float near_plane, float far_plane);

/**
 * Create a view matrix for a camera at `eye` looking at `target`.
 *
 * \param eye camera position.
 * \param target point the camera looks at.
 * \param up which direction is "up" in the world (e.g. {0, 0, 1} for Z-up).
 * \returns the view matrix.
 */
extern SDL_DECLSPEC R3D_Mat4 SDLCALL R3D_Mat4LookAt(R3D_Vec3 eye, R3D_Vec3 target, R3D_Vec3 up);

/* ------------------------------------------------------------------------ */
/* Camera                                                                   */
/* ------------------------------------------------------------------------ */

/** A perspective camera. */
typedef struct R3D_Camera
{
    R3D_Vec3 position;     /**< where the camera is */
    R3D_Vec3 target;       /**< what it looks at */
    R3D_Vec3 up;           /**< world "up" direction */
    float fovy;            /**< vertical field of view, radians */
    float near_plane;      /**< near clipping distance (> 0) */
    float far_plane;       /**< far clipping distance */
} R3D_Camera;

/**
 * Orbit the camera around its target (useful for mouse dragging).
 *
 * \param camera the camera to modify.
 * \param yaw_delta rotation around the up axis, radians.
 * \param pitch_delta rotation up/down, radians (clamped so it never flips).
 * \param zoom_factor multiplies the distance to the target (1.0 = no change).
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_OrbitCamera(R3D_Camera *camera, float yaw_delta, float pitch_delta, float zoom_factor);

/* ------------------------------------------------------------------------ */
/* Context and frame                                                        */
/* ------------------------------------------------------------------------ */

/**
 * The opaque drawing context. One per SDL_Renderer.
 */
typedef struct R3D_Context R3D_Context;

/**
 * Counters for the last finished frame, for debugging and profiling.
 */
typedef struct R3D_Stats
{
    int triangles;          /**< triangles drawn */
    int lines;              /**< lines drawn */
    int points;             /**< points drawn */
    int culled_triangles;   /**< triangles skipped by back-face culling */
    int clipped_primitives; /**< primitives fully behind the camera */
} R3D_Stats;

/**
 * Create a 3D drawing context for an SDL renderer.
 *
 * Defaults: draw color opaque white, back-face culling off, lighting off.
 *
 * \param renderer the renderer to draw with.
 * \returns a new context, or NULL on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC R3D_Context * SDLCALL R3D_CreateContext(SDL_Renderer *renderer);

/**
 * Destroy a context created with R3D_CreateContext(). NULL is ignored.
 */
extern SDL_DECLSPEC void SDLCALL R3D_DestroyContext(R3D_Context *ctx);

/**
 * Start a 3D frame using the given camera. Call before any R3D_Draw*().
 *
 * The viewport is the renderer's logical size if logical presentation is
 * enabled, otherwise its current output size.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_BeginFrame(R3D_Context *ctx, const R3D_Camera *camera);

/**
 * Finish the frame: depth-sort everything queued since R3D_BeginFrame() and
 * submit it to the renderer. Does not call SDL_RenderPresent(). The
 * renderer's draw color is left unchanged.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_EndFrame(R3D_Context *ctx);

/**
 * Get the counters of the last finished frame.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_GetStats(R3D_Context *ctx, R3D_Stats *stats);

/**
 * Set the color used by the following R3D_Draw*() calls (meshes use their
 * own vertex colors).
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_SetDrawColor(R3D_Context *ctx, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/**
 * Enable or disable back-face culling: triangles seen from behind are not
 * drawn. Faster for closed meshes (cubes, spheres); turn it off for open
 * surfaces. Off by default.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_SetCulling(R3D_Context *ctx, bool enabled);

/**
 * Configure simple directional lighting for filled triangles.
 *
 * Each triangle is shaded by the angle between its normal and the light:
 * `brightness = ambient + (1 - ambient) * max(0, dot(normal, -direction))`.
 * Lines, points and text are never lit. Off by default.
 *
 * \param enabled true to turn lighting on.
 * \param direction the direction the light travels (e.g. {-1, -1, -2} for a
 *                  light coming from above); does not need to be normalized.
 * \param ambient minimum brightness in [0, 1], for faces facing away.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_SetLighting(R3D_Context *ctx, bool enabled, R3D_Vec3 direction, float ambient);

/**
 * Project a world-space point to renderer coordinates.
 *
 * \param ctx the context (inside a frame).
 * \param point the point in world space.
 * \param out receives the screen position if the point is visible.
 * \returns true if the point is in front of the camera, false otherwise.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_ProjectPoint(R3D_Context *ctx, R3D_Vec3 point, SDL_FPoint *out);

/**
 * Get the world-space ray that goes through a screen position (the inverse
 * of R3D_ProjectPoint). Useful for picking objects with the mouse.
 *
 * \param ctx the context (inside a frame).
 * \param x screen x, in renderer coordinates.
 * \param y screen y, in renderer coordinates.
 * \param origin receives the ray origin (on the near plane).
 * \param direction receives the normalized ray direction.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_ScreenToRay(R3D_Context *ctx, float x, float y, R3D_Vec3 *origin, R3D_Vec3 *direction);

/* ------------------------------------------------------------------------ */
/* Drawing                                                                  */
/* ------------------------------------------------------------------------ */

/**
 * Draw a 3D line, clipped against the near plane.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawLine3D(R3D_Context *ctx, R3D_Vec3 a, R3D_Vec3 b);

/**
 * Draw a 3D point as a square of `size` pixels (size does not shrink with
 * distance).
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawPoint3D(R3D_Context *ctx, R3D_Vec3 p, float size);

/**
 * Draw a filled 3D triangle, clipped against the near plane.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawTriangle3D(R3D_Context *ctx, R3D_Vec3 a, R3D_Vec3 b, R3D_Vec3 c);

/**
 * Draw a filled convex polygon (all points on one plane). It is split into
 * triangles internally.
 *
 * \param points the corners, in order (clockwise or counter-clockwise).
 * \param num_points number of corners (>= 3).
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawPolygon3D(R3D_Context *ctx, const R3D_Vec3 *points, int num_points);

/**
 * Draw a text label at a 3D position using SDL's built-in debug font
 * (SDL_RenderDebugText). Labels are always drawn on top of the scene.
 *
 * \param p where the label goes (its top-left corner).
 * \param text the text, UTF-8. It is copied.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawText3D(R3D_Context *ctx, R3D_Vec3 p, const char *text);

/**
 * Draw a square grid on the plane z = 0, centered at the origin.
 *
 * \param slices number of cells per side.
 * \param spacing size of each cell.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawGrid(R3D_Context *ctx, int slices, float spacing);

/**
 * Draw the X (red), Y (green) and Z (blue) axes from the origin.
 *
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawAxes(R3D_Context *ctx, float length);

/* ------------------------------------------------------------------------ */
/* Meshes                                                                   */
/* ------------------------------------------------------------------------ */

/** A mesh vertex: a position and a color. */
typedef struct R3D_Vertex
{
    R3D_Vec3 position;
    SDL_FColor color;
} R3D_Vertex;

/**
 * An opaque mesh: any shape, stored as vertices plus triangle indices
 * (the same layout SDL_RenderGeometry() uses).
 */
typedef struct R3D_Mesh R3D_Mesh;

/**
 * Create a mesh from vertices and triangle indices. The data is copied.
 *
 * \param vertices the vertices.
 * \param num_vertices number of vertices (>= 3).
 * \param indices three indices per triangle, or NULL to use the vertices in
 *                order (every 3 vertices form a triangle).
 * \param num_indices number of indices (a multiple of 3), or 0 if indices is
 *                    NULL.
 * \returns a new mesh, or NULL on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC R3D_Mesh * SDLCALL R3D_CreateMesh(const R3D_Vertex *vertices, int num_vertices,
                                                      const int *indices, int num_indices);

/**
 * Destroy a mesh. NULL is ignored.
 */
extern SDL_DECLSPEC void SDLCALL R3D_DestroyMesh(R3D_Mesh *mesh);

/**
 * Create an axis-aligned cube centered at the origin, with outward-facing
 * triangles.
 *
 * \returns a new mesh, or NULL on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC R3D_Mesh * SDLCALL R3D_CreateCubeMesh(float size, SDL_FColor color);

/**
 * Create a UV sphere centered at the origin (poles on the Z axis), with
 * outward-facing triangles.
 *
 * \param rings horizontal divisions (>= 2).
 * \param segments vertical divisions (>= 3).
 * \returns a new mesh, or NULL on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC R3D_Mesh * SDLCALL R3D_CreateSphereMesh(float radius, int rings, int segments, SDL_FColor color);

/**
 * Create a flat rectangle on the plane z = 0, centered at the origin,
 * facing +Z.
 *
 * Large flat polygons sort badly against nearby objects (see the
 * limitations in the README), so the plane is split into a grid of cells.
 *
 * \param subdivisions cells per side (>= 1); around one cell per object-size
 *                     unit works well for floors.
 * \returns a new mesh, or NULL on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC R3D_Mesh * SDLCALL R3D_CreatePlaneMesh(float width, float length, int subdivisions, SDL_FColor color);

/**
 * Draw a mesh with filled triangles, using its vertex colors.
 *
 * \param transform model matrix (position/rotation/scale), or NULL for none.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawMesh(R3D_Context *ctx, const R3D_Mesh *mesh, const R3D_Mat4 *transform);

/**
 * Draw the edges of a mesh's triangles with the current draw color.
 *
 * \param transform model matrix, or NULL for none.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawMeshWireframe(R3D_Context *ctx, const R3D_Mesh *mesh, const R3D_Mat4 *transform);

/* ------------------------------------------------------------------------ */
/* Surfaces                                                                 */
/* ------------------------------------------------------------------------ */

/**
 * A function z = f(x, y) for R3D_DrawSurface(). It may return NaN or
 * infinity where it is undefined; those samples are skipped.
 */
typedef float (SDLCALL *R3D_SurfaceFunction)(void *userdata, float x, float y);

/**
 * Draw the surface z = func(x, y) over [x_min, x_max] x [y_min, y_max] with
 * the current draw color.
 *
 * \param resolution number of samples per side (>= 2).
 * \param filled true for filled triangles, false for a wireframe.
 * \returns true on success or false on failure; call SDL_GetError() for more
 *          information.
 */
extern SDL_DECLSPEC bool SDLCALL R3D_DrawSurface(R3D_Context *ctx,
                                                 R3D_SurfaceFunction func, void *userdata,
                                                 float x_min, float x_max,
                                                 float y_min, float y_max,
                                                 int resolution, bool filled);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_R3D_H_ */