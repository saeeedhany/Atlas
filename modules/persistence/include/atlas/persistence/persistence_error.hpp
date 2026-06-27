#pragma once

#include <string>

namespace atlas::persistence {

// Unlike atlas-core's ValidationError (a plain enum — domain failures
// are few and self-explanatory), persistence failures originate from an
// external system (the filesystem, SQLite) where the underlying message
// genuinely matters for diagnosis: "disk full," "database is locked,"
// "file permission denied" all look identical as a bare error code.
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

}  // namespace atlas::persistence
