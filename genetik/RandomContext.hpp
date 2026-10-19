#pragma once
#include <cstdint>
#include <random>

// Own one per serial run. It must outlive every population and algorithm using it.
// Deliberately not copyable/movable: consumers borrow its engine.
class RandomContext {
public:
    explicit RandomContext(std::uint32_t seed) : seed_(seed), engine_(seed) {}
    RandomContext(const RandomContext&) = delete;
    RandomContext& operator=(const RandomContext&) = delete;
    RandomContext(RandomContext&&) = delete;
    RandomContext& operator=(RandomContext&&) = delete;
    std::uint32_t seed() const noexcept { return seed_; }
    std::mt19937& engine() noexcept { return engine_; }
private:
    const std::uint32_t seed_;
    std::mt19937 engine_;
};
