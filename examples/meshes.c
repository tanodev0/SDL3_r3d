/*
  SDL3_r3d example: lit meshes, back-face culling and mouse picking.

  What this example shows:
    - ready-made meshes: R3D_CreateCubeMesh, R3D_CreateSphereMesh,
      R3D_CreatePlaneMesh
    - placing, rotating and animating meshes with transform matrices
    - lighting (R3D_SetLighting) and back-face culling (R3D_SetCulling)
    - mouse picking: R3D_ScreenToRay turns the mouse position into a 3D ray,
      and we intersect it with the floor to place a marker
    - points (R3D_DrawPoint3D) and text labels (R3D_DrawText3D)

  Controls:
    left mouse drag   orbit
    mouse wheel       zoom
    C                 toggle back-face culling
    L                 toggle lighting
    W                 toggle wireframe
    Escape            quit
*/

#define SDL_MAIN_USE_CALLBACKS 1   /* SDL calls our 4 SDL_App* functions */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_r3d/SDL_r3d.h>

#define WIDTH  900
#define HEIGHT 700

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static R3D_Context  *r3d      = NULL;

/* Meshes are created once and drawn every frame. Creating a mesh copies its
   vertices, so it is cheap to draw many times but not to create every frame. */
static R3D_Mesh *cube       = NULL;
static R3D_Mesh *sphere     = NULL;
static R3D_Mesh *floor_mesh = NULL;

static bool dragging = false;
static bool culling = true;
static bool lighting = true;
static bool wireframe = false;
static float mouse_x = -1.0f, mouse_y = -1.0f;   /* -1 = mouse not seen yet */

/* Direction the light travels: mostly downwards, a bit sideways. */
static const R3D_Vec3 LIGHT = { -1.0f, 0.5f, -2.0f };

