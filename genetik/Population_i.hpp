#pragma once
#include "Creature_i.hpp"
#include <memory>
#include <vector>

template <typename T>
class Population_i {
public:
    virtual ~Population_i() = default;
    virtual GenerationOutcome setSize(const unsigned int& size) = 0;
    // Independently owned snapshot of the committed population, never an editing view.
    virtual std::shared_ptr<std::vector<T>> creatures() const = 0;
    virtual double mean() = 0;
    virtual T bestCreature() = 0;
    // Edits are staged until updateValues(); selection and current-best queries reject pending edits.
    virtual void setCreature(const unsigned int& pos, const T& creature) = 0;
    virtual GenerationOutcome updateValues() = 0;
    virtual std::string stats() = 0;
};
