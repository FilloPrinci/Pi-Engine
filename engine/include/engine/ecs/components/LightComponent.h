#pragma once

#include <glm/vec3.hpp>

namespace engine::ecs {

// Dynamic + static lighting, phase A (docs/01 section 8.3's "Low-Poly Retro" profile --
// vertex/Blinn-Phong lighting, an indicative budget of 2-4 simultaneous lights -- not the
// "PBR profile" CLAUDE.md section 8 explicitly keeps out of scope). A light is just
// another entity component, same shape as Mesh/Collider/Rigidbody: attach one to any
// entity with a Transform to place/orient it.
//
// `isStatic` is a hint only in this phase -- "this light won't move/change" -- not yet
// acted on by any runtime optimization (every light, static or not, is re-collected and
// re-uploaded to the per-frame light UBO every frame in phase A). It exists now so scene
// JSON/the Editor already have a stable place to record author intent, and so phase B
// (a static shadow map, baked once rather than every frame) has something to filter on
// without another round of format changes -- `castsShadow` is the phase-B-specific flag
// that actually matters there; a light with `isStatic == false` is never eligible to cast
// into the static shadow map regardless of `castsShadow`, since a moving light's shadow
// would immediately be wrong.
struct LightComponent {
    enum class Type {
        Directional, // Direction only (TransformComponent's own forward/-Z axis, no
                     // attenuation -- e.g. "the sun"). Position is ignored.
        Point,       // Position only (TransformComponent's own world position), inverse-
                     // square-ish attenuation out to `range`. Direction is ignored.
    };

    Type type = Type::Directional;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    float range = 10.0f; // Point lights only -- meaningless for Directional.

    bool isStatic = false;     // Hint only in phase A -- see this struct's own comment.
    bool castsShadow = false;  // Phase B (static shadow map) -- meaningless unless
                               // isStatic is also true.
};

} // namespace engine::ecs
