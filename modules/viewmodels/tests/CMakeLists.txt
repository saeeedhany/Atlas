add_executable(atlas_viewmodels_tests
    test_main.cpp
    test_memory_controller.cpp
    test_placement_controller.cpp
    test_workspace_controller.cpp
)

target_link_libraries(atlas_viewmodels_tests PRIVATE atlas_viewmodels atlas_viewmodelsplugin)
target_include_directories(atlas_viewmodels_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)

add_test(NAME atlas_viewmodels_tests COMMAND atlas_viewmodels_tests)
set_tests_properties(atlas_viewmodels_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