static R3D_Camera camera = {
    .position   = { 9.0f, -11.0f, 7.0f },
    .target     = { 0.0f, 0.0f, 1.0f },      /* look slightly above the floor */
    .up         = { 0.0f, 0.0f, 1.0f },      /* Z up */
    .fovy       = 0.8f,
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
    if (!SDL_CreateWindowAndRenderer("SDL3_r3d - meshes", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    r3d = R3D_CreateContext(renderer);

    /* Colors are SDL_FColor: red, green, blue, alpha from 0.0 to 1.0. */
    cube = R3D_CreateCubeMesh(2.0f, (SDL_FColor){ 0.95f, 0.45f, 0.20f, 1.0f });
    /* 16 rings x 24 segments: more = rounder sphere, but more triangles. */
    sphere = R3D_CreateSphereMesh(1.3f, 16, 24, (SDL_FColor){ 0.25f, 0.55f, 0.95f, 1.0f });
    /* 12 x 12 units, split into 12 x 12 cells. A floor made of ONE big
       rectangle would sort badly against the objects standing on it,
       because SDL3_r3d orders whole triangles by their average distance. */
    floor_mesh = R3D_CreatePlaneMesh(12.0f, 12.0f, 12, (SDL_FColor){ 0.80f, 0.82f, 0.78f, 1.0f });

    /* Every create function returns NULL on failure, like SDL does. */
    if (!r3d || !cube || !sphere || !floor_mesh) {
        SDL_Log("SDL3_r3d setup failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Culling skips triangles seen from behind (the far side of the cube and
       sphere). Half the work for closed objects, same picture. */
    R3D_SetCulling(r3d, culling);
    R3D_SetLighting(r3d, lighting, LIGHT, 0.3f);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    /* Mouse events come in window pixels. With logical presentation on, this
       converts them to our 900x700 logical space, which is the space that
       R3D_ScreenToRay expects. */
    SDL_ConvertEventToRenderCoordinates(renderer, event);

    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        switch (event->key.key) {
        case SDLK_ESCAPE: return SDL_APP_SUCCESS;
        case SDLK_C: culling = !culling;   R3D_SetCulling(r3d, culling); break;
        case SDLK_L: lighting = !lighting; R3D_SetLighting(r3d, lighting, LIGHT, 0.3f); break;
        case SDLK_W: wireframe = !wireframe; break;
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
        /* Remember where the mouse is, for picking in SDL_AppIterate. */
        mouse_x = event->motion.x;
        mouse_y = event->motion.y;
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

/* Draws a mesh filled or as a wireframe, depending on the current mode.
   `model` is the mesh's transform: where it is, how it is rotated and scaled.
   NULL means "draw it exactly as it was created". */
static void draw(R3D_Mesh *mesh, const R3D_Mat4 *model)
{
    if (wireframe) {
        R3D_DrawMeshWireframe(r3d, mesh, model);   /* edges, with the current draw color */
    } else {
        R3D_DrawMesh(r3d, mesh, model);            /* filled, with the mesh's own colors */
    }
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void)appstate;
    float t = (float)SDL_GetTicks() / 1000.0f;   /* seconds since start, for animation */

    SDL_SetRenderDrawColor(renderer, 28, 30, 36, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    R3D_BeginFrame(r3d, &camera);
    R3D_SetDrawColor(r3d, 90, 95, 105, 255);   /* used by the wireframe mode */

    /* The floor sits at the origin as created: no transform needed. */
    draw(floor_mesh, NULL);

    /* The cube: first rotate it around a tilted axis (it spins over time),
       then move it to (-2, 0, 1.5).
       R3D_Mat4Multiply(A, B) applies B first, then A, so read right to left:
       "rotate, then translate". Swapping them would make it orbit instead. */
    R3D_Mat4 cube_model = R3D_Mat4Multiply(R3D_Mat4Translate((R3D_Vec3){ -2.0f, 0.0f, 1.5f }),
                                           R3D_Mat4Rotate((R3D_Vec3){ 1.0f, 0.6f, 0.3f }, t));
    draw(cube, &cube_model);

    /* The sphere bounces up and down: only its translation changes. */
    R3D_Mat4 sphere_model = R3D_Mat4Translate((R3D_Vec3){ 2.5f, 1.0f, 1.3f + 0.3f * SDL_sinf(t * 2.0f) });
    draw(sphere, &sphere_model);

    /* Mouse picking.
       R3D_ScreenToRay gives the 3D line that goes from the camera through
       the pixel under the mouse: origin + distance * dir.
       The floor is the plane z = 0, so the ray hits it where
           origin.z + distance * dir.z = 0   ->   distance = -origin.z / dir.z
       dir.z < 0 means the ray goes downwards; otherwise it never hits. */
    R3D_Vec3 origin, dir;
    if (mouse_x >= 0.0f && R3D_ScreenToRay(r3d, mouse_x, mouse_y, &origin, &dir) && dir.z < -1e-4f) {
        float distance = -origin.z / dir.z;
        R3D_Vec3 hit = R3D_Vec3Add(origin, R3D_Vec3Scale(dir, distance));

        /* Only show the marker when the mouse is over the 12 x 12 floor. */
        if (SDL_fabsf(hit.x) <= 6.0f && SDL_fabsf(hit.y) <= 6.0f) {
            R3D_SetDrawColor(r3d, 255, 220, 40, 255);
            R3D_DrawLine3D(r3d, hit, R3D_Vec3Add(hit, (R3D_Vec3){ 0, 0, 1.0f }));   /* a small pole */
            R3D_DrawPoint3D(r3d, hit, 6.0f);                                         /* 6-pixel dot */

            char label[64];
            SDL_snprintf(label, sizeof(label), "(%.1f, %.1f)", hit.x, hit.y);
            R3D_DrawText3D(r3d, R3D_Vec3Add(hit, (R3D_Vec3){ 0, 0, 1.2f }), label);
        }
    }

    R3D_EndFrame(r3d);   /* sort far-to-near and draw everything queued above */

    /* 2D overlay with the current settings and what culling saved. */
    R3D_Stats stats;
    R3D_GetStats(r3d, &stats);
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, 10, 10, "[drag] orbit  [wheel] zoom  [C] culling  [L] lighting  [W] wireframe");
    SDL_RenderDebugTextFormat(renderer, 10, 24, "culling %s, lighting %s | %d triangles drawn, %d culled",
                              culling ? "on" : "off", lighting ? "on" : "off",
                              stats.triangles, stats.culled_triangles);

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate; (void)result;
    /* Free meshes before the context (any order works; this one reads well). */
    R3D_DestroyMesh(floor_mesh);
    R3D_DestroyMesh(sphere);
    R3D_DestroyMesh(cube);
    R3D_DestroyContext(r3d);
}
