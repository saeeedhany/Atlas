# atlas-ui: the first module that depends on Qt. Bridges
# atlas-persistence and atlas-graph into something a window can show.
# WorkspaceController carries the "write database first, touch the
# graph only after that succeeds" invariant — see its header comment.

find_package(Qt6 REQUIRED COMPONENTS Widgets Quick QuickWidgets)

set(CMAKE_AUTOMOC ON)

add_library(atlas_ui STATIC
    include/atlas/ui/workspace_controller.hpp
    include/atlas/ui/knowledge_object_list_model.hpp
    include/atlas/ui/knowledge_object_edit_dialog.hpp
    include/atlas/ui/relationship_edit_dialog.hpp
    include/atlas/ui/relationship_list_model.hpp
    include/atlas/ui/main_window.hpp
    include/atlas/ui/graph_window.hpp
    include/atlas/ui/relationships_window.hpp
    src/workspace_controller.cpp
    src/knowledge_object_list_model.cpp
    src/knowledge_object_edit_dialog.cpp
    src/relationship_edit_dialog.cpp
    src/relationship_list_model.cpp
    src/main_window.cpp
    src/graph_window.cpp
    src/relationships_window.cpp
)

target_include_directories(atlas_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(atlas_ui PUBLIC cxx_std_20)
target_link_libraries(atlas_ui PUBLIC
    atlas_core atlas_persistence atlas_graph atlas_render atlas_render_canvas
    Qt6::Widgets Qt6::Quick Qt6::QuickWidgets
)

# GraphWindow loads GraphView.qml directly from this source location
# rather than through Qt's resource system — a deliberate v1
# simplification (see GraphWindow's header comment for the trade-off);
# revisit before this app is actually packaged/installed anywhere.
target_compile_definitions(atlas_ui PRIVATE
    ATLAS_RENDER_QML_DIR="${CMAKE_SOURCE_DIR}/modules/render/qml"
)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
