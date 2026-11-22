/*
  SDL3_r3d: a small software 3D layer on top of SDL_Renderer.
  Copyright (C) 2026 TanoDev0

  Distributed under the zlib license. See LICENSE.txt.
*/

/* Version, context and frame. */

#include "SDL_r3d_internal.h"

/* Returns the library version (see SDL_VERSIONNUM). */
int R3D_Version(void)
{
    return SDL_R3D_VERSION;
}

/* Creates a drawing context bound to an SDL renderer. Returns NULL on failure. */
R3D_Context *R3D_CreateContext(SDL_Renderer *renderer)
{
    if (!renderer) {
        SDL_InvalidParamError("renderer");
        return NULL;
    }

    R3D_Context *ctx = (R3D_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;   /* SDL_calloc already set the error */
    }

    ctx->renderer = renderer;
    ctx->color = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f };
    ctx->light_dir = R3D_Vec3Normalize((R3D_Vec3){ -1.0f, -1.0f, -2.0f });
    ctx->ambient = 0.3f;
    return ctx;
}

/* Frees the text copies of the current frame's labels. */
static void free_labels(R3D_Context *ctx)
{
    for (int i = 0; i < ctx->num_labels; i++) {
        SDL_free(ctx->labels[i].text);
    }
    ctx->num_labels = 0;
}

/* Frees a context and everything it owns. NULL is ignored. */
void R3D_DestroyContext(R3D_Context *ctx)
{
    if (!ctx) {
        return;
    }
    free_labels(ctx);
    SDL_free(ctx->labels);
    SDL_free(ctx->primitives);
    SDL_free(ctx->batch);
    SDL_free(ctx);
}

/* Checks that ctx is valid and inside a frame; sets the SDL error if not. */
bool R3D_CheckFrame(R3D_Context *ctx)
{
    if (!ctx) {
        return SDL_InvalidParamError("ctx");
    }
    if (!ctx->in_frame) {
        return SDL_SetError("R3D: not inside a frame (call R3D_BeginFrame first)");
    }
    return true;
}

/* Gets the size of the area we draw on: the logical size if logical
   presentation is on, otherwise the real output size. */
static bool get_viewport_size(SDL_Renderer *renderer, float *w, float *h)
{
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    if (SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, &mode) &&
        mode != SDL_LOGICAL_PRESENTATION_DISABLED && lw > 0 && lh > 0) {
        *w = (float)lw;
        *h = (float)lh;
        return true;
    }

    int ow = 0, oh = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &ow, &oh)) {
        return false;
    }
    if (ow <= 0 || oh <= 0) {
        return SDL_SetError("R3D: renderer output size is zero");
    }
    *w = (float)ow;
    *h = (float)oh;
    return true;
}

/* Starts a frame: stores the camera, builds the view/projection matrices
   and empties the queues. */
bool R3D_BeginFrame(R3D_Context *ctx, const R3D_Camera *camera)
{
    if (!ctx) {
        return SDL_InvalidParamError("ctx");
    }
    if (!camera) {
        return SDL_InvalidParamError("camera");
    }
    if (camera->fovy <= 0.0f || camera->fovy >= SDL_PI_F) {
        return SDL_SetError("R3D_BeginFrame: camera fovy must be between 0 and pi");
    }
    if (camera->near_plane <= 0.0f || camera->far_plane <= camera->near_plane) {
        return SDL_SetError("R3D_BeginFrame: camera needs 0 < near_plane < far_plane");
    }

    if (!get_viewport_size(ctx->renderer, &ctx->viewport_w, &ctx->viewport_h)) {
        return false;
    }

    ctx->camera = *camera;
    ctx->view = R3D_Mat4LookAt(camera->position, camera->target, camera->up);
    ctx->projection = R3D_Mat4Perspective(camera->fovy, ctx->viewport_w / ctx->viewport_h,
                                          camera->near_plane, camera->far_plane);
    ctx->view_projection = R3D_Mat4Multiply(ctx->projection, ctx->view);

    /* Start from empty queues (also if the previous frame was never ended). */
    ctx->num_primitives = 0;
    free_labels(ctx);
    SDL_zero(ctx->stats);

    ctx->in_frame = true;
    return true;
}

/* Sort order: farthest first; equal depths keep their submission order. */
static int SDLCALL compare_primitives(const void *pa, const void *pb)
{
    const R3D_Primitive *a = (const R3D_Primitive *)pa;
    const R3D_Primitive *b = (const R3D_Primitive *)pb;
    if (a->depth > b->depth) return -1;
    if (a->depth < b->depth) return 1;
    return (a->order < b->order) ? -1 : (a->order > b->order);
}

/* Draws the triangles collected in the batch with one SDL_RenderGeometry call. */
static bool flush_batch(R3D_Context *ctx, int *num_vertices)
{
    bool ok = true;
    if (*num_vertices > 0) {
        ok = SDL_RenderGeometry(ctx->renderer, NULL, ctx->batch, *num_vertices, NULL, 0);
        *num_vertices = 0;
    }
    return ok;
}

