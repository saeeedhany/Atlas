# atlas-core: the domain model. Zero dependencies on Qt, SQLite, or
# OpenGL by design — see M0 requirements (FR5). Every other module
# depends inward on this one; this one depends on nothing in Atlas.

add_library(atlas_core STATIC
    src/uuid.cpp
    src/enums.cpp
    src/knowledge_object.cpp
    src/relationship.cpp
)

target_include_directories(atlas_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(atlas_core PUBLIC cxx_std_20)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
