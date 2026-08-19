#pragma once

#include "engine/scene/SceneDocument.h"

namespace engine::ecs {
class World;
}

namespace engine::scene {

// Loads every entity described in a scene JSON document (docs/01 section 12.2) directly
// into `world`, once, at (0,0,0) (no offset -- that's what makes this different from
// Prefab::Instantiate). `createPhysicsBody` is optional, see SceneDocument.h's own
// comment for why this is a callback rather than a physics::PhysicsWorld* (engine/scene/
// stays physics-agnostic on purpose). Returns false if the file couldn't be parsed --
// parsing happens fully before anything is spawned, so a failure never leaves `world`
// with a half-loaded scene.
bool LoadScene(const char* path, ecs::World& world,
               const CreatePhysicsBodyFn& createPhysicsBody = {});

// Writes every entity currently in `world` back to a scene JSON document at `path`
// (Editor step E4, docs/06-editor-roadmap.md) -- the Editor's "Save" action. See
// SceneDocument.h's ExtractEntityDescs() for the one deliberate gap (Rigidbody
// static/dynamic flag isn't recoverable, so such an entity is omitted with a warning
// rather than guessed).
bool SaveScene(const char* path, const ecs::World& world);

} // namespace engine::scene
