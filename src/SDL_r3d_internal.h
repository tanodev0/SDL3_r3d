/*
  SDL3_r3d: a small software 3D layer on top of SDL_Renderer.
  Copyright (C) 2026 TanoDev0

  Distributed under the zlib license. See LICENSE.txt.
*/

/* Internal definitions. Not installed and not visible to library users. */

#ifndef SDL_R3D_INTERNAL_H_
#define SDL_R3D_INTERNAL_H_

#include <SDL3_r3d/SDL_r3d.h>

/* Kinds of queued primitives. */
typedef enum R3D_PrimitiveType
{
    R3D_PRIMITIVE_TRIANGLE,
    R3D_PRIMITIVE_LINE,
    R3D_PRIMITIVE_POINT
} R3D_PrimitiveType;

/*
 * The triangle is the universal primitive: EVERY filled shape (polygon,
 * cube, sphere, mesh, surface) is split into triangles before it gets here,
 * just like GPUs do. SDL_RenderGeometry() only draws triangles.
 *
 * Lines and points are queued too, so that everything can be sorted back to
 * front together in R3D_EndFrame() and hide each other correctly.
 */
typedef struct R3D_Primitive
{
    R3D_PrimitiveType type;
    SDL_Vertex vertices[3];   /* screen positions + colors (lines use 2, points 1) */
    float size;               /* point size in pixels (points only) */
    float depth;              /* average distance to the camera, for sorting */
    int order;                /* submission order, to keep the sort stable */
} R3D_Primitive;

/* A text label waiting to be drawn on top of the scene. */
typedef struct R3D_Label
{
    SDL_FPoint position;
    SDL_FColor color;
    char *text;               /* owned copy (SDL_strdup) */
} R3D_Label;

/* The real definition of the opaque R3D_Context type. */
struct R3D_Context
{
    SDL_Renderer *renderer;

    bool in_frame;                 /* between BeginFrame and EndFrame */
    R3D_Camera camera;
    R3D_Mat4 view;                 /* world -> camera */
    R3D_Mat4 projection;           /* camera -> clip */
    R3D_Mat4 view_projection;      /* projection * view (precomputed) */
    float viewport_w;              /* size of the area we project onto */
    float viewport_h;

    SDL_FColor color;              /* current color (R3D_SetDrawColor) */
    bool culling;                  /* back-face culling on/off */
    bool lighting;                 /* directional lighting on/off */
    R3D_Vec3 light_dir;            /* normalized direction the light travels */
    float ambient;                 /* minimum brightness, 0..1 */

    /* Queue of primitives for the current frame (grows with SDL_realloc). */
    R3D_Primitive *primitives;
    int num_primitives;
    int max_primitives;

    /* Text labels for the current frame. */
    R3D_Label *labels;
    int num_labels;
    int max_labels;

    /* Scratch buffer used to batch triangles into one SDL_RenderGeometry call. */
    SDL_Vertex *batch;
    int max_batch;

    R3D_Stats stats;               /* counters being filled for this frame */
    R3D_Stats last_stats;          /* counters of the last finished frame */
};

/* The real definition of the opaque R3D_Mesh type: any shape. */
struct R3D_Mesh
{
    R3D_Vertex *vertices;
    int num_vertices;

    int *indices;          /* 3 per triangle; never NULL (0, 1, 2, ... if none were given) */
    int num_indices;
};

/* Shared between source files (not exported). */

/* Checks that ctx is valid and inside a frame; sets the SDL error if not. */
bool R3D_CheckFrame(R3D_Context *ctx);

/* Queues a world-space triangle with one color per corner (lit and culled
   according to the context settings, clipped against the near plane). */
bool R3D_QueueTriangle(R3D_Context *ctx, const R3D_Vec3 pos[3], const SDL_FColor color[3]);

#endif /* SDL_R3D_INTERNAL_H_ */
