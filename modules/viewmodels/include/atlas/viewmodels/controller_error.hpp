#pragma once

#include <string>

namespace atlas::viewmodels {

enum class ControllerErrorCode {
    ValidationFailed,
    NotFound,
    PersistenceFailed,
    GraphInconsistency,
};

struct ControllerFailure {
    ControllerErrorCode code;
    std::string detail;
};

}
