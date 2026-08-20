#include "engine/renderer/MaterialData.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>

namespace engine::renderer {

glm::vec4 MaterialData::GetColor(const std::string& name, const glm::vec4& fallback) const {
    auto it = properties.find(name);
    if (it == properties.end() || it->second.type != ShaderPropertyType::Color) {
        return fallback;
    }
    return it->second.colorValue;
}

float MaterialData::GetFloat(const std::string& name, float fallback) const {
    auto it = properties.find(name);
    if (it == properties.end() || it->second.type != ShaderPropertyType::Float) {
        return fallback;
    }
    return it->second.floatValue;
}

asset::AssetGuid MaterialData::GetTexture(const std::string& name, asset::AssetGuid fallback) const {
    auto it = properties.find(name);
    if (it == properties.end() || it->second.type != ShaderPropertyType::Texture) {
        return fallback;
    }
    return it->second.textureGuid;
}

void MaterialData::SetColor(const std::string& name, const glm::vec4& value) {
    MaterialPropertyValue& entry = properties[name];
    entry.type = ShaderPropertyType::Color;
    entry.colorValue = value;
}

void MaterialData::SetFloat(const std::string& name, float value) {
    MaterialPropertyValue& entry = properties[name];
    entry.type = ShaderPropertyType::Float;
    entry.floatValue = value;
}

void MaterialData::SetTexture(const std::string& name, asset::AssetGuid value) {
    MaterialPropertyValue& entry = properties[name];
    entry.type = ShaderPropertyType::Texture;
    entry.textureGuid = value;
}

bool LoadMaterial(const char* path, MaterialData& outMaterial) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::fprintf(stderr, "LoadMaterial: failed to open \"%s\"\n", path);
        return false;
    }

    // A malformed document throws from inside nlohmann::json -- caught here so a bad
    // material file becomes a clean `return false`, not a crash, same reasoning
    // ParseSceneDocument() gives for its own equivalent try/catch (CLAUDE.md section 5's
    // no-exceptions rule targets renderer/physics/job system hot-path code specifically,
    // not this one-time-at-load-time parsing).
    try {
        nlohmann::json document;
        in >> document;

        MaterialData material;
        material.shaderName = document.value("shader", std::string());

        if (document.contains("properties") && document["properties"].is_object()) {
            for (const auto& [name, value] : document["properties"].items()) {
                if (value.is_array() && value.size() == 4) {
                    material.SetColor(name, glm::vec4(value[0].get<float>(), value[1].get<float>(),
                                                       value[2].get<float>(), value[3].get<float>()));
                } else if (value.is_number()) {
                    material.SetFloat(name, value.get<float>());
                } else if (value.is_object() && value.contains("guid") && value["guid"].is_string()) {
                    asset::AssetGuid guid;
                    if (asset::TryParseAssetGuid(value["guid"].get<std::string>(), guid)) {
                        material.SetTexture(name, guid);
                    }
                }
                // Any other shape (a string, a nested array, ...) is silently skipped --
                // unrecognized, not malformed enough to fail the whole load.
            }
        }

        outMaterial = material;
        return true;
    } catch (const nlohmann::json::exception& e) {
        std::fprintf(stderr, "LoadMaterial: \"%s\" is malformed: %s\n", path, e.what());
        return false;
    }
}

bool WriteMaterial(const char* path, const MaterialData& material) {
    nlohmann::json document;
    document["shader"] = material.shaderName;

    nlohmann::json properties = nlohmann::json::object();
    for (const auto& [name, value] : material.properties) {
        switch (value.type) {
            case ShaderPropertyType::Color:
                properties[name] = nlohmann::json::array(
                    {value.colorValue.r, value.colorValue.g, value.colorValue.b, value.colorValue.a});
                break;
            case ShaderPropertyType::Float:
                properties[name] = value.floatValue;
                break;
            case ShaderPropertyType::Texture:
                properties[name] = nlohmann::json{{"guid", asset::ToString(value.textureGuid)}};
                break;
        }
    }
    document["properties"] = properties;

    std::ofstream out(path);
    if (!out.is_open()) {
        std::fprintf(stderr, "WriteMaterial: failed to open \"%s\" for writing\n", path);
        return false;
    }
    out << document.dump(2);
    if (!out) {
        std::fprintf(stderr, "WriteMaterial: failed while writing \"%s\"\n", path);
        return false;
    }
    return true;
}

} // namespace engine::renderer
