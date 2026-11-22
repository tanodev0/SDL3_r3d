/*
  SDL3_r3d: a small software 3D layer on top of SDL_Renderer.
  Copyright (C) 2026 TanoDev0

  Distributed under the zlib license. See LICENSE.txt.
*/

/* Meshes: any shape as vertices + triangle indices. */

#include "SDL_r3d_internal.h"

/* ----- Building meshes ---------------------------------------------------- */

/* Growing lists used while generating a mesh. */
typedef struct MeshBuilder
{
    R3D_Vertex *vertices;
    int num_vertices;
    int *indices;
    int num_indices;
} MeshBuilder;

/* Allocates room for the given number of vertices and indices. */
static bool builder_init(MeshBuilder *b, int max_vertices, int max_indices)
{
    SDL_zerop(b);
    b->vertices = (R3D_Vertex *)SDL_malloc((size_t)max_vertices * sizeof(R3D_Vertex));
    b->indices = (int *)SDL_malloc((size_t)max_indices * sizeof(int));
    if (!b->vertices || !b->indices) {
        SDL_free(b->vertices);
        SDL_free(b->indices);
        return false;
    }
    return true;
}

/* Frees the builder's temporary lists. */
static void builder_free(MeshBuilder *b)
{
    SDL_free(b->vertices);
    SDL_free(b->indices);
}

/* Adds triangle (a, b, c) so that it faces away from `center` (outward).
   Degenerate triangles (zero area, e.g. at sphere poles) are skipped. */
static void add_outward_triangle(MeshBuilder *b, int ia, int ib, int ic, R3D_Vec3 center)
{
    R3D_Vec3 pa = b->vertices[ia].position;
    R3D_Vec3 pb = b->vertices[ib].position;
    R3D_Vec3 pc = b->vertices[ic].position;
    R3D_Vec3 normal = R3D_Vec3Cross(R3D_Vec3Sub(pb, pa), R3D_Vec3Sub(pc, pa));
    if (R3D_Vec3Length(normal) < 1e-9f) {
        return;
    }
    R3D_Vec3 centroid = R3D_Vec3Scale(R3D_Vec3Add(R3D_Vec3Add(pa, pb), pc), 1.0f / 3.0f);
    if (R3D_Vec3Dot(normal, R3D_Vec3Sub(centroid, center)) < 0.0f) {
        int tmp = ib; ib = ic; ic = tmp;   /* flip to make it counter-clockwise from outside */
    }
    b->indices[b->num_indices++] = ia;
    b->indices[b->num_indices++] = ib;
    b->indices[b->num_indices++] = ic;
}

/* Turns a finished builder into a mesh (copies the data) and frees the builder. */
static R3D_Mesh *builder_finish(MeshBuilder *b)
{
    R3D_Mesh *mesh = R3D_CreateMesh(b->vertices, b->num_vertices, b->indices, b->num_indices);
    builder_free(b);
    return mesh;
}

/* ----- Public ------------------------------------------------------------- */

