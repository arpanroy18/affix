#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <bitset>
#include <vector>
#include <sstream>
#include <iomanip>

namespace affix {

constexpr size_t MAX_CPUS = 1024;
constexpr uint32_t kInvalidCoreId = 0xFFFFFFFF;
constexpr int32_t kInvalidNumaId = -1;
using NumaId = int32_t;

enum class CoreType {
    Unknown,
    Standard,
    Performance,
    Efficient
};

inline const char* core_type_to_string(CoreType type) {
    switch (type) {
        case CoreType::Performance: return "Performance (P-core)";
        case CoreType::Efficient:   return "Efficient (E-core)";
        case CoreType::Standard:    return "Standard";
        default:                    return "Unknown";
    }
}

class CpuSet {
public:
    CpuSet() = default;

    explicit CpuSet(std::initializer_list<size_t> cpus) {
        for (auto cpu : cpus) {
            set(cpu);
        }
    }

    void set(size_t cpu) {
        if (cpu < MAX_CPUS) {
            bits_.set(cpu);
        }
    }

    void reset(size_t cpu) {
        if (cpu < MAX_CPUS) {
            bits_.reset(cpu);
        }
    }

    void clear() {
        bits_.reset();
    }

    bool test(size_t cpu) const {
        return (cpu < MAX_CPUS) ? bits_.test(cpu) : false;
    }

    bool empty() const {
        return bits_.none();
    }

    [[nodiscard]] size_t count() const {
        return bits_.count();
    }

    int first_set() const {
        for (size_t i = 0; i < MAX_CPUS; ++i) {
            if (bits_.test(i)) return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<uint32_t> to_vector() const {
        std::vector<uint32_t> result;
        result.reserve(bits_.count());
        for (size_t i = 0; i < MAX_CPUS; ++i) {
            if (bits_.test(i)) {
                result.push_back(static_cast<uint32_t>(i));
            }
        }
        return result;
    }

    std::string to_string() const {
        if (empty()) return "none";
        std::ostringstream ss;
        bool first = true;
        size_t start = 0;
        bool in_range = false;

        for (size_t i = 0; i <= MAX_CPUS; ++i) {
            bool active = (i < MAX_CPUS && bits_.test(i));
            if (active) {
                if (!in_range) {
                    start = i;
                    in_range = true;
                }
            } else if (in_range) {
                if (!first) ss << ",";
                first = false;
                if (start == i - 1) {
                    ss << start;
                } else {
                    ss << start << "-" << (i - 1);
                }
                in_range = false;
            }
        }
        return ss.str();
    }

    [[nodiscard]] static CpuSet from_string(const std::string& str) {
        CpuSet set;
        std::istringstream ss(str);
        std::string token;
        auto parse_number = [](const std::string& text, size_t& out) {
            try {
                out = std::stoul(text);
                return true;
            } catch (...) {
                return false;
            }
        };
        while (std::getline(ss, token, ',')) {
            size_t first = token.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            size_t last = token.find_last_not_of(" \t\r\n");
            std::string item = token.substr(first, last - first + 1);

            size_t dash = item.find('-');
            size_t start = 0;
            size_t end = 0;
            if (dash != std::string::npos) {
                if (!parse_number(item.substr(0, dash), start) ||
                    !parse_number(item.substr(dash + 1), end)) {
                    continue;
                }
                if (start > end) continue;
                for (size_t i = start; i <= end; ++i) {
                    set.set(i);
                }
            } else if (parse_number(item, start)) {
                set.set(start);
            }
        }
        return set;
    }

    CpuSet operator&(const CpuSet& other) const {
        CpuSet res;
        res.bits_ = this->bits_ & other.bits_;
        return res;
    }

    CpuSet operator|(const CpuSet& other) const {
        CpuSet res;
        res.bits_ = this->bits_ | other.bits_;
        return res;
    }

    CpuSet operator~() const {
        CpuSet res;
        res.bits_ = ~this->bits_;
        return res;
    }

    bool operator==(const CpuSet& other) const {
        return bits_ == other.bits_;
    }

    bool operator!=(const CpuSet& other) const {
        return bits_ != other.bits_;
    }

    const std::bitset<MAX_CPUS>& raw_bits() const {
        return bits_;
    }

    std::bitset<MAX_CPUS>& raw_bits() {
        return bits_;
    }

private:
    std::bitset<MAX_CPUS> bits_;
};

} // namespace affix
