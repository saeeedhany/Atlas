#pragma once

#include <string>

namespace atlas::ui {

// Errors that can originate from any layer the controller orchestrates
// (validation, persistence, the in-memory graph), folded into one type
// so callers — the UI widgets — have a single error shape to handle,
// without needing to know which underlying layer failed.
enum class ControllerErrorCode {
    ValidationFailed,
    NotFound,
    PersistenceFailed,
    // The graph rejected an operation that, given the controller's own
    // write ordering, should have been impossible (e.g. addNode failed
    // right after a fresh id was generated). Not a user-facing
    // scenario — if this ever fires, it means an invariant the
    // controller relies on was violated, which is a bug to investigate,
    // not a condition to recover from gracefully.
    GraphInconsistency,
};

struct ControllerFailure {
    ControllerErrorCode code;
    std::string detail;
};

}  // namespace atlas::ui
