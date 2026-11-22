/*
  SDL3_r3d example: grid, labelled axes and a function surface.

  What this example shows:
    - the minimum setup: window, renderer, SDL3_r3d context and a camera
    - plotting a surface z = f(x, y) with R3D_DrawSurface
    - helpers: R3D_DrawGrid, R3D_DrawAxes and R3D_DrawText3D labels
    - orbiting the camera with the mouse (R3D_OrbitCamera)
    - mixing 2D drawing (SDL_RenderDebugText) on top of the 3D scene

  Controls:
    left mouse drag   orbit around the scene
    mouse wheel       zoom in / out
    F                 switch between filled and wireframe surface
    Escape            quit
*/

/* SDL3 "main callbacks": instead of writing main() and our own loop, we give
   SDL four functions (SDL_AppInit, SDL_AppEvent, SDL_AppIterate,
   SDL_AppQuit) and SDL calls them for us. */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* The only header you need for SDL3_r3d. */
#include <SDL3_r3d/SDL_r3d.h>

/* Size of our drawing area. With logical presentation (see SDL_AppInit) we
   always draw as if the window were this size, even if the user resizes it. */
#define WIDTH  800
#define HEIGHT 800

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;   /* the plain SDL 2D renderer */
static R3D_Context  *r3d      = NULL;   /* SDL3_r3d state, tied to that renderer */

static bool dragging = false;           /* is the left mouse button held down? */
static bool filled = true;              /* filled surface or wireframe */

/* The camera: where it is, what it looks at and which way is "up".
   This example uses Z as the up axis, like math textbooks do for z = f(x, y). */
static R3D_Camera camera = {
    .position   = { 18.0f, -18.0f, 14.0f },   /* up and to the side of the origin */
    .target     = { 0.0f, 0.0f, 0.0f },       /* looking at the origin */
    .up         = { 0.0f, 0.0f, 1.0f },       /* Z up */
    .fovy       = 0.9f,                       /* vertical field of view, radians (~52 degrees) */
    .near_plane = 0.1f,                       /* nothing closer than this is drawn */
    .far_plane  = 200.0f,                     /* used for the projection math */
};

/*
 * The surface we plot: z = 3 sin(x/2) cos(y/2).
 *
 * R3D_DrawSurface calls this function once per sample point. `userdata` is
 * whatever pointer we pass to R3D_DrawSurface (NULL here); it lets you plot
 * functions that need extra parameters without global variables.
 */
static float SDLCALL wave(void *userdata, float x, float y)
{
    (void)userdata;   /* not used in this example */
    return 3.0f * SDL_sinf(x * 0.5f) * SDL_cosf(y * 0.5f);
}

