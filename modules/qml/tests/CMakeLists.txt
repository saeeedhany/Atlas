add_executable(atlas_qml_tests
    qml_fixture.cpp
    test_components.cpp
    test_concept_panel.cpp
    test_foundation.cpp
    test_links_editor.cpp
    test_main.cpp
    test_map.cpp
    test_session_screen.cpp
    test_settings.cpp
    test_shell.cpp
)

target_link_libraries(atlas_qml_tests PRIVATE
    atlas_qml atlas_qmlplugin atlas_viewmodels atlas_viewmodelsplugin Qt6::Quick
)
target_include_directories(atlas_qml_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)

add_test(NAME atlas_qml_tests COMMAND atlas_qml_tests)
set_tests_properties(atlas_qml_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
