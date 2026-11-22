/*
  SDL3_r3d example: building your own meshes.

  What this example shows:
    - a mesh written by hand: a pyramid with 5 vertices and 6 triangles
    - a mesh generated in a loop: a torus (a donut)
    - one color per vertex: the torus gets a rainbow gradient
    - why triangle order (winding) matters for culling and lighting

  How a mesh works:
    - a list of VERTICES: each one is a position and a color
    - a list of INDICES: every 3 indices pick 3 vertices that form a triangle
    A vertex shared by several triangles is stored once and used many times.

  Winding:
    Look at a triangle from the outside of the shape. If its 3 indices go
    around COUNTER-CLOCKWISE, that side is its "front". Back-face culling
    hides triangles whose back faces the camera, and lighting uses the front
    to decide which way the triangle points. Wrong order = invisible or dark
    triangles.

  Controls:
    left mouse drag   orbit
    mouse wheel       zoom
    W                 toggle wireframe
    Escape            quit
*/

#define SDL_MAIN_USE_CALLBACKS 1   /* SDL calls our 4 SDL_App* functions */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_r3d/SDL_r3d.h>

#define WIDTH  900
#define HEIGHT 700

#define TORUS_RING 40   /* steps around the big circle of the torus */
#define TORUS_TUBE 16   /* steps around the tube itself */

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static R3D_Context  *r3d      = NULL;
static R3D_Mesh     *pyramid  = NULL;
static R3D_Mesh     *torus    = NULL;
static bool dragging = false;
static bool wireframe = false;

static R3D_Camera camera = {
    .position   = { 7.0f, -9.0f, 6.0f },
    .target     = { 0.0f, 0.0f, 0.5f },
    .up         = { 0.0f, 0.0f, 1.0f },
    .fovy       = 0.8f,
    .near_plane = 0.1f,
    .far_plane  = 100.0f,
};

/* Converts a hue in [0, 1) (position on the color wheel) into a bright RGB
   color: 0 = red, 1/3 = green, 2/3 = blue, back to red at 1. */
static SDL_FColor hue_to_color(float h)
{
    float r = SDL_fabsf(h * 6.0f - 3.0f) - 1.0f;
    float g = 2.0f - SDL_fabsf(h * 6.0f - 2.0f);
    float b = 2.0f - SDL_fabsf(h * 6.0f - 4.0f);
    return (SDL_FColor){ SDL_clamp(r, 0.0f, 1.0f), SDL_clamp(g, 0.0f, 1.0f), SDL_clamp(b, 0.0f, 1.0f), 1.0f };
}

/*
 * A square pyramid, written out vertex by vertex.
 *
 *            4 (apex, on top)
 *           /|\
 *          / | \
 *     3 ------------ 2        base on the plane z = 0
 *       |  /   \  |
 *     0 ------------ 1
 */
static R3D_Mesh *create_pyramid(void)
{
    const SDL_FColor stone = { 0.85f, 0.75f, 0.50f, 1.0f };
    const R3D_Vertex vertices[5] = {
        { { -1.0f, -1.0f, 0.0f }, stone },   /* 0: base, front left  */
        { {  1.0f, -1.0f, 0.0f }, stone },   /* 1: base, front right */
        { {  1.0f,  1.0f, 0.0f }, stone },   /* 2: base, back right  */
        { { -1.0f,  1.0f, 0.0f }, stone },   /* 3: base, back left   */
        { {  0.0f,  0.0f, 1.6f }, stone },   /* 4: apex              */
    };

    /* 6 triangles, counter-clockwise seen from OUTSIDE the pyramid. */
    const int indices[18] = {
        0, 1, 4,     /* front side */
        1, 2, 4,     /* right side */
        2, 3, 4,     /* back side  */
        3, 0, 4,     /* left side  */
        0, 2, 1,     /* base, first half  (seen from below, so the order flips) */
        0, 3, 2,     /* base, second half */
    };

    /* R3D_CreateMesh COPIES the arrays, so they can live on the stack. */
    return R3D_CreateMesh(vertices, 5, indices, 18);
}

/*
 * A torus around the Z axis, generated with two angles:
 *   u goes around the big circle (the ring),
 *   v goes around the small circle (the tube).
 * Every (u, v) pair is one vertex; neighbouring vertices form small quads,
 * and each quad is split into 2 triangles.
 */
