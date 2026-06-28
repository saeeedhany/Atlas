# atlas-render: layout math (Qt-free, depends on atlas-core +
# atlas-graph only) plus the Qt Quick canvas (added once the layout
# algorithm's performance at 10k nodes is actually verified).

add_library(atlas_render STATIC
    src/force_directed_layout.cpp
)

target_include_directories(atlas_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(atlas_render PUBLIC cxx_std_20)
target_link_libraries(atlas_render PUBLIC atlas_core atlas_graph)

if (ATLAS_BUILD_UI)
    find_package(Qt6 REQUIRED COMPONENTS Quick)
    set(CMAKE_AUTOMOC ON)

    add_library(atlas_render_canvas STATIC
        include/atlas/render/graph_canvas_item.hpp
        src/graph_canvas_item.cpp
    )
    target_include_directories(atlas_render_canvas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(atlas_render_canvas PUBLIC cxx_std_20)
    target_link_libraries(atlas_render_canvas PUBLIC atlas_render atlas_graph Qt6::Quick)
endif()

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