/* Ends a frame: sorts everything back to front and draws it, then the labels. */
bool R3D_EndFrame(R3D_Context *ctx)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }

    bool ok = true;
    SDL_Renderer *renderer = ctx->renderer;

    /* Leave the renderer's draw color as we found it. */
    float old_r, old_g, old_b, old_a;
    SDL_GetRenderDrawColorFloat(renderer, &old_r, &old_g, &old_b, &old_a);

    if (ctx->num_primitives > 1) {
        SDL_qsort(ctx->primitives, (size_t)ctx->num_primitives, sizeof(R3D_Primitive), compare_primitives);
    }

    /* Make sure the batch can hold every triangle at once. */
    int needed = ctx->num_primitives * 3;
    if (needed > ctx->max_batch) {
        SDL_Vertex *grown = (SDL_Vertex *)SDL_realloc(ctx->batch, (size_t)needed * sizeof(SDL_Vertex));
        if (!grown) {
            ok = false;
            needed = 0;   /* draw nothing rather than overflow */
        } else {
            ctx->batch = grown;
            ctx->max_batch = needed;
        }
    }

    int batched = 0;
    for (int i = 0; ok && i < ctx->num_primitives; i++) {
        const R3D_Primitive *p = &ctx->primitives[i];

        if (p->type == R3D_PRIMITIVE_TRIANGLE) {
            ctx->batch[batched++] = p->vertices[0];
            ctx->batch[batched++] = p->vertices[1];
            ctx->batch[batched++] = p->vertices[2];
            ctx->stats.triangles++;
            continue;
        }

        /* A line or point between triangles: draw the triangles so far first,
           so the back-to-front order is kept. */
        ok = flush_batch(ctx, &batched) && ok;

        const SDL_FColor *c = &p->vertices[0].color;
        SDL_SetRenderDrawColorFloat(renderer, c->r, c->g, c->b, c->a);
        if (p->type == R3D_PRIMITIVE_LINE) {
            ok = SDL_RenderLine(renderer, p->vertices[0].position.x, p->vertices[0].position.y,
                                p->vertices[1].position.x, p->vertices[1].position.y) && ok;
            ctx->stats.lines++;
        } else {
            SDL_FRect rect = {
                p->vertices[0].position.x - p->size * 0.5f,
                p->vertices[0].position.y - p->size * 0.5f,
                p->size, p->size
            };
            ok = SDL_RenderFillRect(renderer, &rect) && ok;
            ctx->stats.points++;
        }
    }
    ok = flush_batch(ctx, &batched) && ok;

    /* Labels go on top of everything. */
    for (int i = 0; i < ctx->num_labels; i++) {
        const R3D_Label *l = &ctx->labels[i];
        SDL_SetRenderDrawColorFloat(renderer, l->color.r, l->color.g, l->color.b, l->color.a);
        ok = SDL_RenderDebugText(renderer, l->position.x, l->position.y, l->text) && ok;
    }
    free_labels(ctx);

    SDL_SetRenderDrawColorFloat(renderer, old_r, old_g, old_b, old_a);

    ctx->last_stats = ctx->stats;
    ctx->num_primitives = 0;
    ctx->in_frame = false;
    return ok;
}

/* Copies the counters of the last finished frame into *stats. */
bool R3D_GetStats(R3D_Context *ctx, R3D_Stats *stats)
{
    if (!ctx) {
        return SDL_InvalidParamError("ctx");
    }
    if (!stats) {
        return SDL_InvalidParamError("stats");
    }
    *stats = ctx->last_stats;
    return true;
}

/* Sets the color used by the following draw calls. */
bool R3D_SetDrawColor(R3D_Context *ctx, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!ctx) {
        return SDL_InvalidParamError("ctx");
    }
    ctx->color = (SDL_FColor){ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
    return true;
}

/* Turns back-face culling on or off. */
bool R3D_SetCulling(R3D_Context *ctx, bool enabled)
{
    if (!ctx) {
        return SDL_InvalidParamError("ctx");
    }
    ctx->culling = enabled;
    return true;
}

/* Turns directional lighting on or off and sets the light. */
bool R3D_SetLighting(R3D_Context *ctx, bool enabled, R3D_Vec3 direction, float ambient)
{
    if (!ctx) {
        return SDL_InvalidParamError("ctx");
    }
    R3D_Vec3 dir = R3D_Vec3Normalize(direction);
    if (enabled && R3D_Vec3Length(dir) == 0.0f) {
        return SDL_SetError("R3D_SetLighting: light direction is zero");
    }
    ctx->lighting = enabled;
    if (enabled) {
        ctx->light_dir = dir;
    }
    ctx->ambient = SDL_clamp(ambient, 0.0f, 1.0f);
    return true;
}
