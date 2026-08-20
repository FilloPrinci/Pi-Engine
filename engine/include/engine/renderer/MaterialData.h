#pragma once

#include "engine/asset/AssetGuid.h"
#include "engine/renderer/ShaderPropertySchema.h"

#include <glm/vec4.hpp>

#include <string>
#include <unordered_map>

namespace engine::renderer {

// Material assets (post-Editor-E8, docs/07-unity-parity-analysis.md's "material assets"
// gap) -- a scene author's way to change what an entity looks like via data (a JSON file
// referenced by GUID) instead of writing a new C++ pipeline/shader per visual variation.
//
// A material is an *instance* of a shader (renderer/ShaderPropertySchema.h): `shaderName`
// says which one (e.g. "ForwardLitColor"), `properties` holds a value per property that
// shader declares. This is deliberately generic -- not a hardcoded `tintColor` field --
// so a new shader can gain a new property (a texture, a scalar roughness-like value, ...)
// without this struct or its file format needing to change; only ShaderPropertySchema.h's
// registry and the renderer's own per-shader dispatch code need to grow. See
// ShaderPropertySchema.h's own comment for why this stays consistent with CLAUDE.md rule
// 7 (no uber-shader) even though the data model is generic.
//
// Stored as plain JSON, read directly at runtime, no Cooker binary step -- same reasoning
// engine::scene's own Scene/Prefab documents stay raw JSON (engine/scene/README.md):
// small, simple, human-authorable data that doesn't benefit from a binary cook. GUID-
// tagged via the existing `.meta` sidecar mechanism (engine::asset::TryReadAssetMetaGuid)
// like any other file under assets/, no material-specific asset-id scheme needed.
//
// Reuses ShaderPropertySchema.h's own ShaderPropertyType (Color/Float/Texture) rather
// than declaring a second, parallel enum for the same concept -- a property's *value*
// (here) and its *declaration* (that header) are two views of the same type tag.
struct MaterialPropertyValue {
    ShaderPropertyType type = ShaderPropertyType::Color;
    glm::vec4 colorValue = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float floatValue = 0.0f;
    asset::AssetGuid textureGuid = asset::kInvalidAssetGuid;
};

struct MaterialData {
    // Matches ShaderPropertySchema.h's MaterialShaderInfo::name -- empty or unrecognized
    // (no such entry in GetMaterialShaderRegistry()) means "this material can't be
    // rendered by anything today"; LoadMaterial() doesn't validate this (a material
    // referencing a not-yet-registered shader still round-trips through Load/Write
    // correctly), only the renderer's own dispatch code decides what to do about it
    // (skip the entity, same "degrade, don't crash" precedent as an unknown script name).
    std::string shaderName;
    std::unordered_map<std::string, MaterialPropertyValue> properties;

    // Typed accessors returning `fallback` when the property is missing or stored as a
    // different type than requested (a hand-edited file with a typo, or a property this
    // material's file predates) -- callers (Inspector, renderer) are expected to pass the
    // shader's own ShaderPropertyDecl::defaultColor/defaultFloat as `fallback` rather than
    // an arbitrary zero value, so a missing property still renders/edits as the shader's
    // intended default.
    glm::vec4 GetColor(const std::string& name, const glm::vec4& fallback) const;
    float GetFloat(const std::string& name, float fallback) const;
    asset::AssetGuid GetTexture(const std::string& name, asset::AssetGuid fallback) const;

    void SetColor(const std::string& name, const glm::vec4& value);
    void SetFloat(const std::string& name, float value);
    void SetTexture(const std::string& name, asset::AssetGuid value);
};

// Reads a `.material.json` file (`{"shader": "...", "properties": {...}}`) into
// `outMaterial`. A property's JSON shape says its type -- a 4-number array is Color, a
// bare number is Float, an object with a "guid" string is Texture -- so the file itself
// never needs a redundant per-property "type" field. Returns false (outMaterial left at
// its default: empty shaderName, no properties) if the file can't be opened/parsed.
// Unrecognized property value shapes are skipped, not treated as a parse failure --
// degrade, don't crash, same precedent as SceneDocument.cpp's own per-field parsing.
bool LoadMaterial(const char* path, MaterialData& outMaterial);

// Inverse of LoadMaterial() -- writes `material` to `path` in the same shape LoadMaterial()
// reads. Called by the Editor's Inspector whenever a material property edit gesture ends
// (see editor/main.cpp's TrackFieldEdit-adjacent material-editing code), so a live edit
// persists to the actual asset file, not just the in-memory cache the renderer reads from.
bool WriteMaterial(const char* path, const MaterialData& material);

} // namespace engine::renderer
