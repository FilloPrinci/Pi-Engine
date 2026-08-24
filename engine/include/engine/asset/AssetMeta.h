#pragma once

#include "engine/asset/AssetGuid.h"

namespace engine::asset {

// Reads `<sourcePath>.meta`'s "guid" field, if the sidecar exists and is well-formed
// (docs/01 section 12.3) -- the read-only counterpart to tools/cooker's
// GetOrCreateAssetGuid() (AssetImporter.h/.cpp, Cooker-only). The Editor's Asset Browser
// (step E6, docs/06-editor-roadmap.md) needs to *display* an existing asset's GUID, never
// create one for most asset kinds -- assigning a GUID on first cook is the Cooker's job
// for anything that gets cooked. Returns false if the sidecar doesn't exist or has no
// valid "guid" field.
bool TryReadAssetMetaGuid(const char* sourcePath, AssetGuid& outGuid);

// Generates a fresh GUID and writes it to `<sourcePath>.meta`, overwriting any existing
// sidecar -- the write-side counterpart to TryReadAssetMetaGuid() above. Unlike every
// cooked asset kind (mesh/texture/shader), which only ever gets a GUID assigned once,
// offline, by tools/cooker's own GetOrCreateAssetGuid(), a material asset is never cooked
// at all (MaterialData.h's own comment -- raw JSON, read directly at runtime) -- so a
// *new* material created from inside the Editor (post-E8, "make everything the Editor
// shows manageable" phase 4) has nowhere else to get a GUID from. Returns false (and
// leaves `outGuid` untouched) if the sidecar couldn't be written.
bool GenerateAndWriteAssetMetaGuid(const char* sourcePath, AssetGuid& outGuid);

} // namespace engine::asset
