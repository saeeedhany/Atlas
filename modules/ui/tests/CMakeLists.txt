add_executable(atlas_ui_tests
    test_main.cpp
    test_workspace_controller.cpp
    test_knowledge_object_list_model.cpp
    test_relationship_list_model.cpp
    test_relationship_edit_dialog.cpp
    test_main_window.cpp
    test_graph_window.cpp
)

target_link_libraries(atlas_ui_tests PRIVATE atlas_ui)
target_include_directories(atlas_ui_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

add_test(NAME atlas_ui_tests COMMAND atlas_ui_tests)
set_tests_properties(atlas_ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
