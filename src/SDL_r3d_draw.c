/*
  SDL3_r3d: a small software 3D layer on top of SDL_Renderer.
  Copyright (C) 2026 TanoDev0

  Distributed under the zlib license. See LICENSE.txt.
*/

/* Projection, clipping, lighting and queuing of primitives.

   Pipeline for every primitive:
     world --(view_projection)--> clip space (x, y, z, w)
           --(clip against the near plane: z + w >= 0)-->
           --(divide by w, map to pixels)--> screen
   The depth used for sorting is w, the distance in front of the camera. */

#include "SDL_r3d_internal.h"

/* Max corners after clipping a triangle against one plane. */
#define R3D_MAX_CLIPPED 4

/* A vertex in clip space with its color, used while clipping. */
typedef struct ClipVertex
{
    R3D_Vec4 pos;
    SDL_FColor color;
} ClipVertex;

/* ----- Helpers ----------------------------------------------------------- */

/* Transforms a world point to clip space. */
static R3D_Vec4 to_clip(const R3D_Context *ctx, R3D_Vec3 p)
{
    return R3D_Mat4MultiplyVec4(ctx->view_projection, (R3D_Vec4){ p.x, p.y, p.z, 1.0f });
}

/* Signed distance to the near plane: >= 0 means in front of it (visible). */
static float near_distance(R3D_Vec4 c)
{
    return c.z + c.w;
}

/* Converts a clip-space point (in front of the near plane) to screen pixels. */
static SDL_FPoint clip_to_screen(const R3D_Context *ctx, R3D_Vec4 c)
{
    float inv_w = 1.0f / c.w;
    float ndc_x = c.x * inv_w;
    float ndc_y = c.y * inv_w;
    return (SDL_FPoint){
        (ndc_x * 0.5f + 0.5f) * ctx->viewport_w,
        (0.5f - ndc_y * 0.5f) * ctx->viewport_h   /* screen y grows downwards */
    };
}

/* Linear interpolation between two clip-space vertices. */
static ClipVertex lerp_clip(ClipVertex a, ClipVertex b, float t)
{
    ClipVertex r;
    r.pos.x = a.pos.x + (b.pos.x - a.pos.x) * t;
    r.pos.y = a.pos.y + (b.pos.y - a.pos.y) * t;
    r.pos.z = a.pos.z + (b.pos.z - a.pos.z) * t;
    r.pos.w = a.pos.w + (b.pos.w - a.pos.w) * t;
    r.color.r = a.color.r + (b.color.r - a.color.r) * t;
    r.color.g = a.color.g + (b.color.g - a.color.g) * t;
    r.color.b = a.color.b + (b.color.b - a.color.b) * t;
    r.color.a = a.color.a + (b.color.a - a.color.a) * t;
    return r;
}

/* Reserves one more slot in the primitive queue. Returns NULL on failure. */
static R3D_Primitive *push_primitive(R3D_Context *ctx, R3D_PrimitiveType type)
{
    if (ctx->num_primitives == ctx->max_primitives) {
        int new_max = ctx->max_primitives ? ctx->max_primitives * 2 : 256;
        R3D_Primitive *grown = (R3D_Primitive *)SDL_realloc(ctx->primitives, (size_t)new_max * sizeof(R3D_Primitive));
        if (!grown) {
            return NULL;
        }
        ctx->primitives = grown;
        ctx->max_primitives = new_max;
    }
    R3D_Primitive *p = &ctx->primitives[ctx->num_primitives];
    SDL_zerop(p);
    p->type = type;
    p->order = ctx->num_primitives;
    ctx->num_primitives++;
    return p;
}

/* Builds an SDL_Vertex at a screen position with a color. */
static SDL_Vertex make_vertex(SDL_FPoint pos, SDL_FColor color)
{
    SDL_Vertex v;
    v.position = pos;
    v.color = color;
    v.tex_coord = (SDL_FPoint){ 0.0f, 0.0f };
    return v;
}

/* Multiplies the RGB of a color by a brightness factor. */
static SDL_FColor shade(SDL_FColor c, float brightness)
{
    return (SDL_FColor){ c.r * brightness, c.g * brightness, c.b * brightness, c.a };
}

/* ----- Public: projection ------------------------------------------------ */

/* Converts a 3D world point into a 2D renderer position.
   Returns false if the point is behind the camera. */
bool R3D_ProjectPoint(R3D_Context *ctx, R3D_Vec3 point, SDL_FPoint *out)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (!out) {
        return SDL_InvalidParamError("out");
    }
    R3D_Vec4 c = to_clip(ctx, point);
    if (near_distance(c) < 0.0f || c.w <= 0.0f) {
        return false;
    }
    *out = clip_to_screen(ctx, c);
    return true;
}

