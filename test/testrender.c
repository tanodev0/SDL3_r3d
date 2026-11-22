/*
  SDL3_r3d: rendering tests.

  Runs without a window: it uses SDL's "dummy" video driver and the software
  renderer, draws into an offscreen surface and checks positions, clipping,
  culling, statistics and pixels.

  Run:  ctest --test-dir build --output-on-failure
   or:  ./build/testrender
*/

#include <SDL3_r3d/SDL_r3d.h>
#include <stdio.h>

static int num_tests = 0;
static int num_failed = 0;

#define CHECK(cond, name)                                              \
    do {                                                               \
        num_tests++;                                                   \
        if (cond) { printf("  ok    %s\n", name); }                    \
        else      { printf("  FAIL  %s  (%s:%d)\n", name, __FILE__, __LINE__); num_failed++; } \
    } while (0)

#define SIZE 200

static bool near_eq(float a, float b, float eps) { return SDL_fabsf(a - b) < eps; }

/* A camera on the -Y side looking at the origin, Z up. */
static R3D_Camera make_camera(void)
{
    R3D_Camera c = {
        .position = { 0, -10, 0 },
        .target = { 0, 0, 0 },
        .up = { 0, 0, 1 },
        .fovy = SDL_PI_F / 2,
        .near_plane = 0.5f,
        .far_plane = 100.0f,
    };
    return c;
}

/* Reads the color of one pixel of the renderer's target. */
static SDL_Color pixel(SDL_Renderer *renderer, int x, int y)
{
    SDL_Color c = { 0, 0, 0, 0 };
    SDL_Rect r = { x, y, 1, 1 };
    SDL_Surface *s = SDL_RenderReadPixels(renderer, &r);
    if (s) {
        SDL_ReadSurfacePixel(s, 0, 0, &c.r, &c.g, &c.b, &c.a);
        SDL_DestroySurface(s);
    }
    return c;
}

