/*
  SDL3_r3d example: one mesh, many objects.

  What this example shows:
    - drawing the SAME mesh 121 times, each time with a different matrix
      (no need to create 121 meshes)
    - animating position and scale every frame
    - measuring the cost of the 3D work and reading R3D_GetStats
    - what back-face culling saves: press C and watch the counters

  Controls:
    left mouse drag   orbit
    mouse wheel       zoom
    C                 toggle back-face culling
    Escape            quit
*/

#define SDL_MAIN_USE_CALLBACKS 1   /* SDL calls our 4 SDL_App* functions */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_r3d/SDL_r3d.h>

#define WIDTH  900
#define HEIGHT 700
#define SIDE   11      /* cubes per row and per column: 11 x 11 = 121 */
#define GAP    1.3f    /* distance between cube centers */

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static R3D_Context  *r3d      = NULL;
static R3D_Mesh     *cube     = NULL;   /* the only mesh in this example */
static bool dragging = false;
static bool culling = true;
static float frame_ms = 0.0f;           /* smoothed time spent on the 3D part */

static R3D_Camera camera = {
    .position   = { 14.0f, -14.0f, 12.0f },
    .target     = { 0.0f, 0.0f, 0.0f },
    .up         = { 0.0f, 0.0f, 1.0f },
    .fovy       = 0.9f,
    .near_plane = 0.1f,
    .far_plane  = 200.0f,
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)appstate; (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("SDL3_r3d - cubes", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    r3d = R3D_CreateContext(renderer);
    /* A unit cube (side 1) centered at the origin. We scale and move it
       with matrices when drawing, so one mesh covers every cube. */
    cube = R3D_CreateCubeMesh(1.0f, (SDL_FColor){ 0.35f, 0.75f, 0.70f, 1.0f });
    if (!r3d || !cube) {
        SDL_Log("SDL3_r3d setup failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    R3D_SetCulling(r3d, culling);
    R3D_SetLighting(r3d, true, (R3D_Vec3){ -0.6f, 0.8f, -1.4f }, 0.3f);
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
        if (event->key.key == SDLK_C) {
            culling = !culling;
            R3D_SetCulling(r3d, culling);
        }
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
    Uint64 start = SDL_GetTicksNS();               /* to measure the 3D work */
    float t = (float)SDL_GetTicks() / 1000.0f;     /* seconds, for the animation */

    SDL_SetRenderDrawColor(renderer, 236, 238, 242, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    R3D_BeginFrame(r3d, &camera);

    /* Center the block of cubes on the origin. */
    float offset = (SIDE - 1) * GAP * 0.5f;

    for (int j = 0; j < SIDE; j++) {
        for (int i = 0; i < SIDE; i++) {
            float x = i * GAP - offset;
            float y = j * GAP - offset;

            /* A wave that travels outwards from the center: the phase depends
               on the distance to the origin and moves with time. */
            float wave = SDL_sinf(t * 2.0f - SDL_sqrtf(x * x + y * y) * 0.6f);
            float height = 1.0f + wave * 0.8f;   /* between 0.2 and 1.8 */

            /* Right to left:
                 1. Scale:     make the unit cube 0.9 wide and `height` tall
                               (it is centered, so it grows up AND down).
                 2. Translate: move it to (x, y) and lift it by half its
                               height so its bottom rests on z = 0. */
            R3D_Mat4 model = R3D_Mat4Multiply(R3D_Mat4Translate((R3D_Vec3){ x, y, height * 0.5f }),
                                              R3D_Mat4Scale((R3D_Vec3){ 0.9f, 0.9f, height }));
            R3D_DrawMesh(r3d, cube, &model);   /* same mesh, new matrix */
        }
    }

    R3D_EndFrame(r3d);   /* sort 121 x 12 triangles and draw them */

    /* Exponential smoothing: 90% old value + 10% new value, so the number on
       screen is readable instead of jumping every frame. */
    frame_ms = frame_ms * 0.9f + (float)(SDL_GetTicksNS() - start) / 1e6f * 0.1f;

    /* Stats of the frame we just finished: with culling on, about half of
       the triangles are skipped (the hidden sides of every cube). */
    R3D_Stats stats;
    R3D_GetStats(r3d, &stats);
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugTextFormat(renderer, 10, 10, "%d cubes from 1 mesh   [C] culling: %s", SIDE * SIDE, culling ? "on" : "off");
    SDL_RenderDebugTextFormat(renderer, 10, 24, "%d triangles drawn, %d culled, 3D time %.2f ms",
                              stats.triangles, stats.culled_triangles, frame_ms);

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate; (void)result;
    R3D_DestroyMesh(cube);
    R3D_DestroyContext(r3d);
}
