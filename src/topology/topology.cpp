#include "affix/topology.hpp"
#include "../platform/platform.hpp"
#include <sstream>
#include <set>
#include <iomanip>

namespace affix {

const SystemTopology& SystemTopology::instance() {
    static SystemTopology cached = discover();
    return cached;
}

SystemTopology SystemTopology::discover() {
    SystemTopology topo;
    internal::platform_discover_topology(topo);
    return topo;
}

void SystemTopology::add_core(const CpuCore& core) {
    cores_.push_back(core);
}

void SystemTopology::add_numa_node(const NumaNode& node) {
    numa_nodes_.push_back(node);
}

size_t SystemTopology::physical_core_count() const {
    std::set<std::pair<uint32_t, uint32_t>> phys_cores; // (package_id, physical_id)
    for (const auto& c : cores_) {
        phys_cores.insert({c.package_id, c.physical_id});
    }
    return phys_cores.empty() ? cores_.size() : phys_cores.size();
}

const CpuCore* SystemTopology::get_core(uint32_t logical_id) const {
    for (const auto& c : cores_) {
        if (c.logical_id == logical_id) return &c;
    }
    return nullptr;
}

const NumaNode* SystemTopology::get_numa_node(NumaId id) const {
    for (const auto& n : numa_nodes_) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

CpuSet SystemTopology::all_cpus() const {
    CpuSet set;
    for (const auto& c : cores_) {
        set.set(c.logical_id);
    }
    return set;
}

CpuSet SystemTopology::physical_core_cpus(uint32_t physical_id) const {
    CpuSet set;
    for (const auto& c : cores_) {
        if (c.physical_id == physical_id) {
            set.set(c.logical_id);
        }
    }
    return set;
}

CpuSet SystemTopology::numa_cpus(NumaId node) const {
    const NumaNode* n = get_numa_node(node);
    return n ? n->cpus : CpuSet{};
}

CpuSet SystemTopology::smt_siblings(uint32_t logical_id) const {
    const CpuCore* c = get_core(logical_id);
    return c ? c->smt_siblings : CpuSet{};
}

CpuSet SystemTopology::performance_cores() const {
    CpuSet set;
    for (const auto& c : cores_) {
        if (c.core_type == CoreType::Performance) {
            set.set(c.logical_id);
        }
    }
    return set;
}

CpuSet SystemTopology::efficient_cores() const {
    CpuSet set;
    for (const auto& c : cores_) {
        if (c.core_type == CoreType::Efficient) {
            set.set(c.logical_id);
        }
    }
    return set;
}

std::string SystemTopology::summary() const {
    std::ostringstream ss;
    ss << "=== CPU Topology Summary ===\n";
    ss << "Logical Processors:  " << logical_core_count() << "\n";
    ss << "Physical Cores:      " << physical_core_count() << "\n";
    ss << "NUMA Nodes:          " << numa_nodes_.size() << "\n";
    ss << "Process Allowed CPUs:" << allowed_cpus_.to_string() << "\n";
    ss << "Kernel Isolated CPUs:" << (isolated_cpus_.empty() ? "None" : isolated_cpus_.to_string()) << "\n\n";

    ss << std::left << std::setw(8) << "CPU"
       << std::setw(12) << "Physical"
       << std::setw(10) << "Package"
       << std::setw(8) << "NUMA"
       << std::setw(18) << "Type"
       << std::setw(12) << "Isolated"
       << "SMT Siblings\n";
    ss << std::string(80, '-') << "\n";

    for (const auto& c : cores_) {
        ss << std::left << std::setw(8) << c.logical_id
           << std::setw(12) << c.physical_id
           << std::setw(10) << c.package_id
           << std::setw(8) << c.numa_node
           << std::setw(18) << core_type_to_string(c.core_type)
           << std::setw(12) << (c.is_isolated ? "Yes" : "No")
           << c.smt_siblings.to_string() << "\n";
    }

    if (!numa_nodes_.empty()) {
        ss << "\n=== NUMA Nodes ===\n";
        for (const auto& n : numa_nodes_) {
            ss << "Node " << n.id << ": CPUs [" << n.cpus.to_string() << "]";
            if (n.total_memory_bytes > 0) {
                ss << " Memory: " << (n.total_memory_bytes / (1024 * 1024)) << " MB";
            }
            ss << "\n";
        }
    }

    return ss.str();
}

std::string SystemTopology::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"logical_core_count\": " << logical_core_count() << ",\n";
    ss << "  \"physical_core_count\": " << physical_core_count() << ",\n";
    ss << "  \"allowed_cpus\": \"" << allowed_cpus_.to_string() << "\",\n";
    ss << "  \"isolated_cpus\": \"" << isolated_cpus_.to_string() << "\",\n";
    ss << "  \"cores\": [\n";

    for (size_t i = 0; i < cores_.size(); ++i) {
        const auto& c = cores_[i];
        ss << "    {\n";
        ss << "      \"logical_id\": " << c.logical_id << ",\n";
        ss << "      \"physical_id\": " << c.physical_id << ",\n";
        ss << "      \"package_id\": " << c.package_id << ",\n";
        ss << "      \"numa_node\": " << c.numa_node << ",\n";
        ss << "      \"type\": \"" << core_type_to_string(c.core_type) << "\",\n";
        ss << "      \"is_isolated\": " << (c.is_isolated ? "true" : "false") << ",\n";
        ss << "      \"smt_siblings\": \"" << c.smt_siblings.to_string() << "\"\n";
        ss << "    }" << (i + 1 < cores_.size() ? "," : "") << "\n";
    }

    ss << "  ],\n";
    ss << "  \"numa_nodes\": [\n";
    for (size_t i = 0; i < numa_nodes_.size(); ++i) {
        const auto& n = numa_nodes_[i];
        ss << "    {\n";
        ss << "      \"id\": " << n.id << ",\n";
        ss << "      \"cpus\": \"" << n.cpus.to_string() << "\",\n";
        ss << "      \"total_memory_bytes\": " << n.total_memory_bytes << "\n";
        ss << "    }" << (i + 1 < numa_nodes_.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

} // namespace affix
