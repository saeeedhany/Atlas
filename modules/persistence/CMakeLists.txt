# atlas-persistence: SQLite-backed storage for the domain model.
# Depends on atlas-core and SQLite; nothing depends on this except
# atlas-graph (M2) and above. The system SQLite dev package is used for
# this sandbox build; for cross-platform shipping (M9), vendoring the
# SQLite amalgamation instead removes the dependency on a system
# package that may not exist on a fresh Windows/macOS machine — see
# README for the trade-off. Swapping the SQLite source is isolated to
# this one CMakeLists.txt.
find_package(SQLite3 REQUIRED)

# CMake renamed the imported target from SQLite::SQLite3 to
# SQLite3::SQLite3 partway through its release history (to match the
# FindSQLite3 module name); the old name now prints a deprecation
# warning on newer CMake but still exists. Detecting which target was
# actually defined avoids pinning this build to one CMake version.
if (TARGET SQLite3::SQLite3)
    set(ATLAS_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(ATLAS_SQLITE_TARGET SQLite::SQLite3)
endif()

add_library(atlas_persistence STATIC
    src/detail/statement.cpp
    src/migrations.cpp
    src/database.cpp
    src/knowledge_object_repository.cpp
    src/relationship_repository.cpp
)

target_include_directories(atlas_persistence
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(atlas_persistence PUBLIC cxx_std_20)

# PUBLIC, not PRIVATE: even though SQLite's types never appear in our
# public headers (database.hpp forward-declares sqlite3), anything
# linking this static library still needs the SQLite symbols resolved
# at link time. PRIVATE would hide that requirement from consumers and
# produce a confusing "undefined reference to sqlite3_open" at the
# final executable, far from where the real dependency lives.
target_link_libraries(atlas_persistence PUBLIC atlas_core ${ATLAS_SQLITE_TARGET})

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
