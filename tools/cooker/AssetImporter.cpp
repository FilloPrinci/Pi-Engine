#include "AssetImporter.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>

using engine::asset::AssetGuid;
using engine::asset::GenerateAssetGuid;
using engine::asset::ToString;
using engine::asset::TryParseAssetGuid;

namespace {

std::string MetaPath(const std::string& sourcePath) {
    return sourcePath + ".meta";
}

} // namespace

AssetGuid GetOrCreateAssetGuid(const std::string& sourcePath) {
    const std::string metaPath = MetaPath(sourcePath);

    std::ifstream in(metaPath);
    if (in.is_open()) {
        nlohmann::json json;
        in >> json;
        AssetGuid guid;
        if (json.contains("guid") && json["guid"].is_string() &&
            TryParseAssetGuid(json["guid"].get<std::string>(), guid)) {
            return guid;
        }
        std::fprintf(stderr,
                      "cooker: \"%s\" exists but has no valid \"guid\" field -- "
                      "regenerating (this changes the asset's id!)\n",
                      metaPath.c_str());
    }

    const AssetGuid guid = GenerateAssetGuid();

    nlohmann::json json;
    json["guid"] = ToString(guid);
    std::ofstream out(metaPath);
    out << json.dump(2) << '\n';

    std::printf("cooker: assigned new GUID %s to \"%s\" (wrote \"%s\" -- commit this file)\n",
                ToString(guid).c_str(), sourcePath.c_str(), metaPath.c_str());
    return guid;
}