/* Returns the world-space ray through a screen position (for mouse picking). */
bool R3D_ScreenToRay(R3D_Context *ctx, float x, float y, R3D_Vec3 *origin, R3D_Vec3 *direction)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (!origin) {
        return SDL_InvalidParamError("origin");
    }
    if (!direction) {
        return SDL_InvalidParamError("direction");
    }

    R3D_Mat4 inverse;
    if (!R3D_Mat4Invert(ctx->view_projection, &inverse)) {
        return false;
    }

    /* Screen -> normalized device coordinates (-1..1, y up). */
    float ndc_x = (x / ctx->viewport_w) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (y / ctx->viewport_h) * 2.0f;

    /* The same screen point on the near plane (z = -1) and far plane (z = 1). */
    R3D_Vec4 n = R3D_Mat4MultiplyVec4(inverse, (R3D_Vec4){ ndc_x, ndc_y, -1.0f, 1.0f });
    R3D_Vec4 f = R3D_Mat4MultiplyVec4(inverse, (R3D_Vec4){ ndc_x, ndc_y, 1.0f, 1.0f });
    if (n.w == 0.0f || f.w == 0.0f) {
        return SDL_SetError("R3D_ScreenToRay: degenerate projection");
    }
    R3D_Vec3 near_point = { n.x / n.w, n.y / n.w, n.z / n.w };
    R3D_Vec3 far_point  = { f.x / f.w, f.y / f.w, f.z / f.w };

    *origin = near_point;
    *direction = R3D_Vec3Normalize(R3D_Vec3Sub(far_point, near_point));
    return true;
}

/* ----- Public: primitives ------------------------------------------------ */

/* Draws a 3D line, cutting off the part that is behind the camera. */
bool R3D_DrawLine3D(R3D_Context *ctx, R3D_Vec3 a, R3D_Vec3 b)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }

    ClipVertex va = { to_clip(ctx, a), ctx->color };
    ClipVertex vb = { to_clip(ctx, b), ctx->color };
    float da = near_distance(va.pos);
    float db = near_distance(vb.pos);

    if (da < 0.0f && db < 0.0f) {
        ctx->stats.clipped_primitives++;
        return true;   /* fully behind the camera: nothing to draw */
    }
    if (da < 0.0f) {
        va = lerp_clip(va, vb, da / (da - db));
    } else if (db < 0.0f) {
        vb = lerp_clip(vb, va, db / (db - da));
    }

    R3D_Primitive *p = push_primitive(ctx, R3D_PRIMITIVE_LINE);
    if (!p) {
        return false;
    }
    p->vertices[0] = make_vertex(clip_to_screen(ctx, va.pos), ctx->color);
    p->vertices[1] = make_vertex(clip_to_screen(ctx, vb.pos), ctx->color);
    p->depth = (va.pos.w + vb.pos.w) * 0.5f;
    return true;
}

/* Draws a 3D point as a square of `size` pixels. */
bool R3D_DrawPoint3D(R3D_Context *ctx, R3D_Vec3 point, float size)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (size <= 0.0f) {
        return SDL_InvalidParamError("size");
    }

    R3D_Vec4 c = to_clip(ctx, point);
    if (near_distance(c) < 0.0f || c.w <= 0.0f) {
        ctx->stats.clipped_primitives++;
        return true;
    }

    R3D_Primitive *p = push_primitive(ctx, R3D_PRIMITIVE_POINT);
    if (!p) {
        return false;
    }
    p->vertices[0] = make_vertex(clip_to_screen(ctx, c), ctx->color);
    p->size = size;
    p->depth = c.w;
    return true;
}

/* Queues a world-space triangle: lights it, clips it against the near plane,
   culls it if it faces away, and splits the clipped polygon into triangles. */
