cmake_minimum_required(VERSION 3.21)
project(Atlas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ATLAS_BUILD_TESTS "Build unit tests" ON)
option(ATLAS_BUILD_UI "Build the Qt UI and app (requires Qt6)" ON)

enable_testing()

add_subdirectory(modules/core)
add_subdirectory(modules/persistence)
add_subdirectory(modules/graph)
add_subdirectory(modules/render)

if (ATLAS_BUILD_UI)
    add_subdirectory(modules/ui)
    add_subdirectory(modules/app)
endif()
