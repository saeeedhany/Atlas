#include "atlas/viewmodels/clock.hpp"

namespace atlas::viewmodels {

Clock systemClock() {
    return [] { return std::chrono::system_clock::now(); };
}

}
