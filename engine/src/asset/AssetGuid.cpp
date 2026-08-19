#include "engine/asset/AssetGuid.h"

#include <cctype>
#include <charconv>
#include <random>
#include <sstream>
#include <iomanip>

namespace engine::asset {

AssetGuid GenerateAssetGuid() {
    // thread_local: tools/cooker cooks assets sequentially today (docs/03's shared
    // cooked_assets target, one add_custom_command per asset), but this stays safe if
    // that ever changes without needing a second look here.
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;

    AssetGuid guid;
    do {
        guid.high = dist(rng);
        guid.low = dist(rng);
    } while (!guid.IsValid()); // reject the all-zero sentinel; astronomically unlikely

    return guid;
}

std::string ToString(const AssetGuid& guid) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << guid.high << std::setw(16)
        << guid.low;
    return oss.str();
}

bool TryParseAssetGuid(const std::string& text, AssetGuid& outGuid) {
    if (text.size() != 32) {
        return false;
    }
    for (char c : text) {
        if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }

    std::uint64_t high = 0;
    std::uint64_t low = 0;
    const auto highResult = std::from_chars(text.data(), text.data() + 16, high, 16);
    if (highResult.ec != std::errc()) {
        return false;
    }
    const auto lowResult = std::from_chars(text.data() + 16, text.data() + 32, low, 16);
    if (lowResult.ec != std::errc()) {
        return false;
    }

    outGuid.high = high;
    outGuid.low = low;
    return true;
}

} // namespace engine::asset
