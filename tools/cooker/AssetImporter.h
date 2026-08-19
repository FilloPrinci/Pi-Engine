#pragma once

#include "engine/asset/AssetGuid.h"

#include <string>

// Offline-only (docs/01 section 12.3: "every source asset receives a stable GUID on
// first import, saved in a sidecar file... next to the source"): resolves the persistent
// GUID for a source asset, creating its sidecar `<path>.meta` file the first time this
// tool sees it (and reusing the existing one on every cook after that, so the id survives
// renaming/moving as long as the sidecar moves with the source). Never called at
// runtime -- see engine/asset/README.md.
engine::asset::AssetGuid GetOrCreateAssetGuid(const std::string& sourcePath);
