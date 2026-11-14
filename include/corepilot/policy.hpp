#pragma once

#include "types.hpp"
#include "topology.hpp"
#include <string>

namespace corepilot {

enum class PolicyType {
    Unrestricted,
    Pinned,
    PhysicalCore,
    CpuSet,
    NumaLocal,
    Preferred,
    PerformanceCores,
    EfficientCores
};

inline const char* policy_type_to_string(PolicyType type) {
    switch (type) {
        case PolicyType::Pinned:           return "Pinned";
        case PolicyType::PhysicalCore:     return "PhysicalCore";
        case PolicyType::CpuSet:           return "CpuSet";
        case PolicyType::NumaLocal:        return "NumaLocal";
        case PolicyType::Preferred:        return "Preferred";
        case PolicyType::PerformanceCores: return "PerformanceCores";
        case PolicyType::EfficientCores:   return "EfficientCores";
        case PolicyType::Unrestricted:     return "Unrestricted";
        default:                           return "Unknown";
    }
}

class Policy {
public:
    Policy() : type_(PolicyType::Unrestricted), target_id_(0) {}

    static Policy pinned(uint32_t logical_core_id) {
        Policy p;
        p.type_ = PolicyType::Pinned;
        p.target_id_ = logical_core_id;
        p.cpu_set_.set(logical_core_id);
        return p;
    }

    static Policy physical_core(uint32_t physical_core_id) {
        Policy p;
        p.type_ = PolicyType::PhysicalCore;
        p.target_id_ = physical_core_id;
        return p;
    }

    static Policy cpu_set(const CpuSet& set) {
        Policy p;
        p.type_ = PolicyType::CpuSet;
        p.cpu_set_ = set;
        return p;
    }

    static Policy numa_node(NumaId node_id) {
        Policy p;
        p.type_ = PolicyType::NumaLocal;
        p.target_id_ = static_cast<uint32_t>(node_id);
        return p;
    }

    static Policy preferred(uint32_t logical_core_id) {
        Policy p;
        p.type_ = PolicyType::Preferred;
        p.target_id_ = logical_core_id;
        p.cpu_set_.set(logical_core_id);
        return p;
    }

    static Policy performance_cores() {
        Policy p;
        p.type_ = PolicyType::PerformanceCores;
        return p;
    }

    static Policy efficient_cores() {
        Policy p;
        p.type_ = PolicyType::EfficientCores;
        return p;
    }

    static Policy unrestricted() {
        return Policy();
    }

    PolicyType type() const { return type_; }
    uint32_t target_id() const { return target_id_; }
    const CpuSet& explicit_cpu_set() const { return cpu_set_; }

    CpuSet resolve(const SystemTopology& topology) const {
        CpuSet allowed = topology.allowed_cpus();
        if (allowed.empty()) {
            allowed = topology.all_cpus();
        }

        CpuSet target;
        switch (type_) {
            case PolicyType::Pinned:
            case PolicyType::Preferred:
                target.set(target_id_);
                break;
            case PolicyType::PhysicalCore:
                target = topology.physical_core_cpus(target_id_);
                break;
            case PolicyType::CpuSet:
                target = cpu_set_;
                break;
            case PolicyType::NumaLocal:
                target = topology.numa_cpus(static_cast<NumaId>(target_id_));
                break;
            case PolicyType::PerformanceCores:
                target = topology.performance_cores();
                if (target.empty()) target = allowed;
                break;
            case PolicyType::EfficientCores:
                target = topology.efficient_cores();
                if (target.empty()) target = allowed;
                break;
            case PolicyType::Unrestricted:
            default:
                return allowed;
        }

        CpuSet effective = target & allowed;
        return effective.empty() ? target : effective;
    }

    std::string to_string() const {
        std::string s = policy_type_to_string(type_);
        switch (type_) {
            case PolicyType::Pinned:
            case PolicyType::Preferred:
                s += " (Core " + std::to_string(target_id_) + ")";
                break;
            case PolicyType::PhysicalCore:
                s += " (Physical Core " + std::to_string(target_id_) + ")";
                break;
            case PolicyType::CpuSet:
                s += " (" + cpu_set_.to_string() + ")";
                break;
            case PolicyType::NumaLocal:
                s += " (NUMA " + std::to_string(target_id_) + ")";
                break;
            default:
                break;
        }
        return s;
    }

private:
    PolicyType type_ = PolicyType::Unrestricted;
    uint32_t target_id_ = 0;
    CpuSet cpu_set_;
};

} // namespace corepilot
