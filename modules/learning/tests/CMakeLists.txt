add_executable(atlas_learning_tests
    test_main.cpp
    test_fsrs.cpp
    test_memory_ledger.cpp
)

target_link_libraries(atlas_learning_tests PRIVATE atlas_learning)
target_include_directories(atlas_learning_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)

add_test(NAME atlas_learning_tests COMMAND atlas_learning_tests)
