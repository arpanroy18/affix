#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <memory>

namespace affix {

struct CacheInfo {
    uint32_t level = 0;
    std::string type; // "Data", "Instruction", "Unified"
    uint64_t size_bytes = 0;
    CpuSet shared_cpus;
};

struct CpuCore {
    uint32_t logical_id = kInvalidCoreId;
    uint32_t physical_id = kInvalidCoreId;
    uint32_t package_id = 0;
    NumaId numa_node = kInvalidNumaId;
    CoreType core_type = CoreType::Standard;
    bool is_isolated = false;
    CpuSet smt_siblings;
    std::vector<CacheInfo> caches;
};

struct NumaNode {
    NumaId id = kInvalidNumaId;
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

    [[nodiscard]] const CpuCore* get_core(uint32_t logical_id) const;
    [[nodiscard]] const NumaNode* get_numa_node(NumaId id) const;

    CpuSet allowed_cpus() const { return allowed_cpus_; }
    CpuSet isolated_cpus() const { return isolated_cpus_; }
    bool has_isolated_cpus() const { return !isolated_cpus_.empty(); }
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
    void set_isolated_cpus(const CpuSet& set) { isolated_cpus_ = set; }

private:
    std::vector<CpuCore> cores_;
    std::vector<NumaNode> numa_nodes_;
    CpuSet allowed_cpus_;
    CpuSet isolated_cpus_;
};

[[nodiscard]] inline const SystemTopology& get_system_topology() {
    return SystemTopology::instance();
}

} // namespace affix
