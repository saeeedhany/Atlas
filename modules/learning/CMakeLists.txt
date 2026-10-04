add_library(atlas_learning STATIC
    src/fsrs.cpp
    src/memory_ledger.cpp
    src/network_rules.cpp
    src/rebuild_grader.cpp
    src/session_planner.cpp
)

target_include_directories(atlas_learning PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(atlas_learning PUBLIC cxx_std_20)
target_link_libraries(atlas_learning PUBLIC atlas_core atlas_graph)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
