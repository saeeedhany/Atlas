# atlas-graph: in-memory index over KnowledgeObjects and Relationships.
# Depends on atlas-core only — deliberately not on atlas-persistence.
# See module header comment for why.

add_library(atlas_graph STATIC
    src/graph_engine.cpp
)

target_include_directories(atlas_graph PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(atlas_graph PUBLIC cxx_std_20)
target_link_libraries(atlas_graph PUBLIC atlas_core)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
