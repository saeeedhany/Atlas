add_executable(atlas_persistence_tests
    test_main.cpp
    test_database.cpp
    test_knowledge_object_repository.cpp
    test_relationship_repository.cpp
)

target_link_libraries(atlas_persistence_tests PRIVATE atlas_persistence)
target_include_directories(atlas_persistence_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

add_test(NAME atlas_persistence_tests COMMAND atlas_persistence_tests)
