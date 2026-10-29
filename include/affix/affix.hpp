#pragma once

#include "types.hpp"
#include "topology.hpp"
#include "policy.hpp"
#include "placement.hpp"
#include "this_thread.hpp"
#include "observability.hpp"
#include "workload.hpp"
#include "numa.hpp"
#include "warmup.hpp"
#include "advisor.hpp"

namespace affix {

[[nodiscard]] inline const char* version() {
    return "0.2.0";
}

} // namespace affix
