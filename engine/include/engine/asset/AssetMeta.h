#pragma once

#include "engine/asset/AssetGuid.h"

namespace engine::asset {

// Reads `<sourcePath>.meta`'s "guid" field, if the sidecar exists and is well-formed
// (docs/01 section 12.3) -- the read-only counterpart to tools/cooker's
// GetOrCreateAssetGuid() (AssetImporter.h/.cpp, Cooker-only). The Editor's Asset Browser
// (step E6, docs/06-editor-roadmap.md) needs to *display* an existing asset's GUID, never
// create one -- assigning a GUID on first cook is the Cooker's job. Returns false if the
// sidecar doesn't exist or has no valid "guid" field.
bool TryReadAssetMetaGuid(const char* sourcePath, AssetGuid& outGuid);

} // namespace engine::asset
