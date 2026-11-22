/*
  SDL3_r3d example: a tiny solar system.

  What this example shows:
    - transform hierarchies: a moon's matrix is built ON TOP of its planet's
      matrix, so when the planet moves, the moon moves with it
    - reusing one mesh per planet and drawing it with a new matrix each frame
    - switching lighting off for one object (the sun glows by itself)
    - drawing curves (the orbits) out of short R3D_DrawLine3D segments
    - labels that follow moving objects (R3D_DrawText3D)

  Controls:
    left mouse drag   orbit
    mouse wheel       zoom
    Space             pause / resume
    Escape            quit
*/

#define SDL_MAIN_USE_CALLBACKS 1   /* SDL calls our 4 SDL_App* functions */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_r3d/SDL_r3d.h>

#define WIDTH  900
#define HEIGHT 700
#define ORBIT_STEPS 96   /* line segments per orbit circle */

/* Everything we need to know about one planet. */
typedef struct Planet
{
    const char *name;
    float distance;      /* orbit radius around the sun */
    float radius;        /* size of the planet */
    float speed;         /* orbit speed, radians per second */
    SDL_FColor color;
    bool has_moon;
} Planet;

static const Planet PLANETS[] = {
    { "Mercury", 3.0f, 0.25f, 1.6f, { 0.70f, 0.65f, 0.60f, 1 }, false },
    { "Venus",   4.6f, 0.40f, 1.2f, { 0.95f, 0.80f, 0.45f, 1 }, false },
    { "Earth",   6.4f, 0.45f, 0.9f, { 0.25f, 0.55f, 0.95f, 1 }, true  },
    { "Mars",    8.2f, 0.35f, 0.7f, { 0.90f, 0.40f, 0.25f, 1 }, true  },
};
#define NUM_PLANETS ((int)SDL_arraysize(PLANETS))

/* The light travels from the sun's side of the scene. */
static const R3D_Vec3 LIGHT = { 0.3f, 1.0f, -0.6f };

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static R3D_Context  *r3d      = NULL;
static R3D_Mesh     *sun      = NULL;
static R3D_Mesh     *planet_mesh[NUM_PLANETS];   /* one sphere per planet (size and color differ) */
static R3D_Mesh     *moon     = NULL;            /* one moon mesh, drawn for every planet that has one */

static bool dragging = false;
static bool paused = false;
static float sim_time = 0.0f;    /* simulated seconds; stops while paused */
static Uint64 last_ticks = 0;

