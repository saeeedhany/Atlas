add_executable(atlas_learning_tests
    test_main.cpp
    test_fsrs.cpp
    test_memory_ledger.cpp
    test_network_rules.cpp
    test_rebuild_grader.cpp
    test_session_planner.cpp
    test_calibration.cpp
    test_simulated_learner.cpp
)

target_link_libraries(atlas_learning_tests PRIVATE atlas_learning)
target_include_directories(atlas_learning_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)

add_test(NAME atlas_learning_tests COMMAND atlas_learning_tests)
