#pragma once

#include <glm/vec4.hpp>

#include <string>
#include <vector>

namespace engine::renderer {

// A material's data is only meaningful in the context of *which* shader it targets --
// this file is the fixed, hand-declared "what properties does each shader accept" table
// a MaterialData (MaterialData.h) is authored against and the Inspector renders UI from.
//
// This is the piece that lets material editing be genuinely generic (any property the
// shader declares, not just a hardcoded tint) while still honoring CLAUDE.md rule 7
// ("every rendering pipeline is a separate concrete class, never an uber-shader with
// branching"): the genericness lives entirely here, in data describing a *fixed, small*
// set of already-distinct concrete pipeline classes (ForwardLitColorPipeline,
// ForwardLitTexturedColorPipeline, ...) -- nothing here causes any single shader to branch
// at runtime based on a material's data. Picking *which* pipeline to bind for a given
// MaterialData::shaderName is a small, explicit dispatch in the renderer's own code
// (editor/main.cpp, editor/play_main.cpp), never inside a shader itself. There is no
// runtime shader reflection (no SPIRV-Cross/spirv-reflect in this project) -- this table
// is hand-written to match each pipeline's own hand-written push-constant/descriptor-set
// layout, the same "everything explicit, nothing inferred from compiled shader bytes"
// approach already used everywhere else in this engine.
enum class ShaderPropertyType {
    Color,   // glm::vec4 (RGBA) -- edited via a color picker.
    Float,   // A single scalar -- edited via a drag/slider.
    Texture, // engine::asset::AssetGuid, referencing a source image asset under assets/
             // (its cooked .tex, resolved by the caller -- this file knows nothing about
             // cooked paths or RHITexture) -- edited via an asset picker.
};

struct ShaderPropertyDecl {
    std::string name;
    ShaderPropertyType type = ShaderPropertyType::Color;

    // Used when a MaterialData targeting this shader has no explicit entry for this
    // property (a material authored by hand and missing a field, or authored before this
    // shader gained the property) -- MaterialData's own GetColor()/GetFloat()/GetTexture()
    // take a fallback parameter for exactly this case, and the Inspector/renderer always
    // pass this declared default as that fallback rather than an arbitrary zero value.
    glm::vec4 defaultColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float defaultFloat = 0.0f;
};

struct MaterialShaderInfo {
    // Matches MaterialData::shaderName and the concrete pipeline class this shader maps
    // to 1:1 (e.g. "ForwardLitColor" -> ForwardLitColorPipeline) -- the renderer's own
    // dispatch code is what actually encodes that mapping; this struct only carries the
    // name and its property schema, not a pointer/reference to the pipeline class itself
    // (this header stays free of any RHI/Vulkan dependency, same reasoning MaterialData.h
    // does).
    std::string name;
    std::vector<ShaderPropertyDecl> properties;
};

// The fixed, small set of material-compatible shaders this engine has today. Grows one
// entry at a time as a new concrete pipeline gains material support -- never a place to
// describe a hypothetical/generic shader that doesn't have a real pipeline class behind
// it.
const std::vector<MaterialShaderInfo>& GetMaterialShaderRegistry();

// Returns nullptr if `name` doesn't match any registered shader (e.g. a material
// referencing a shader from a newer engine version, or a typo in hand-edited JSON) --
// the caller (Inspector, renderer) is expected to degrade gracefully rather than crash,
// same "unknown script name" precedent already established for scene JSON's own
// `"scripts"` field.
const MaterialShaderInfo* FindMaterialShader(const std::string& name);

} // namespace engine::renderer
