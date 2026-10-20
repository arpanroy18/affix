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