static void clear(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

static void test_errors(SDL_Renderer *renderer, R3D_Context *ctx)
{
    printf("Errors\n");
    CHECK(R3D_CreateContext(NULL) == NULL, "CreateContext(NULL) fails");
    CHECK(!R3D_DrawLine3D(ctx, (R3D_Vec3){ 0 }, (R3D_Vec3){ 1, 0, 0 }) &&
          SDL_strstr(SDL_GetError(), "not inside a frame") != NULL,
          "Drawing outside a frame fails with a clear message");
    R3D_Camera bad = make_camera();
    bad.near_plane = 0.0f;
    CHECK(!R3D_BeginFrame(ctx, &bad), "BeginFrame rejects near_plane = 0");
    CHECK(!R3D_EndFrame(ctx), "EndFrame without BeginFrame fails");
    (void)renderer;
}

static void test_projection(SDL_Renderer *renderer, R3D_Context *ctx)
{
    printf("Projection\n");
    R3D_Camera cam = make_camera();
    clear(renderer);
    R3D_BeginFrame(ctx, &cam);

    SDL_FPoint p;
    CHECK(R3D_ProjectPoint(ctx, (R3D_Vec3){ 0, 0, 0 }, &p) &&
          near_eq(p.x, SIZE / 2, 0.01f) && near_eq(p.y, SIZE / 2, 0.01f),
          "Target projects to the center of the screen");

    /* fovy = 90 degrees: at distance 10, z = 10 is the top edge. */
    CHECK(R3D_ProjectPoint(ctx, (R3D_Vec3){ 0, 0, 10 }, &p) && near_eq(p.y, 0, 0.01f),
          "Point at the edge of the field of view lands on the top edge");
    CHECK(R3D_ProjectPoint(ctx, (R3D_Vec3){ 5, 0, 0 }, &p) && p.x > SIZE / 2,
          "+X appears to the right");
    CHECK(!R3D_ProjectPoint(ctx, (R3D_Vec3){ 0, -20, 0 }, &p), "Point behind the camera is not visible");

    R3D_Vec3 origin, dir;
    CHECK(R3D_ScreenToRay(ctx, SIZE / 2, SIZE / 2, &origin, &dir) &&
          near_eq(dir.x, 0, 1e-4f) && near_eq(dir.y, 1, 1e-4f) && near_eq(dir.z, 0, 1e-4f),
          "ScreenToRay through the center points at the target");
    R3D_EndFrame(ctx);
}

static void test_clipping(SDL_Renderer *renderer, R3D_Context *ctx)
{
    printf("Clipping\n");
    R3D_Camera cam = make_camera();
    R3D_Stats st;

    clear(renderer);
    R3D_BeginFrame(ctx, &cam);
    R3D_DrawLine3D(ctx, (R3D_Vec3){ 0, -20, 0 }, (R3D_Vec3){ 1, -30, 0 });       /* fully behind */
    R3D_DrawLine3D(ctx, (R3D_Vec3){ 0, -20, 0 }, (R3D_Vec3){ 0, 10, 0 });        /* crosses the camera */
    R3D_DrawTriangle3D(ctx, (R3D_Vec3){ -1, -20, 0 }, (R3D_Vec3){ 1, -20, 0 }, (R3D_Vec3){ 0, -25, 1 });
    R3D_DrawTriangle3D(ctx, (R3D_Vec3){ -1, -20, 0 }, (R3D_Vec3){ 1, 5, 0 }, (R3D_Vec3){ 0, 5, 1 });
    R3D_EndFrame(ctx);
    R3D_GetStats(ctx, &st);

    CHECK(st.lines == 1, "Line fully behind the camera is dropped, crossing line is kept");
    CHECK(st.clipped_primitives == 2, "Two primitives were fully behind the camera");
    CHECK(st.triangles >= 1 && st.triangles <= 2, "Triangle crossing the near plane is clipped into 1-2 triangles");
}

static void test_culling_and_meshes(SDL_Renderer *renderer, R3D_Context *ctx)
{
    printf("Meshes and culling\n");
    R3D_Camera cam = make_camera();
    cam.position = (R3D_Vec3){ 6, -8, 5 };   /* sees 3 faces of the cube */
    R3D_Stats st;

    R3D_Mesh *cube = R3D_CreateCubeMesh(2.0f, (SDL_FColor){ 1, 0, 0, 1 });
    CHECK(cube != NULL, "CreateCubeMesh");

    clear(renderer);
    R3D_BeginFrame(ctx, &cam);
    R3D_DrawMesh(ctx, cube, NULL);
    R3D_EndFrame(ctx);
    R3D_GetStats(ctx, &st);
    CHECK(st.triangles == 12 && st.culled_triangles == 0, "Without culling, all 12 triangles are drawn");

    R3D_SetCulling(ctx, true);
    clear(renderer);
    R3D_BeginFrame(ctx, &cam);
    R3D_DrawMesh(ctx, cube, NULL);
    R3D_EndFrame(ctx);
    R3D_GetStats(ctx, &st);
    CHECK(st.triangles == 6 && st.culled_triangles == 6, "With culling, the 3 front faces (6 triangles) are drawn");

    SDL_Color c = pixel(renderer, SIZE / 2, SIZE / 2);
    CHECK(c.r > 200 && c.g < 30 && c.b < 30, "The cube covers the center of the screen in red");
    R3D_SetCulling(ctx, false);

    R3D_Mesh *sphere = R3D_CreateSphereMesh(1.0f, 8, 12, (SDL_FColor){ 1, 1, 1, 1 });
    R3D_SetCulling(ctx, true);
    clear(renderer);
    R3D_BeginFrame(ctx, &cam);
    R3D_DrawMesh(ctx, sphere, NULL);
    R3D_EndFrame(ctx);
    R3D_GetStats(ctx, &st);
    CHECK(sphere && st.triangles > 0 && st.culled_triangles > 0, "Sphere: front faces drawn, back faces culled");
    R3D_SetCulling(ctx, false);

    int bad_indices[3] = { 0, 1, 7 };
    R3D_Vertex tri[3] = { { { 0, 0, 0 }, { 1, 1, 1, 1 } }, { { 1, 0, 0 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 1, 1, 1, 1 } } };
    CHECK(R3D_CreateMesh(tri, 3, bad_indices, 3) == NULL, "CreateMesh rejects out-of-range indices");
    R3D_Mesh *one = R3D_CreateMesh(tri, 3, NULL, 0);
    CHECK(one != NULL, "CreateMesh without indices");

    R3D_DestroyMesh(one);
    R3D_DestroyMesh(sphere);
    R3D_DestroyMesh(cube);
    R3D_DestroyMesh(NULL);   /* must not crash */
}

static void test_depth_order(SDL_Renderer *renderer, R3D_Context *ctx)
{
    printf("Depth order\n");
    R3D_Camera cam = make_camera();

    clear(renderer);
    R3D_BeginFrame(ctx, &cam);
    /* Near green square first, far blue square second: green must still win. */
    R3D_SetDrawColor(ctx, 0, 255, 0, 255);
    R3D_Vec3 near_quad[4] = { { -2, 0, -2 }, { 2, 0, -2 }, { 2, 0, 2 }, { -2, 0, 2 } };
    R3D_DrawPolygon3D(ctx, near_quad, 4);
    R3D_SetDrawColor(ctx, 0, 0, 255, 255);
    R3D_Vec3 far_quad[4] = { { -4, 5, -4 }, { 4, 5, -4 }, { 4, 5, 4 }, { -4, 5, 4 } };
    R3D_DrawPolygon3D(ctx, far_quad, 4);
    R3D_EndFrame(ctx);

    SDL_Color c = pixel(renderer, SIZE / 2, SIZE / 2);
    CHECK(c.g > 200 && c.b < 30, "Nearer polygon hides the farther one, whatever the draw order");

    SDL_SetRenderDrawColor(renderer, 12, 34, 56, 78);
    R3D_BeginFrame(ctx, &cam);
    R3D_DrawAxes(ctx, 5);
    R3D_EndFrame(ctx);
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    CHECK(r == 12 && g == 34 && b == 56 && a == 78, "EndFrame leaves the renderer draw color unchanged");
}

static float SDLCALL inverse_x(void *userdata, float x, float y)
{
    (void)userdata; (void)y;
    return 1.0f / x;   /* infinite at x = 0 */
}

static void test_surface_and_text(SDL_Renderer *renderer, R3D_Context *ctx)
{
    printf("Surfaces and text\n");
    R3D_Camera cam = make_camera();
    R3D_Stats st;

    clear(renderer);
    R3D_BeginFrame(ctx, &cam);
    bool ok = R3D_DrawSurface(ctx, inverse_x, NULL, -2, 2, -2, 2, 5, true);
    R3D_EndFrame(ctx);
    R3D_GetStats(ctx, &st);
    /* 5x5 samples -> 4x4 cells = 32 triangles; the column x = 0 is infinite,
       so the 2 x 4 cells touching it are skipped -> 16 triangles. */
    CHECK(ok && st.triangles == 16, "Surface skips cells where the function is infinite");

    clear(renderer);
    R3D_BeginFrame(ctx, &cam);
    R3D_SetDrawColor(ctx, 255, 255, 255, 255);
    ok = R3D_DrawText3D(ctx, (R3D_Vec3){ 0, 0, 0 }, "X");
    R3D_EndFrame(ctx);
    bool found = false;
    for (int y = SIZE / 2; y < SIZE / 2 + 8 && !found; y++) {
        for (int x = SIZE / 2; x < SIZE / 2 + 8 && !found; x++) {
            if (pixel(renderer, x, y).r > 200) found = true;
        }
    }
    CHECK(ok && found, "Text label is drawn at the projected position");
}

int main(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Surface *target = SDL_CreateSurface(SIZE, SIZE, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *renderer = target ? SDL_CreateSoftwareRenderer(target) : NULL;
    R3D_Context *ctx = renderer ? R3D_CreateContext(renderer) : NULL;
    if (!ctx) {
        printf("Setup failed: %s\n", SDL_GetError());
        return 1;
    }

    test_errors(renderer, ctx);
    test_projection(renderer, ctx);
    test_clipping(renderer, ctx);
    test_culling_and_meshes(renderer, ctx);
    test_depth_order(renderer, ctx);
    test_surface_and_text(renderer, ctx);

    R3D_DestroyContext(ctx);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(target);
    SDL_Quit();

    printf("\n%d tests, %d failed\n", num_tests, num_failed);
    return num_failed == 0 ? 0 : 1;
}
