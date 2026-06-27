add_executable(atlas_core_tests
    test_main.cpp
    test_result.cpp
    test_uuid.cpp
    test_strong_id.cpp
    test_enums.cpp
    test_knowledge_object.cpp
    test_relationship.cpp
)

target_link_libraries(atlas_core_tests PRIVATE atlas_core)
target_include_directories(atlas_core_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

add_test(NAME atlas_core_tests COMMAND atlas_core_tests)