/* Creates a mesh from vertices and triangle indices (the data is copied). */
R3D_Mesh *R3D_CreateMesh(const R3D_Vertex *vertices, int num_vertices,
                         const int *indices, int num_indices)
{
    if (!vertices) {
        SDL_InvalidParamError("vertices");
        return NULL;
    }
    if (num_vertices < 3) {
        SDL_InvalidParamError("num_vertices");
        return NULL;
    }
    if (indices) {
        if (num_indices < 3 || num_indices % 3 != 0) {
            SDL_InvalidParamError("num_indices");
            return NULL;
        }
        for (int i = 0; i < num_indices; i++) {
            if (indices[i] < 0 || indices[i] >= num_vertices) {
                SDL_SetError("R3D_CreateMesh: index %d is out of range (0..%d)", indices[i], num_vertices - 1);
                return NULL;
            }
        }
    } else {
        if (num_vertices % 3 != 0) {
            SDL_SetError("R3D_CreateMesh: without indices, num_vertices must be a multiple of 3");
            return NULL;
        }
        num_indices = num_vertices;
    }

    R3D_Mesh *mesh = (R3D_Mesh *)SDL_calloc(1, sizeof(*mesh));
    if (!mesh) {
        return NULL;
    }
    mesh->vertices = (R3D_Vertex *)SDL_malloc((size_t)num_vertices * sizeof(R3D_Vertex));
    mesh->indices = (int *)SDL_malloc((size_t)num_indices * sizeof(int));
    if (!mesh->vertices || !mesh->indices) {
        R3D_DestroyMesh(mesh);
        return NULL;
    }

    SDL_memcpy(mesh->vertices, vertices, (size_t)num_vertices * sizeof(R3D_Vertex));
    if (indices) {
        SDL_memcpy(mesh->indices, indices, (size_t)num_indices * sizeof(int));
    } else {
        for (int i = 0; i < num_indices; i++) {
            mesh->indices[i] = i;
        }
    }
    mesh->num_vertices = num_vertices;
    mesh->num_indices = num_indices;
    return mesh;
}

/* Frees a mesh. NULL is ignored. */
void R3D_DestroyMesh(R3D_Mesh *mesh)
{
    if (!mesh) {
        return;
    }
    SDL_free(mesh->vertices);
    SDL_free(mesh->indices);
    SDL_free(mesh);
}

/* Creates a cube of side `size` centered at the origin. */
R3D_Mesh *R3D_CreateCubeMesh(float size, SDL_FColor color)
{
    if (size <= 0.0f) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    MeshBuilder b;
    if (!builder_init(&b, 8, 36)) {
        return NULL;
    }

    /* Corner i has x = bit 0, y = bit 1, z = bit 2. */
    float h = size * 0.5f;
    for (int i = 0; i < 8; i++) {
        b.vertices[b.num_vertices++] = (R3D_Vertex){
            { (i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h }, color
        };
    }

    /* Each face as 4 corners going around its edge. */
    static const int faces[6][4] = {
        { 0, 2, 6, 4 },   /* -X */
        { 1, 3, 7, 5 },   /* +X */
        { 0, 1, 5, 4 },   /* -Y */
        { 2, 3, 7, 6 },   /* +Y */
        { 0, 1, 3, 2 },   /* -Z */
        { 4, 5, 7, 6 },   /* +Z */
    };
    const R3D_Vec3 center = { 0.0f, 0.0f, 0.0f };
    for (int f = 0; f < 6; f++) {
        add_outward_triangle(&b, faces[f][0], faces[f][1], faces[f][2], center);
        add_outward_triangle(&b, faces[f][0], faces[f][2], faces[f][3], center);
    }
    return builder_finish(&b);
}

/* Creates a sphere centered at the origin, made of rings x segments quads. */
R3D_Mesh *R3D_CreateSphereMesh(float radius, int rings, int segments, SDL_FColor color)
{
    if (radius <= 0.0f) {
        SDL_InvalidParamError("radius");
        return NULL;
    }
    if (rings < 2) {
        SDL_InvalidParamError("rings");
        return NULL;
    }
    if (segments < 3) {
        SDL_InvalidParamError("segments");
        return NULL;
    }

    int cols = segments + 1;
    MeshBuilder b;
    if (!builder_init(&b, (rings + 1) * cols, rings * segments * 6)) {
        return NULL;
    }

    /* theta goes from the north pole (+Z) to the south pole; phi around Z. */
    for (int r = 0; r <= rings; r++) {
        float theta = SDL_PI_F * (float)r / (float)rings;
        for (int s = 0; s <= segments; s++) {
            float phi = 2.0f * SDL_PI_F * (float)s / (float)segments;
            b.vertices[b.num_vertices++] = (R3D_Vertex){
                { radius * SDL_sinf(theta) * SDL_cosf(phi),
                  radius * SDL_sinf(theta) * SDL_sinf(phi),
                  radius * SDL_cosf(theta) },
                color
            };
        }
    }

    const R3D_Vec3 center = { 0.0f, 0.0f, 0.0f };
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            int a = r * cols + s;
            int bb = (r + 1) * cols + s;
            add_outward_triangle(&b, a, bb, bb + 1, center);
            add_outward_triangle(&b, a, bb + 1, a + 1, center);
        }
    }
    return builder_finish(&b);
}

