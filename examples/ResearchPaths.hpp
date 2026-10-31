#pragma once

#include <cstdlib>
#include <filesystem>
#include <string_view>

namespace prime_example {

// An unset root preserves the historical relative output path. When configured,
// the value names Genetik-data itself; runs are kept beneath research-runs/.
inline std::filesystem::path researchRunPath(
    const std::filesystem::path& relative,
    const char* dataRoot = std::getenv("GENETIK_DATA_ROOT")) {
    if (dataRoot != nullptr && *dataRoot != '\0')
        return std::filesystem::path(dataRoot) / "research-runs" / relative;
    return std::filesystem::path("out/runs") / relative;
}

} // namespace prime_example
