add_executable(atlas_render_tests
    test_main.cpp
    test_force_directed_layout.cpp
    test_force_directed_layout_scale.cpp
)

target_link_libraries(atlas_render_tests PRIVATE atlas_render)
target_include_directories(atlas_render_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

add_test(NAME atlas_render_tests COMMAND atlas_render_tests --test-suite-exclude=scale)
add_test(NAME atlas_render_scale_tests COMMAND atlas_render_tests --test-suite=scale)
set_tests_properties(atlas_render_scale_tests PROPERTIES LABELS scale)
