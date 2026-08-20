#pragma once

#include <glm/vec4.hpp>

namespace engine::renderer {

// Material assets (post-Editor-E8, docs/07-unity-parity-analysis.md's "material assets"
// gap) -- a scene author's way to change what an entity looks like via data (a JSON file
// referenced by GUID) instead of writing a new C++ pipeline/shader per visual variation.
// **v1 scope**: a flat tint color only -- no texture reference yet (that needs RHITexture/
// descriptor-set plumbing neither editor/main.cpp nor editor/play_main.cpp have today,
// only samples/m7_textures does; a real follow-up, not silently dropped, see
// docs/07-unity-parity-analysis.md and this type's own README). Rendered by
// ForwardLitColorPipeline (a separate concrete class, never branching inside
// ForwardLitPipeline itself -- CLAUDE.md rule 7): an entity with no material assigned
// keeps rendering exactly as it always has (ForwardLitPipeline's own debug normal-color
// visualization, M1's original exit criterion, untouched).
//
// Stored as plain JSON, read directly at runtime, no Cooker binary step -- same reasoning
// engine::scene's own Scene/Prefab documents stay raw JSON (engine/scene/README.md):
// small, simple, human-authorable data that doesn't benefit from a binary cook. GUID-
// tagged via the existing `.meta` sidecar mechanism (engine::asset::TryReadAssetMetaGuid)
// like any other file under assets/, no material-specific asset-id scheme needed.
struct MaterialData {
    glm::vec4 tintColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // opaque white
};

// Reads a `.material.json` file (`{"tintColor": [r, g, b, a]}`) into `outMaterial`.
// Returns false (outMaterial left at its default) if the file can't be opened/parsed.
bool LoadMaterial(const char* path, MaterialData& outMaterial);

// Inverse of LoadMaterial() -- writes `material` to `path` in the same shape LoadMaterial()
// reads. Not called by the Editor yet (no material-editing UI exists, see this type's own
// README) -- exists for round-trip testing and so a future material editor has a real
// counterpart to LoadMaterial() to call rather than needing to invent one then.
bool WriteMaterial(const char* path, const MaterialData& material);

} // namespace engine::renderer