static R3D_Camera camera = {
    .position   = { 0.0f, -16.0f, 9.0f },
    .target     = { 0.0f, 0.0f, 0.0f },   /* the sun */
    .up         = { 0.0f, 0.0f, 1.0f },   /* Z up: orbits lie on the z = 0 plane */
    .fovy       = 0.9f,
    .near_plane = 0.1f,
    .far_plane  = 300.0f,
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)appstate; (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("SDL3_r3d - solar system", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    r3d = R3D_CreateContext(renderer);
    sun = R3D_CreateSphereMesh(1.4f, 14, 20, (SDL_FColor){ 1.0f, 0.85f, 0.30f, 1.0f });
    moon = R3D_CreateSphereMesh(0.14f, 6, 10, (SDL_FColor){ 0.85f, 0.85f, 0.85f, 1.0f });   /* small = few triangles */
    if (!r3d || !sun || !moon) {
        SDL_Log("SDL3_r3d setup failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    for (int i = 0; i < NUM_PLANETS; i++) {
        planet_mesh[i] = R3D_CreateSphereMesh(PLANETS[i].radius, 10, 16, PLANETS[i].color);
        if (!planet_mesh[i]) {
            SDL_Log("R3D_CreateSphereMesh: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    /* Spheres are closed: culling the far half is free speed. */
    R3D_SetCulling(r3d, true);
    R3D_SetLighting(r3d, true, LIGHT, 0.35f);
    last_ticks = SDL_GetTicks();
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
        if (event->key.key == SDLK_SPACE) paused = !paused;
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

/* Draws a circle of the given radius around the origin, on the plane z = 0.
   There is no "draw circle" call: a curve is just many short straight lines.
   Short segments also sort better against the planets than long ones. */
static void draw_orbit(float radius)
{
    for (int i = 0; i < ORBIT_STEPS; i++) {
        float a0 = 2.0f * SDL_PI_F * (float)i / ORBIT_STEPS;         /* angle at this point */
        float a1 = 2.0f * SDL_PI_F * (float)(i + 1) / ORBIT_STEPS;   /* angle at the next one */
        R3D_DrawLine3D(r3d,
                       (R3D_Vec3){ radius * SDL_cosf(a0), radius * SDL_sinf(a0), 0.0f },
                       (R3D_Vec3){ radius * SDL_cosf(a1), radius * SDL_sinf(a1), 0.0f });
    }
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void)appstate;

    /* Advance the simulation clock by the real time that passed, unless paused. */
    Uint64 now = SDL_GetTicks();
    if (!paused) {
        sim_time += (float)(now - last_ticks) / 1000.0f;
    }
    last_ticks = now;

    SDL_SetRenderDrawColor(renderer, 8, 10, 20, SDL_ALPHA_OPAQUE);   /* deep space blue */
    SDL_RenderClear(renderer);

    R3D_BeginFrame(r3d, &camera);

    /* The sun gives light, it does not receive it: turn lighting off just
       for it (full brightness), then turn it back on for the planets.
       Settings apply to the draw calls made while they are active. */
    R3D_SetLighting(r3d, false, LIGHT, 1.0f);
    R3D_Mat4 sun_model = R3D_Mat4RotateZ(sim_time * 0.2f);   /* slow spin */
    R3D_DrawMesh(r3d, sun, &sun_model);
    R3D_SetLighting(r3d, true, LIGHT, 0.35f);

    for (int i = 0; i < NUM_PLANETS; i++) {
        const Planet *p = &PLANETS[i];

        R3D_SetDrawColor(r3d, 50, 55, 80, 255);
        draw_orbit(p->distance);

        /* The planet's place in the system, read right to left:
             1. Translate: move it out to its orbit radius on the X axis.
             2. RotateZ:   swing that point around the sun by the orbit angle. */
        R3D_Mat4 orbit = R3D_Mat4Multiply(R3D_Mat4RotateZ(sim_time * p->speed),
                                          R3D_Mat4Translate((R3D_Vec3){ p->distance, 0.0f, 0.0f }));

        /* The planet also spins on itself: one more rotation, applied first. */
        R3D_Mat4 planet_model = R3D_Mat4Multiply(orbit, R3D_Mat4RotateZ(sim_time * 2.0f));
        R3D_DrawMesh(r3d, planet_mesh[i], &planet_model);

        /* The moon: start from the planet's `orbit` matrix (so the moon goes
           wherever the planet goes) and add the moon's own orbit on top:
             orbit * (rotate around the planet * move out from the planet). */
        if (p->has_moon) {
            R3D_Mat4 moon_model = R3D_Mat4Multiply(orbit,
                                  R3D_Mat4Multiply(R3D_Mat4RotateZ(sim_time * 3.0f),
                                                   R3D_Mat4Translate((R3D_Vec3){ p->radius + 0.45f, 0.0f, 0.0f })));
            R3D_DrawMesh(r3d, moon, &moon_model);
        }

        /* The label goes just above the planet. R3D_Mat4TransformPoint moves
           a point with a matrix: (0, 0, height) in the planet's own space
           becomes "above the planet, wherever it is now". */
        R3D_Vec3 label_pos = R3D_Mat4TransformPoint(orbit, (R3D_Vec3){ 0.0f, 0.0f, p->radius + 0.35f });
        R3D_SetDrawColor(r3d, 210, 210, 230, 255);
        R3D_DrawText3D(r3d, label_pos, p->name);
    }

    R3D_EndFrame(r3d);   /* sort and draw */

    SDL_SetRenderDrawColor(renderer, 200, 200, 220, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, 10, 10, paused ? "[drag] orbit  [wheel] zoom  [space] resume  (paused)"
                                                 : "[drag] orbit  [wheel] zoom  [space] pause");

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate; (void)result;
    for (int i = 0; i < NUM_PLANETS; i++) {
        R3D_DestroyMesh(planet_mesh[i]);
    }
    R3D_DestroyMesh(moon);
    R3D_DestroyMesh(sun);
    R3D_DestroyContext(r3d);
}
