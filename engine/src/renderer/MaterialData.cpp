#include "engine/renderer/MaterialData.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>

namespace engine::renderer {

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
        if (document.contains("tintColor")) {
            const auto& tint = document["tintColor"];
            if (tint.is_array() && tint.size() == 4) {
                material.tintColor = glm::vec4(tint[0].get<float>(), tint[1].get<float>(),
                                               tint[2].get<float>(), tint[3].get<float>());
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
    document["tintColor"] = nlohmann::json::array({material.tintColor.r, material.tintColor.g,
                                                   material.tintColor.b, material.tintColor.a});

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