static R3D_Mesh *create_torus(float major_radius, float minor_radius)
{
    /* +1 on each side: the last row/column repeats the first one, so the
       seam closes and the rainbow can wrap from red back to red. */
    R3D_Vertex vertices[(TORUS_RING + 1) * (TORUS_TUBE + 1)];
    int indices[TORUS_RING * TORUS_TUBE * 6];
    int nv = 0, ni = 0;

    for (int i = 0; i <= TORUS_RING; i++) {
        float u = 2.0f * SDL_PI_F * (float)i / TORUS_RING;
        SDL_FColor color = hue_to_color((float)i / TORUS_RING);   /* color changes around the ring */
        for (int j = 0; j <= TORUS_TUBE; j++) {
            float v = 2.0f * SDL_PI_F * (float)j / TORUS_TUBE;
            /* Distance from the Z axis: the ring radius plus the part of the
               tube that points outwards at this angle v. */
            float ring = major_radius + minor_radius * SDL_cosf(v);
            vertices[nv++] = (R3D_Vertex){
                { ring * SDL_cosf(u), ring * SDL_sinf(u), minor_radius * SDL_sinf(v) },
                color
            };
        }
    }

    /* Vertices are stored row by row: vertex (i, j) is at index i * cols + j. */
    const int cols = TORUS_TUBE + 1;
    for (int i = 0; i < TORUS_RING; i++) {
        for (int j = 0; j < TORUS_TUBE; j++) {
            int a = i * cols + j;        /* (u,     v)     */
            int b = (i + 1) * cols + j;  /* (u + 1, v)     */
            /* b + 1 = (u + 1, v + 1) and a + 1 = (u, v + 1).
               These two triangles are counter-clockwise from outside. */
            indices[ni++] = a; indices[ni++] = b;     indices[ni++] = b + 1;
            indices[ni++] = a; indices[ni++] = b + 1; indices[ni++] = a + 1;
        }
    }
    return R3D_CreateMesh(vertices, nv, indices, ni);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)appstate; (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("SDL3_r3d - custom meshes", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    r3d = R3D_CreateContext(renderer);
    pyramid = create_pyramid();
    torus = create_torus(1.6f, 0.45f);
    if (!r3d || !pyramid || !torus) {
        /* R3D_CreateMesh explains what was wrong (e.g. an index out of range). */
        SDL_Log("SDL3_r3d setup failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Both shapes are closed, so culling is safe. If you get the winding
       wrong, culling makes the mistake obvious: those triangles disappear. */
    R3D_SetCulling(r3d, true);
    R3D_SetLighting(r3d, true, (R3D_Vec3){ -0.8f, 0.6f, -1.5f }, 0.3f);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) return SDL_APP_SUCCESS;
        if (event->key.key == SDLK_W) wireframe = !wireframe;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) dragging = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT) dragging = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (dragging) {
            R3D_OrbitCamera(&camera, -event->motion.xrel * 0.01f, event->motion.yrel * 0.01f, 1.0f);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        R3D_OrbitCamera(&camera, 0.0f, 0.0f, event->wheel.y > 0 ? 0.9f : 1.1f);
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void)appstate;
    float t = (float)SDL_GetTicks() / 1000.0f;

    SDL_SetRenderDrawColor(renderer, 24, 26, 32, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    R3D_BeginFrame(r3d, &camera);
    R3D_SetDrawColor(r3d, 60, 64, 72, 255);
    R3D_DrawGrid(r3d, 12, 1.0f);

    /* Pyramid: spin around its own vertical axis, then move it into place. */
    R3D_Mat4 pyramid_model = R3D_Mat4Multiply(R3D_Mat4Translate((R3D_Vec3){ -2.2f, 1.2f, 0.0f }),
                                              R3D_Mat4RotateZ(t * 0.5f));
    /* Torus: tumble around a tilted axis, then lift it above the floor. */
    R3D_Mat4 torus_model = R3D_Mat4Multiply(R3D_Mat4Translate((R3D_Vec3){ 1.8f, -0.8f, 1.4f }),
                                            R3D_Mat4Rotate((R3D_Vec3){ 1.0f, 0.3f, 0.0f }, t * 0.7f));

    /* In wireframe mode you can see how each shape is cut into triangles. */
    R3D_SetDrawColor(r3d, 230, 230, 230, 255);
    if (wireframe) {
        R3D_DrawMeshWireframe(r3d, pyramid, &pyramid_model);
        R3D_DrawMeshWireframe(r3d, torus, &torus_model);
    } else {
        R3D_DrawMesh(r3d, pyramid, &pyramid_model);
        R3D_DrawMesh(r3d, torus, &torus_model);
    }

    R3D_EndFrame(r3d);   /* sort and draw */

    SDL_SetRenderDrawColor(renderer, 210, 210, 210, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, 10, 10, "Custom meshes: hand-written pyramid, generated torus  [W] wireframe");

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate; (void)result;
    R3D_DestroyMesh(torus);
    R3D_DestroyMesh(pyramid);
    R3D_DestroyContext(r3d);
}