bool R3D_QueueTriangle(R3D_Context *ctx, const R3D_Vec3 pos[3], const SDL_FColor color[3])
{
    SDL_FColor colors[3] = { color[0], color[1], color[2] };

    /* Lighting: one brightness for the whole triangle (flat shading). */
    if (ctx->lighting) {
        R3D_Vec3 normal = R3D_Vec3Normalize(R3D_Vec3Cross(R3D_Vec3Sub(pos[1], pos[0]),
                                                          R3D_Vec3Sub(pos[2], pos[0])));
        float facing = -R3D_Vec3Dot(normal, ctx->light_dir);
        if (!ctx->culling) {
            facing = SDL_fabsf(facing);   /* both sides of open surfaces get light */
        }
        float brightness = ctx->ambient + (1.0f - ctx->ambient) * SDL_max(facing, 0.0f);
        for (int i = 0; i < 3; i++) {
            colors[i] = shade(colors[i], brightness);
        }
    }

    /* Clip against the near plane (Sutherland-Hodgman with one plane). */
    ClipVertex in[3];
    for (int i = 0; i < 3; i++) {
        in[i].pos = to_clip(ctx, pos[i]);
        in[i].color = colors[i];
    }

    ClipVertex out[R3D_MAX_CLIPPED];
    int count = 0;
    for (int i = 0; i < 3; i++) {
        ClipVertex cur = in[i];
        ClipVertex next = in[(i + 1) % 3];
        float dc = near_distance(cur.pos);
        float dn = near_distance(next.pos);
        if (dc >= 0.0f) {
            out[count++] = cur;
        }
        if ((dc >= 0.0f) != (dn >= 0.0f)) {
            out[count++] = lerp_clip(cur, next, dc / (dc - dn));
        }
    }
    if (count < 3) {
        ctx->stats.clipped_primitives++;
        return true;
    }

    SDL_FPoint screen[R3D_MAX_CLIPPED];
    for (int i = 0; i < count; i++) {
        screen[i] = clip_to_screen(ctx, out[i].pos);
    }

    /* Back-face culling. Front faces are counter-clockwise in normalized
       device coordinates (y up); with screen y pointing down they become
       clockwise, which gives a negative signed area here. */
    if (ctx->culling) {
        float area = (screen[1].x - screen[0].x) * (screen[2].y - screen[0].y) -
                     (screen[1].y - screen[0].y) * (screen[2].x - screen[0].x);
        if (area >= 0.0f) {
            ctx->stats.culled_triangles++;
            return true;
        }
    }

    /* The clipped polygon is convex: split it as a fan from corner 0. */
    for (int i = 1; i + 1 < count; i++) {
        R3D_Primitive *p = push_primitive(ctx, R3D_PRIMITIVE_TRIANGLE);
        if (!p) {
            return false;
        }
        p->vertices[0] = make_vertex(screen[0], out[0].color);
        p->vertices[1] = make_vertex(screen[i], out[i].color);
        p->vertices[2] = make_vertex(screen[i + 1], out[i + 1].color);
        p->depth = (out[0].pos.w + out[i].pos.w + out[i + 1].pos.w) / 3.0f;
    }
    return true;
}

/* Draws a filled 3D triangle with the current color. */
bool R3D_DrawTriangle3D(R3D_Context *ctx, R3D_Vec3 a, R3D_Vec3 b, R3D_Vec3 c)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    const R3D_Vec3 pos[3] = { a, b, c };
    const SDL_FColor colors[3] = { ctx->color, ctx->color, ctx->color };
    return R3D_QueueTriangle(ctx, pos, colors);
}

/* Draws a filled convex polygon by splitting it into triangles. */
bool R3D_DrawPolygon3D(R3D_Context *ctx, const R3D_Vec3 *points, int num_points)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (!points) {
        return SDL_InvalidParamError("points");
    }
    if (num_points < 3) {
        return SDL_InvalidParamError("num_points");
    }

    const SDL_FColor colors[3] = { ctx->color, ctx->color, ctx->color };
    for (int i = 1; i + 1 < num_points; i++) {
        const R3D_Vec3 pos[3] = { points[0], points[i], points[i + 1] };
        if (!R3D_QueueTriangle(ctx, pos, colors)) {
            return false;
        }
    }
    return true;
}

/* Draws a text label at a 3D position (on top of the scene). */
bool R3D_DrawText3D(R3D_Context *ctx, R3D_Vec3 point, const char *text)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (!text) {
        return SDL_InvalidParamError("text");
    }

    R3D_Vec4 c = to_clip(ctx, point);
    if (near_distance(c) < 0.0f || c.w <= 0.0f) {
        return true;   /* behind the camera: not visible */
    }

    if (ctx->num_labels == ctx->max_labels) {
        int new_max = ctx->max_labels ? ctx->max_labels * 2 : 16;
        R3D_Label *grown = (R3D_Label *)SDL_realloc(ctx->labels, (size_t)new_max * sizeof(R3D_Label));
        if (!grown) {
            return false;
        }
        ctx->labels = grown;
        ctx->max_labels = new_max;
    }

    char *copy = SDL_strdup(text);
    if (!copy) {
        return false;
    }
    R3D_Label *label = &ctx->labels[ctx->num_labels++];
    label->position = clip_to_screen(ctx, c);
    label->color = ctx->color;
    label->text = copy;
    return true;
}

/* ----- Public: helpers built on the primitives ---------------------------- */

