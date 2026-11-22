/*
  SDL3_r3d: a small software 3D layer on top of SDL_Renderer.
  Copyright (C) 2026 TanoDev0

  Distributed under the zlib license. See LICENSE.txt.
*/

/* Camera. */

#include "SDL_r3d_internal.h"

/* How close to straight up/down the camera may look (radians from the pole). */
#define R3D_PITCH_LIMIT (SDL_PI_F * 0.5f - 0.01f)

/* Rotates the camera around its target and zooms in/out (mouse orbit). */
bool R3D_OrbitCamera(R3D_Camera *camera, float yaw_delta, float pitch_delta, float zoom_factor)
{
    if (!camera) {
        return SDL_InvalidParamError("camera");
    }
    if (zoom_factor <= 0.0f) {
        return SDL_InvalidParamError("zoom_factor");
    }

    R3D_Vec3 up = R3D_Vec3Normalize(camera->up);
    if (R3D_Vec3Length(up) == 0.0f) {
        return SDL_SetError("R3D_OrbitCamera: camera up vector is zero");
    }

    R3D_Vec3 offset = R3D_Vec3Sub(camera->position, camera->target);
    float radius = R3D_Vec3Length(offset);
    if (radius == 0.0f) {
        return SDL_SetError("R3D_OrbitCamera: camera position equals its target");
    }

    /* Split the offset into a height along `up` and a horizontal part. */
    float height = R3D_Vec3Dot(offset, up);
    R3D_Vec3 horizontal = R3D_Vec3Sub(offset, R3D_Vec3Scale(up, height));
    if (R3D_Vec3Length(horizontal) < 1e-6f) {
        /* Looking straight down/up: pick any horizontal direction. */
        R3D_Vec3 any = (SDL_fabsf(up.x) < 0.9f) ? (R3D_Vec3){ 1, 0, 0 } : (R3D_Vec3){ 0, 1, 0 };
        horizontal = R3D_Vec3Cross(up, R3D_Vec3Cross(any, up));
    }
    horizontal = R3D_Vec3Normalize(horizontal);

    /* Yaw: spin the horizontal direction around `up`. */
    R3D_Mat4 spin = R3D_Mat4Rotate(up, yaw_delta);
    horizontal = R3D_Vec3Normalize(R3D_Mat4TransformPoint(spin, horizontal));

    /* Pitch: change the elevation angle, never going over the pole. */
    float elevation = SDL_asinf(SDL_clamp(height / radius, -1.0f, 1.0f)) + pitch_delta;
    elevation = SDL_clamp(elevation, -R3D_PITCH_LIMIT, R3D_PITCH_LIMIT);

    /* Zoom: scale the distance, but never closer than the near plane. */
    radius *= zoom_factor;
    float min_radius = (camera->near_plane > 0.0f ? camera->near_plane : 0.01f) * 2.0f;
    if (radius < min_radius) {
        radius = min_radius;
    }

    R3D_Vec3 dir = R3D_Vec3Add(R3D_Vec3Scale(horizontal, SDL_cosf(elevation)),
                               R3D_Vec3Scale(up, SDL_sinf(elevation)));
    camera->position = R3D_Vec3Add(camera->target, R3D_Vec3Scale(dir, radius));
    return true;
}