/* Creates a flat width x length rectangle on z = 0, centered at the origin,
   split into subdivisions x subdivisions cells. */
R3D_Mesh *R3D_CreatePlaneMesh(float width, float length, int subdivisions, SDL_FColor color)
{
    if (width <= 0.0f) {
        SDL_InvalidParamError("width");
        return NULL;
    }
    if (length <= 0.0f) {
        SDL_InvalidParamError("length");
        return NULL;
    }
    if (subdivisions < 1) {
        SDL_InvalidParamError("subdivisions");
        return NULL;
    }

    int n = subdivisions;
    int cols = n + 1;
    MeshBuilder b;
    if (!builder_init(&b, cols * cols, n * n * 6)) {
        return NULL;
    }

    for (int j = 0; j <= n; j++) {
        for (int i = 0; i <= n; i++) {
            b.vertices[b.num_vertices++] = (R3D_Vertex){
                { width * ((float)i / (float)n - 0.5f), length * ((float)j / (float)n - 0.5f), 0.0f },
                color
            };
        }
    }

    /* Counter-clockwise seen from +Z. */
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            int k = j * cols + i;
            int tri[6] = { k, k + 1, k + cols + 1, k, k + cols + 1, k + cols };
            for (int t = 0; t < 6; t++) {
                b.indices[b.num_indices++] = tri[t];
            }
        }
    }
    return builder_finish(&b);
}

/* Returns the world position of a mesh vertex, applying the optional transform. */
static R3D_Vec3 mesh_position(const R3D_Mesh *mesh, int index, const R3D_Mat4 *transform)
{
    R3D_Vec3 p = mesh->vertices[index].position;
    return transform ? R3D_Mat4TransformPoint(*transform, p) : p;
}

/* Draws a mesh with filled triangles, optionally transformed by a matrix. */
bool R3D_DrawMesh(R3D_Context *ctx, const R3D_Mesh *mesh, const R3D_Mat4 *transform)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (!mesh) {
        return SDL_InvalidParamError("mesh");
    }

    for (int i = 0; i < mesh->num_indices; i += 3) {
        const int *idx = &mesh->indices[i];
        const R3D_Vec3 pos[3] = {
            mesh_position(mesh, idx[0], transform),
            mesh_position(mesh, idx[1], transform),
            mesh_position(mesh, idx[2], transform),
        };
        const SDL_FColor colors[3] = {
            mesh->vertices[idx[0]].color,
            mesh->vertices[idx[1]].color,
            mesh->vertices[idx[2]].color,
        };
        if (!R3D_QueueTriangle(ctx, pos, colors)) {
            return false;
        }
    }
    return true;
}

/* Draws only the edges of a mesh's triangles. */
bool R3D_DrawMeshWireframe(R3D_Context *ctx, const R3D_Mesh *mesh, const R3D_Mat4 *transform)
{
    if (!R3D_CheckFrame(ctx)) {
        return false;
    }
    if (!mesh) {
        return SDL_InvalidParamError("mesh");
    }

    for (int i = 0; i < mesh->num_indices; i += 3) {
        const int *idx = &mesh->indices[i];
        R3D_Vec3 a = mesh_position(mesh, idx[0], transform);
        R3D_Vec3 b = mesh_position(mesh, idx[1], transform);
        R3D_Vec3 c = mesh_position(mesh, idx[2], transform);
        if (!R3D_DrawLine3D(ctx, a, b) || !R3D_DrawLine3D(ctx, b, c) || !R3D_DrawLine3D(ctx, c, a)) {
            return false;
        }
    }
    return true;
}
