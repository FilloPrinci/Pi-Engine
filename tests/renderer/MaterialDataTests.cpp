#include "engine/renderer/MaterialData.h"
#include "engine/renderer/ShaderPropertySchema.h"

#include <doctest/doctest.h>

#include <cstdio>

using engine::asset::AssetGuid;
using engine::asset::GenerateAssetGuid;
using engine::renderer::FindMaterialShader;
using engine::renderer::LoadMaterial;
using engine::renderer::MaterialData;
using engine::renderer::ShaderPropertyType;
using engine::renderer::WriteMaterial;

namespace {

// Same reasoning as CookedTextureTests.cpp's identical helper -- each TEST_CASE gets its
// own file name so cases stay order-independent.
struct ScopedTempFile {
    explicit ScopedTempFile(const char* path) : m_path(path) {}
    ~ScopedTempFile() { std::remove(m_path); }
    const char* m_path;
};

} // namespace

TEST_CASE("WriteMaterial/LoadMaterial round-trip preserves shaderName and a Color property") {
    ScopedTempFile file("material_roundtrip_color.tmp.material.json");

    MaterialData original;
    original.shaderName = "ForwardLitColor";
    original.SetColor("tintColor", glm::vec4(0.85f, 0.1f, 0.1f, 1.0f));

    REQUIRE(WriteMaterial(file.m_path, original));

    MaterialData loaded;
    REQUIRE(LoadMaterial(file.m_path, loaded));
    CHECK(loaded.shaderName == "ForwardLitColor");
    CHECK(loaded.GetColor("tintColor", glm::vec4(0.0f)) == glm::vec4(0.85f, 0.1f, 0.1f, 1.0f));
}

TEST_CASE("WriteMaterial/LoadMaterial round-trip preserves a Float property") {
    ScopedTempFile file("material_roundtrip_float.tmp.material.json");

    MaterialData original;
    original.shaderName = "SomeFutureShader";
    original.SetFloat("roughness", 0.42f);

    REQUIRE(WriteMaterial(file.m_path, original));

    MaterialData loaded;
    REQUIRE(LoadMaterial(file.m_path, loaded));
    CHECK(loaded.GetFloat("roughness", -1.0f) == doctest::Approx(0.42f));
}

TEST_CASE("WriteMaterial/LoadMaterial round-trip preserves a Texture property") {
    ScopedTempFile file("material_roundtrip_texture.tmp.material.json");

    const AssetGuid textureGuid = GenerateAssetGuid();
    MaterialData original;
    original.shaderName = "ForwardLitTexturedColor";
    original.SetColor("tintColor", glm::vec4(1.0f));
    original.SetTexture("albedoTexture", textureGuid);

    REQUIRE(WriteMaterial(file.m_path, original));

    MaterialData loaded;
    REQUIRE(LoadMaterial(file.m_path, loaded));
    CHECK(loaded.GetTexture("albedoTexture", AssetGuid{}) == textureGuid);
    CHECK(loaded.GetColor("tintColor", glm::vec4(0.0f)) == glm::vec4(1.0f));
}

TEST_CASE("MaterialData::GetColor/GetFloat/GetTexture return the fallback when missing") {
    MaterialData material;
    CHECK(material.GetColor("tintColor", glm::vec4(0.5f)) == glm::vec4(0.5f));
    CHECK(material.GetFloat("roughness", 0.75f) == doctest::Approx(0.75f));
    const AssetGuid fallbackGuid = GenerateAssetGuid();
    CHECK(material.GetTexture("albedoTexture", fallbackGuid) == fallbackGuid);
}

TEST_CASE("MaterialData::GetColor returns the fallback when the property exists as a "
          "different type") {
    MaterialData material;
    material.SetFloat("tintColor", 1.0f); // wrong type on purpose.
    CHECK(material.GetColor("tintColor", glm::vec4(0.5f)) == glm::vec4(0.5f));
}

TEST_CASE("LoadMaterial leaves outMaterial empty when \"properties\" is missing") {
    ScopedTempFile file("material_no_properties.tmp.material.json");
    std::FILE* f = std::fopen(file.m_path, "wb");
    REQUIRE(f != nullptr);
    const char bytes[] = R"({"shader": "ForwardLitColor"})";
    std::fwrite(bytes, 1, sizeof(bytes) - 1, f);
    std::fclose(f);

    MaterialData loaded;
    REQUIRE(LoadMaterial(file.m_path, loaded));
    CHECK(loaded.shaderName == "ForwardLitColor");
    CHECK(loaded.properties.empty());
}

TEST_CASE("LoadMaterial rejects malformed JSON") {
    ScopedTempFile file("material_garbage.tmp.material.json");
    std::FILE* f = std::fopen(file.m_path, "wb");
    REQUIRE(f != nullptr);
    const char bytes[] = "not json at all {{{";
    std::fwrite(bytes, 1, sizeof(bytes) - 1, f);
    std::fclose(f);

    MaterialData loaded;
    CHECK_FALSE(LoadMaterial(file.m_path, loaded));
}

TEST_CASE("LoadMaterial rejects a missing file") {
    MaterialData loaded;
    CHECK_FALSE(LoadMaterial("this_file_does_not_exist.material.json", loaded));
}

TEST_CASE("GetMaterialShaderRegistry knows ForwardLitColor and ForwardLitTexturedColor") {
    const auto* color = FindMaterialShader("ForwardLitColor");
    REQUIRE(color != nullptr);
    REQUIRE(color->properties.size() == 1);
    CHECK(color->properties[0].name == "tintColor");
    CHECK(color->properties[0].type == ShaderPropertyType::Color);

    const auto* texturedColor = FindMaterialShader("ForwardLitTexturedColor");
    REQUIRE(texturedColor != nullptr);
    REQUIRE(texturedColor->properties.size() == 2);
    CHECK(texturedColor->properties[1].name == "albedoTexture");
    CHECK(texturedColor->properties[1].type == ShaderPropertyType::Texture);
}

TEST_CASE("FindMaterialShader returns nullptr for an unregistered shader name") {
    CHECK(FindMaterialShader("SomeShaderThatDoesNotExist") == nullptr);
}

TEST_CASE("GetMaterialShaderRegistry knows ForwardVertexLit and ForwardVertexLitTextured") {
    const auto* vertexLit = FindMaterialShader("ForwardVertexLit");
    REQUIRE(vertexLit != nullptr);
    REQUIRE(vertexLit->properties.size() == 1);
    CHECK(vertexLit->properties[0].name == "tintColor");
    CHECK(vertexLit->properties[0].type == ShaderPropertyType::Color);

    const auto* vertexLitTextured = FindMaterialShader("ForwardVertexLitTextured");
    REQUIRE(vertexLitTextured != nullptr);
    REQUIRE(vertexLitTextured->properties.size() == 2);
    CHECK(vertexLitTextured->properties[0].name == "tintColor");
    CHECK(vertexLitTextured->properties[1].name == "albedoTexture");
    CHECK(vertexLitTextured->properties[1].type == ShaderPropertyType::Texture);
}
