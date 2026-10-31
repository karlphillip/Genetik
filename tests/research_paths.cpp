#include "../examples/ResearchPaths.hpp"
#include <cstdlib>
#include <iostream>

int main() {
    using prime_example::researchRunPath;
    const std::filesystem::path relative = "prime-exact/seed42/checkpoints";
    if (researchRunPath(relative, nullptr) != std::filesystem::path("out/runs") / relative) {
        std::cerr << "unset GENETIK_DATA_ROOT changed the historical path\n";
        return 1;
    }
    if (researchRunPath(relative, "") != std::filesystem::path("out/runs") / relative) {
        std::cerr << "empty GENETIK_DATA_ROOT changed the historical path\n";
        return 1;
    }
    if (researchRunPath(relative, "/data/Genetik-data") !=
        std::filesystem::path("/data/Genetik-data/research-runs") / relative) {
        std::cerr << "configured GENETIK_DATA_ROOT was not used\n";
        return 1;
    }
    return 0;
}
