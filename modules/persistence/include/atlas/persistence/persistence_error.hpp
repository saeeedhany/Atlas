#pragma once

#include <string>

namespace atlas::persistence {

enum class PersistenceErrorCode {
    ConnectionFailed,
    MigrationFailed,
    QueryFailed,
    ConstraintViolation,
};

struct PersistenceError {
    PersistenceErrorCode code;
    std::string detail;
};

}
