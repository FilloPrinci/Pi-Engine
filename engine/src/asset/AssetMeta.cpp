#include "engine/asset/AssetMeta.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>

namespace engine::asset {

bool TryReadAssetMetaGuid(const char* sourcePath, AssetGuid& outGuid) {
    const std::string metaPath = std::string(sourcePath) + ".meta";
    std::ifstream in(metaPath);
    if (!in.is_open()) {
        return false;
    }

    // A malformed .meta is just "no GUID to show", not a crash -- same reasoning as
    // ParseSceneDocument's own try/catch around nlohmann::json (engine/scene/SceneDocument.cpp).
    try {
        nlohmann::json json;
        in >> json;
        if (json.contains("guid") && json["guid"].is_string()) {
            return TryParseAssetGuid(json["guid"].get<std::string>(), outGuid);
        }
    } catch (const nlohmann::json::exception&) {
        return false;
    }
    return false;
}

bool GenerateAndWriteAssetMetaGuid(const char* sourcePath, AssetGuid& outGuid) {
    const AssetGuid guid = GenerateAssetGuid();
    const std::string metaPath = std::string(sourcePath) + ".meta";

    std::ofstream out(metaPath);
    if (!out.is_open()) {
        return false;
    }

    nlohmann::json json;
    json["guid"] = ToString(guid);
    out << json.dump(2) << '\n';
    if (!out) {
        return false;
    }

    outGuid = guid;
    return true;
}

} // namespace engine::asset
