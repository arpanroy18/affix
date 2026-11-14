#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <memory>

namespace corepilot {

struct CacheInfo {
    uint32_t level = 0;
    std::string type; // "Data", "Instruction", "Unified"
    uint64_t size_bytes = 0;
    CpuSet shared_cpus;
};

struct CpuCore {
    uint32_t logical_id = INVALID_CORE_ID;
    uint32_t physical_id = INVALID_CORE_ID;
    uint32_t package_id = 0;
    NumaId numa_node = INVALID_NUMA_ID;
    CoreType core_type = CoreType::Standard;
    CpuSet smt_siblings;
    std::vector<CacheInfo> caches;
};

struct NumaNode {
    NumaId id = INVALID_NUMA_ID;
    uint64_t total_memory_bytes = 0;
    CpuSet cpus;
};

class SystemTopology {
public:
    SystemTopology() = default;

    static const SystemTopology& instance();
    static SystemTopology discover();

    size_t logical_core_count() const { return cores_.size(); }
    size_t physical_core_count() const;
    size_t numa_node_count() const { return numa_nodes_.size(); }

    const std::vector<CpuCore>& cores() const { return cores_; }
    const std::vector<NumaNode>& numa_nodes() const { return numa_nodes_; }

    const CpuCore* get_core(uint32_t logical_id) const;
    const NumaNode* get_numa_node(NumaId id) const;

    CpuSet allowed_cpus() const { return allowed_cpus_; }
    CpuSet all_cpus() const;
    CpuSet physical_core_cpus(uint32_t physical_id) const;
    CpuSet numa_cpus(NumaId node) const;
    CpuSet smt_siblings(uint32_t logical_id) const;
    CpuSet performance_cores() const;
    CpuSet efficient_cores() const;

    std::string summary() const;
    std::string to_json() const;

    void add_core(const CpuCore& core);
    void add_numa_node(const NumaNode& node);
    void set_allowed_cpus(const CpuSet& set) { allowed_cpus_ = set; }

private:
    std::vector<CpuCore> cores_;
    std::vector<NumaNode> numa_nodes_;
    CpuSet allowed_cpus_;
};

inline const SystemTopology& get_system_topology() {
    return SystemTopology::instance();
}

} // namespace corepilot
