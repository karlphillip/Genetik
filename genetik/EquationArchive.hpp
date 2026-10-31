#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <system_error>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace genetik {

// An output-only record. Construct these after a run from authoritative candidate
// measurements; this type has no connection to RNG, fitness, or population state.
struct EquationRecord {
    double trainingLoss{};
    std::size_t complexity{};
    bool mathematicallyValid{};
    std::size_t constantCount{};
    std::size_t constantDescriptionLength{};
    std::string canonicalExpression;
    std::string mathematicalExpression;
    std::string constraints;
};

struct EquationArchiveLimits {
    std::size_t maxRecords{256};
    std::size_t maxRecordTextBytes{65'536};
    std::size_t maxTotalTextBytes{4 * 1024 * 1024};
};

// Deterministic two-objective (loss, node/operation complexity) nondominated set.
// Exact canonical-expression duplicates collapse; prediction agreement is never
// used as a semantic-equivalence test. The bounded archive retains the first
// maxRecords records in deterministic objective/canonical order.
class EquationArchive {
public:
    explicit EquationArchive(EquationArchiveLimits limits = {}) : limits_(limits) {
        if (!limits_.maxRecords || !limits_.maxRecordTextBytes || !limits_.maxTotalTextBytes)
            throw std::invalid_argument("equation archive limits must be positive");
    }

    void insert(EquationRecord record) {
        if (!record.mathematicallyValid || !std::isfinite(record.trainingLoss) || record.trainingLoss < 0.0 || record.canonicalExpression.empty()) return;
        const auto incomingBytes = textBytes(record);
        if (incomingBytes > limits_.maxRecordTextBytes || incomingBytes > limits_.maxTotalTextBytes)
            throw std::length_error("equation archive record text limit");
        for (auto it = records_.begin(); it != records_.end();) {
            if (it->canonicalExpression == record.canonicalExpression) {
                if (!betterRecord(record, *it)) return;
                it = records_.erase(it);
            } else ++it;
        }
        for (const auto& old : records_) if (dominates(old, record)) return;
        records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const auto& old) { return dominates(record, old); }), records_.end());
        records_.push_back(std::move(record));
        std::sort(records_.begin(), records_.end(), order);
        if (records_.size() > limits_.maxRecords) records_.resize(limits_.maxRecords);
        while (!records_.empty() && totalTextBytes() > limits_.maxTotalTextBytes) records_.pop_back();
    }

    const std::vector<EquationRecord>& records() const noexcept { return records_; }

    // Stable JSON Lines with escaped strings and round-trip double precision.
    std::string toJsonLines() const {
        std::string out;
        for (const auto& r : records_) {
            out += "{\"schema\":\"genetik.equation-archive/1\",\"training_loss\":" + number(r.trainingLoss) + ",\"complexity\":" + std::to_string(r.complexity)
                + ",\"mathematically_valid\":true"
                + ",\"constant_count\":" + std::to_string(r.constantCount) + ",\"constant_description_length\":" + std::to_string(r.constantDescriptionLength)
                + ",\"canonical_expression\":" + quote(r.canonicalExpression) + ",\"mathematical_expression\":" + quote(r.mathematicalExpression)
                + ",\"constraints\":" + quote(r.constraints) + "}\n";
        }
        return out;
    }

    std::string toText() const {
        std::string out;
        for (const auto& r : records_) out += "loss=" + number(r.trainingLoss) + " complexity=" + std::to_string(r.complexity) + " equation=" + r.mathematicalExpression + (r.constraints.empty() ? "" : " under " + r.constraints) + "\n";
        return out;
    }

private:
    static std::size_t textBytes(const EquationRecord& r) noexcept {
        const auto a = r.canonicalExpression.size(), b = r.mathematicalExpression.size(), c = r.constraints.size();
        if (a > static_cast<std::size_t>(-1) - b || a + b > static_cast<std::size_t>(-1) - c)
            return static_cast<std::size_t>(-1);
        return a + b + c;
    }
    std::size_t totalTextBytes() const noexcept {
        std::size_t total = 0;
        for (const auto& r : records_) {
            const auto bytes = textBytes(r);
            if (total > static_cast<std::size_t>(-1) - bytes) return static_cast<std::size_t>(-1);
            total += bytes;
        }
        return total;
    }
    static bool dominates(const EquationRecord& a, const EquationRecord& b) noexcept {
        return a.trainingLoss <= b.trainingLoss && a.complexity <= b.complexity && (a.trainingLoss < b.trainingLoss || a.complexity < b.complexity);
    }
    static bool order(const EquationRecord& a, const EquationRecord& b) noexcept {
        if (a.trainingLoss != b.trainingLoss) return a.trainingLoss < b.trainingLoss;
        if (a.complexity != b.complexity) return a.complexity < b.complexity;
        return a.canonicalExpression < b.canonicalExpression;
    }
    static bool betterRecord(const EquationRecord& a, const EquationRecord& b) noexcept {
        if (a.trainingLoss != b.trainingLoss) return a.trainingLoss < b.trainingLoss;
        if (a.complexity != b.complexity) return a.complexity < b.complexity;
        if (a.constantCount != b.constantCount) return a.constantCount < b.constantCount;
        if (a.constantDescriptionLength != b.constantDescriptionLength) return a.constantDescriptionLength < b.constantDescriptionLength;
        if (a.mathematicalExpression != b.mathematicalExpression) return a.mathematicalExpression < b.mathematicalExpression;
        return a.constraints < b.constraints;
    }
    static std::string number(double x) {
        char b[64]; auto r = std::to_chars(b, b + sizeof b, x, std::chars_format::general, 17);
        if (r.ec != std::errc{}) throw std::runtime_error("equation archive number formatting failed");
        return {b, r.ptr};
    }
    static std::string quote(std::string_view s) {
        std::string out = "\"";
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
            else if (c < 0x20) { constexpr char h[]="0123456789abcdef"; out += "\\u00"; out += h[c >> 4]; out += h[c & 15]; }
            else out += static_cast<char>(c);
        }
        out += '"'; return out;
    }
    EquationArchiveLimits limits_;
    std::vector<EquationRecord> records_;
};

} // namespace genetik
