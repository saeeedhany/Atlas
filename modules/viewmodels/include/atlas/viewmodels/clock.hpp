#pragma once

#include <functional>

#include "atlas/core/memory.hpp"

namespace atlas::viewmodels {

using Clock = std::function<atlas::core::TimePoint()>;

Clock systemClock();

}  // namespace atlas::viewmodels