/* Called once at startup. Create everything here. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)appstate; (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("SDL3_r3d - showaxes", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Draw in a fixed 800x800 space and let SDL scale it to the window
       (with black bars if the shape differs). SDL3_r3d detects this and
       projects onto the logical size automatically. */
    SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* Wait for the screen refresh instead of drawing thousands of frames per
       second and keeping the CPU at 100%. */
    SDL_SetRenderVSync(renderer, 1);

    /* One SDL3_r3d context per renderer. It holds the draw color, the light,
       the culling setting and the queue of things to draw each frame. */
    r3d = R3D_CreateContext(renderer);
    if (!r3d) {
        SDL_Log("R3D_CreateContext: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Turn on lighting so the surface shows its shape: triangles facing the
       light are bright, triangles facing away get only the ambient 0.35.
       The vector is the direction the light TRAVELS (here: down and sideways). */
    R3D_SetLighting(r3d, true, (R3D_Vec3){ -1.0f, 1.0f, -2.0f }, 0.35f);

    /* R3D_Version() returns a number built with SDL_VERSIONNUM; these SDL
       macros split it back into major.minor.micro. */
    SDL_Log("SDL3_r3d version %d.%d.%d",
            SDL_VERSIONNUM_MAJOR(R3D_Version()),
            SDL_VERSIONNUM_MINOR(R3D_Version()),
            SDL_VERSIONNUM_MICRO(R3D_Version()));
    return SDL_APP_CONTINUE;
}

/* Called once for every event (keyboard, mouse, window...). */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    switch (event->type) {
    case SDL_EVENT_QUIT:                     /* window closed */
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) return SDL_APP_SUCCESS;
        if (event->key.key == SDLK_F) filled = !filled;
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) dragging = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT) dragging = false;
        break;

    case SDL_EVENT_MOUSE_MOTION:
        /* xrel / yrel = how many pixels the mouse moved since the last event.
           Moving left/right spins the camera around the up axis (yaw);
           moving up/down tilts it (pitch). 0.01 radians per pixel feels good.
           R3D_OrbitCamera keeps the camera looking at camera.target. */
        if (dragging) {
            R3D_OrbitCamera(&camera, -event->motion.xrel * 0.01f, event->motion.yrel * 0.01f, 1.0f);
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        /* The last argument multiplies the distance to the target:
           0.9 = 10% closer, 1.1 = 10% farther. */
        R3D_OrbitCamera(&camera, 0.0f, 0.0f, event->wheel.y > 0 ? 0.9f : 1.1f);
        break;

    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

/* Called once per frame. Draw everything here. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void)appstate;

    /* 1. Clear the screen with normal SDL calls. */
    SDL_SetRenderDrawColor(renderer, 245, 245, 245, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    /* 2. Start a 3D frame. This computes the camera matrices for this frame.
          Every R3D_Draw* call must happen between BeginFrame and EndFrame. */
    R3D_BeginFrame(r3d, &camera);

    /* The floor grid: 20 x 20 cells of 1 unit, on the plane z = 0.
       R3D_SetDrawColor works like SDL_SetRenderDrawColor: it affects the
       calls that come after it. */
    R3D_SetDrawColor(r3d, 200, 200, 200, 255);
    R3D_DrawGrid(r3d, 20, 1.0f);

    /* The surface over x and y in [-10, 10], sampled 40 x 40 times.
       More samples = smoother surface but more triangles. */
    R3D_SetDrawColor(r3d, 120, 80, 210, 255);
    R3D_DrawSurface(r3d, wave, NULL, -10, 10, -10, 10, 40, filled);

    /* X (red), Y (green) and Z (blue) axes from the origin, 11 units long. */
    R3D_DrawAxes(r3d, 11.0f);

    /* Text labels placed at 3D positions. They follow the camera and are
       always drawn on top of the scene. */
    R3D_SetDrawColor(r3d, 40, 40, 40, 255);
    R3D_DrawText3D(r3d, (R3D_Vec3){ 11.5f, 0, 0 }, "X");
    R3D_DrawText3D(r3d, (R3D_Vec3){ 0, 11.5f, 0 }, "Y");
    R3D_DrawText3D(r3d, (R3D_Vec3){ 0, 0, 11.5f }, "Z");

    /* 3. Finish the 3D frame. Nothing above was drawn yet: SDL3_r3d queued
          it all. Now it sorts everything from far to near (so near things
          hide far things) and sends it to the renderer. */
    R3D_EndFrame(r3d);

    /* 4. Plain 2D drawing still works on top of the 3D scene. */
    R3D_Stats stats;
    R3D_GetStats(r3d, &stats);   /* counters of the frame we just finished */
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, 10, 10, "z = 3 sin(x/2) cos(y/2)   [drag] orbit  [wheel] zoom  [F] fill");
    SDL_RenderDebugTextFormat(renderer, 10, 24, "%d triangles, %d lines", stats.triangles, stats.lines);

    /* 5. Show the result. SDL3_r3d never calls this for you. */
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

/* Called once at exit. SDL destroys the window and renderer itself;
   we only free what we created with SDL3_r3d. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate; (void)result;
    R3D_DestroyContext(r3d);
}
