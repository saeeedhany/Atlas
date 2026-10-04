add_executable(atlas_graph_tests
    test_main.cpp
    test_graph_engine.cpp
    test_graph_engine_scale.cpp
    test_project_suggestions.cpp
)

target_link_libraries(atlas_graph_tests PRIVATE atlas_graph)
target_include_directories(atlas_graph_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

add_test(NAME atlas_graph_tests COMMAND atlas_graph_tests --test-suite-exclude=scale)
add_test(NAME atlas_graph_scale_tests COMMAND atlas_graph_tests --test-suite=scale)
set_tests_properties(atlas_graph_scale_tests PROPERTIES LABELS scale)
