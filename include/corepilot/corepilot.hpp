#pragma once

#include "types.hpp"
#include "topology.hpp"
#include "policy.hpp"
#include "placement.hpp"
#include "this_thread.hpp"
#include "observability.hpp"
#include "workload.hpp"

namespace corepilot {

inline const char* version() {
    return "0.1.0";
}

} // namespace corepilot