/* Draws a square grid on the plane z = 0, centered at the origin. */
bool R3D_DrawGrid(R3D_Context *ctx, int slices, float spacing)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (slices < 1) {
        return SDL_InvalidParamError("slices");
    }
    if (spacing <= 0.0f) {
        return SDL_InvalidParamError("spacing");
    }

    /* Each line is drawn one cell at a time, so every short piece is depth
       sorted on its own and the grid hides correctly behind other objects. */
    float half = slices * spacing * 0.5f;
    for (int i = 0; i <= slices; i++) {
        float v = -half + i * spacing;
        for (int k = 0; k < slices; k++) {
            float a = -half + k * spacing;
            float b = a + spacing;
            if (!R3D_DrawLine3D(ctx, (R3D_Vec3){ v, a, 0.0f }, (R3D_Vec3){ v, b, 0.0f }) ||
                !R3D_DrawLine3D(ctx, (R3D_Vec3){ a, v, 0.0f }, (R3D_Vec3){ b, v, 0.0f })) {
                return false;
            }
        }
    }
    return true;
}

/* Draws the X (red), Y (green) and Z (blue) axes from the origin. */
bool R3D_DrawAxes(R3D_Context *ctx, float length)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }

    const R3D_Vec3 origin = { 0.0f, 0.0f, 0.0f };
    SDL_FColor saved = ctx->color;
    bool ok = true;

    ctx->color = (SDL_FColor){ 0.86f, 0.15f, 0.15f, 1.0f };
    ok = R3D_DrawLine3D(ctx, origin, (R3D_Vec3){ length, 0.0f, 0.0f }) && ok;
    ctx->color = (SDL_FColor){ 0.15f, 0.65f, 0.15f, 1.0f };
    ok = R3D_DrawLine3D(ctx, origin, (R3D_Vec3){ 0.0f, length, 0.0f }) && ok;
    ctx->color = (SDL_FColor){ 0.15f, 0.30f, 0.90f, 1.0f };
    ok = R3D_DrawLine3D(ctx, origin, (R3D_Vec3){ 0.0f, 0.0f, length }) && ok;

    ctx->color = saved;
    return ok;
}

/* Draws the surface z = func(x, y) over a rectangle, filled or as a wireframe.
   Samples where func returns NaN or infinity are skipped. */
bool R3D_DrawSurface(R3D_Context *ctx,
                     R3D_SurfaceFunction func, void *userdata,
                     float x_min, float x_max,
                     float y_min, float y_max,
                     int resolution, bool filled)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (!func) {
        return SDL_InvalidParamError("func");
    }
    if (resolution < 2) {
        return SDL_InvalidParamError("resolution");
    }

    int n = resolution;
    R3D_Vec3 *samples = (R3D_Vec3 *)SDL_malloc((size_t)n * (size_t)n * sizeof(R3D_Vec3));
    bool *valid = (bool *)SDL_malloc((size_t)n * (size_t)n * sizeof(bool));
    if (!samples || !valid) {
        SDL_free(samples);
        SDL_free(valid);
        return false;
    }

    /* Sample the function on an n x n grid. Index = j * n + i. */
    for (int j = 0; j < n; j++) {
        float y = y_min + (y_max - y_min) * (float)j / (float)(n - 1);
        for (int i = 0; i < n; i++) {
            float x = x_min + (x_max - x_min) * (float)i / (float)(n - 1);
            float z = func(userdata, x, y);
            samples[j * n + i] = (R3D_Vec3){ x, y, z };
            valid[j * n + i] = SDL_isinf(z) == 0 && SDL_isnan(z) == 0;
        }
    }

    bool ok = true;
    const SDL_FColor colors[3] = { ctx->color, ctx->color, ctx->color };

    for (int j = 0; ok && j < n; j++) {
        for (int i = 0; ok && i < n; i++) {
            int k = j * n + i;
            if (filled) {
                if (i + 1 >= n || j + 1 >= n) {
                    continue;
                }
                int k10 = k + 1, k01 = k + n, k11 = k + n + 1;
                if (!valid[k] || !valid[k10] || !valid[k01] || !valid[k11]) {
                    continue;   /* skip cells that touch an undefined sample */
                }
                /* Counter-clockwise seen from +Z. */
                const R3D_Vec3 t1[3] = { samples[k], samples[k10], samples[k11] };
                const R3D_Vec3 t2[3] = { samples[k], samples[k11], samples[k01] };
                ok = R3D_QueueTriangle(ctx, t1, colors) && R3D_QueueTriangle(ctx, t2, colors);
            } else {
                if (i + 1 < n && valid[k] && valid[k + 1]) {
                    ok = R3D_DrawLine3D(ctx, samples[k], samples[k + 1]);
                }
                if (ok && j + 1 < n && valid[k] && valid[k + n]) {
                    ok = R3D_DrawLine3D(ctx, samples[k], samples[k + n]);
                }
            }
        }
    }

    SDL_free(samples);
    SDL_free(valid);
    return ok;
}
