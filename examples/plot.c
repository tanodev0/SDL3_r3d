/*
  SDL3_r3d example: a 3D function plotter.

  What this example shows:
    - plotting several functions z = f(x, y) with R3D_DrawSurface
    - choosing the resolution (how many samples per side) at run time
    - functions with holes: when f returns NaN or infinity, R3D_DrawSurface
      skips that part instead of drawing a huge spike

  Controls:
    1 - 4             pick a function
    W                 toggle wireframe
    + / -             more / fewer samples
    left mouse drag   orbit
    mouse wheel       zoom
    Escape            quit
*/

#define SDL_MAIN_USE_CALLBACKS 1   /* SDL calls our 4 SDL_App* functions */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_r3d/SDL_r3d.h>

#include <math.h>   /* NAN */

#define WIDTH  900
#define HEIGHT 750
#define RANGE  8.0f   /* we plot x and y in [-RANGE, RANGE] */

/* One entry of the function menu. */
typedef struct Plot
{
    const char *formula;           /* shown on screen */
    R3D_SurfaceFunction func;      /* the function itself */
    SDL_Color color;               /* surface color */
} Plot;

/*
 * The functions. They all have the R3D_SurfaceFunction signature:
 *     float f(void *userdata, float x, float y)
 * and return the height z at (x, y).
 */

/* The "Mexican hat": a ripple that fades out from the center.
   sin(r)/r is 0/0 at the center; its limit there is 1, so we return 4 * 1. */
static float SDLCALL sombrero(void *userdata, float x, float y)
{
    (void)userdata;
    float r = SDL_sqrtf(x * x + y * y);   /* distance from the origin */
    return r < 1e-4f ? 4.0f : 4.0f * SDL_sinf(r) / r;
}

/* A saddle (hyperbolic paraboloid): up along X, down along Y. */
static float SDLCALL saddle(void *userdata, float x, float y)
{
    (void)userdata;
    return (x * x - y * y) / 12.0f;
}

/* Circular waves that shrink with distance (a decaying cosine). */
static float SDLCALL ripple(void *userdata, float x, float y)
{
    (void)userdata;
    float r2 = x * x + y * y;
    return 1.5f * SDL_cosf(SDL_sqrtf(r2) * 1.5f) * SDL_expf(-r2 / 60.0f);
}

/* 4 / (x y): infinite on both axes and very tall near them.
   Returning NAN tells R3D_DrawSurface "undefined here, leave a hole".
   We also cut the very tall parts so the plot stays readable. */
static float SDLCALL hyperbola(void *userdata, float x, float y)
{
    (void)userdata;
    float z = 4.0f / (x * y);
    return SDL_fabsf(z) > 6.0f ? NAN : z;
}

static const Plot PLOTS[] = {
    { "z = 4 sin(r) / r",               sombrero,  { 230, 120, 60, 255 } },
    { "z = (x^2 - y^2) / 12",           saddle,    { 60, 170, 120, 255 } },
    { "z = 1.5 cos(1.5 r) e^(-r^2/60)", ripple,    { 70, 120, 230, 255 } },
    { "z = 4 / (x y)",                  hyperbola, { 190, 80, 200, 255 } },
};
#define NUM_PLOTS ((int)SDL_arraysize(PLOTS))

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static R3D_Context  *r3d      = NULL;

static int current = 0;          /* index into PLOTS */
static int resolution = 48;      /* samples per side: 48 x 48 points */
static bool wireframe = false;
static bool dragging = false;

static R3D_Camera camera = {
    .position   = { 15.0f, -15.0f, 11.0f },
    .target     = { 0.0f, 0.0f, 0.0f },
    .up         = { 0.0f, 0.0f, 1.0f },   /* Z up: the height of the plot */
    .fovy       = 0.85f,
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
    if (!SDL_CreateWindowAndRenderer("SDL3_r3d - plot", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    r3d = R3D_CreateContext(renderer);
    if (!r3d) {
        SDL_Log("R3D_CreateContext: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Lighting makes the shape of the surface readable. Culling stays OFF
       (the default): a surface is open, and we want to see both its top and
       its underside. With culling off, both sides also receive light. */
    R3D_SetLighting(r3d, true, (R3D_Vec3){ -1.0f, 0.4f, -1.5f }, 0.35f);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        switch (event->key.key) {
        case SDLK_ESCAPE: return SDL_APP_SUCCESS;
        /* SDLK_1 .. SDLK_4 are consecutive codes, so key - SDLK_1 gives 0..3. */
        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
            current = (int)(event->key.key - SDLK_1);
            break;
        case SDLK_W: wireframe = !wireframe; break;
        /* '=' is the unshifted '+' key on most keyboards. */
        case SDLK_EQUALS: case SDLK_KP_PLUS:  resolution = SDL_min(resolution + 8, 120); break;
        case SDLK_MINUS:  case SDLK_KP_MINUS: resolution = SDL_max(resolution - 8, 8);   break;
        default: break;
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
    const Plot *plot = &PLOTS[current];

    SDL_SetRenderDrawColor(renderer, 250, 250, 248, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    R3D_BeginFrame(r3d, &camera);

    /* A light grid on the floor, 16 x 16 cells of 1 unit = [-8, 8]. */
    R3D_SetDrawColor(r3d, 215, 215, 215, 255);
    R3D_DrawGrid(r3d, 16, 1.0f);

    /* The surface. Arguments: function, userdata, x range, y range,
       samples per side, filled (true) or wireframe (false). */
    R3D_SetDrawColor(r3d, plot->color.r, plot->color.g, plot->color.b, 255);
    R3D_DrawSurface(r3d, plot->func, NULL, -RANGE, RANGE, -RANGE, RANGE, resolution, !wireframe);

    /* Axes slightly longer than the plot, with lowercase labels. */
    R3D_DrawAxes(r3d, RANGE + 1.0f);
    R3D_SetDrawColor(r3d, 50, 50, 50, 255);
    R3D_DrawText3D(r3d, (R3D_Vec3){ RANGE + 1.5f, 0, 0 }, "x");
    R3D_DrawText3D(r3d, (R3D_Vec3){ 0, RANGE + 1.5f, 0 }, "y");
    R3D_DrawText3D(r3d, (R3D_Vec3){ 0, 0, RANGE + 1.5f }, "z");

    R3D_EndFrame(r3d);   /* sort and draw */

    /* 2D overlay: current formula and controls. */
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugTextFormat(renderer, 10, 10, "[%d] %s", current + 1, plot->formula);
    SDL_RenderDebugTextFormat(renderer, 10, 24, "[1-4] function  [W] wireframe  [+/-] resolution: %d", resolution);

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate; (void)result;
    R3D_DestroyContext(r3d);
}
