add_executable(atlas_render_tests
    test_main.cpp
    test_force_directed_layout.cpp
    test_force_directed_layout_scale.cpp
    test_canvas_geometry.cpp
)

target_link_libraries(atlas_render_tests PRIVATE atlas_render)
target_include_directories(atlas_render_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

add_test(NAME atlas_render_tests COMMAND atlas_render_tests --test-suite-exclude=scale)
add_test(NAME atlas_render_scale_tests COMMAND atlas_render_tests --test-suite=scale)
set_tests_properties(atlas_render_scale_tests PROPERTIES LABELS scale)

if (ATLAS_BUILD_UI)
    add_executable(atlas_render_canvas_tests
        test_canvas_main.cpp
        test_canvas_view.cpp
        test_graph_canvas_item.cpp
        test_region_geometry.cpp
    )
    target_link_libraries(atlas_render_canvas_tests PRIVATE atlas_render_canvas)
    target_include_directories(atlas_render_canvas_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
    add_test(NAME atlas_render_canvas_tests COMMAND atlas_render_canvas_tests)
    set_tests_properties(atlas_render_canvas_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
