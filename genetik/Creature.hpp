#pragma once
#include "Creature_i.hpp"
#include <iomanip>
#include <sstream>

#define TARGET "'Never underestimate a droid' - General Leia Organa, The Rise of Skywalker"
#define CHROMOSOME_SZ 74

// Concrete character-search example, separate from the reusable scalar-genome contract.
template <typename T>
class Creature : public Creature_i<T> {
public:
    explicit Creature(std::mt19937& rng) {
        std::vector<T> genes(CHROMOSOME_SZ);
        for (auto& gene : genes) gene = std::uniform_int_distribution<int>(32, 126)(rng);
        this->replaceChromosome(std::move(genes));
    }
    explicit Creature(const std::vector<T>& genes) : Creature_i<T>(genes) {}
    std::string getString() const {
        const auto genes = this->chromosome();
        return std::string(genes.begin(), genes.end());
    }
    std::string stats() override {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3) << "str=[ " << getString() << " ] f=";
        switch (this->fitnessState()) {
        case FitnessState::Valid: out << this->fitness(); break;
        case FitnessState::Invalid: out << "invalid"; break;
        case FitnessState::Unevaluated: out << "unevaluated"; break;
        }
        float start, end;
        this->rouletteRange(start, end);
        out << " fP=" << this->fitnessPercent() << " rouletteRange=(" << start << ", " << end << ")";
        return out.str();
    }
protected:
    Fitness evaluate() const override {
        const std::string target = TARGET;
        const auto genes = this->chromosome();
        if (genes.size() != target.size()) return Fitness::invalid();
        unsigned int matched = 0;
        for (std::size_t i = 0; i < target.size(); ++i) matched += genes[i] == target[i];
        return Fitness::valid(static_cast<double>(matched) / target.size());
    }
    T mutatedGene(unsigned int, std::mt19937& rng) const override {
        return std::uniform_int_distribution<int>(32, 126)(rng);
    }
};
