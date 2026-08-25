// Defines the stb_image implementation (vendored third_party/stb_image.h, the same
// header tools/cooker's own CookTexture.cpp already uses to decode a source .png at cook
// time) for the whole `editor` executable -- the Asset Browser's texture thumbnails
// (post-Editor-E8, docs/07-unity-parity-analysis.md's Asset Browser row) decode a source
// image directly, at thumbnail-display time, so `editor` needs its own decoder rather
// than reusing anything engine_core exposes (that header is PRIVATE to engine_core's own
// MeshLoader.cpp/CookTexture-adjacent sources, never propagated to a consumer).
//
// Exactly one translation unit in the whole `editor` binary may define
// STB_IMAGE_IMPLEMENTATION before including the header -- kept in this dedicated file
// rather than folded into main.cpp so the vendored library's own C-style code (which
// trips this project's strict warning flags, not ours to fix, same reasoning
// engine/CMakeLists.txt already applies to MeshLoader.cpp for cgltf.h) only ever needs
// warnings suppressed for this one file, never for any of the Editor's own code. Every
// other file that needs stbi_load()/stbi_image_free()/stbi_failure_reason() -- main.cpp
// -- just includes <stb_image.h> normally, without the implementation macro, and links
// against the symbols defined here.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
