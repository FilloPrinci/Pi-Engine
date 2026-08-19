#pragma once

// `cooker texture <input.png> <output.tex>` (M7, Asset Pipeline textures step). Decodes
// via stb_image (third_party/stb_image.h, forced to 4 channels) and writes
// engine::renderer::CookedTexture.h's binary format -- tightly packed RGBA8, no mipmaps,
// no block compression (ETC2/BC7, docs/01 section 12.2 -- deferred, same as meshes).
int CookTexture(int argc, char** argv);
