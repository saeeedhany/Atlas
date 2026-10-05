find_package(SQLite3 REQUIRED)

if (TARGET SQLite3::SQLite3)
    set(ATLAS_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(ATLAS_SQLITE_TARGET SQLite::SQLite3)
endif()

add_library(atlas_persistence STATIC
    src/detail/statement.cpp
    src/migrations.cpp
    src/database.cpp
    src/knowledge_object_repository.cpp
    src/relationship_repository.cpp
    src/topic_repository.cpp
    src/learning_repository.cpp
    src/placement_repository.cpp
    src/note_repository.cpp
)

target_include_directories(atlas_persistence
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(atlas_persistence PUBLIC cxx_std_20)

target_link_libraries(atlas_persistence PUBLIC atlas_core ${ATLAS_SQLITE_TARGET})

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
