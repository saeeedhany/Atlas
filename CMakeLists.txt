cmake_minimum_required(VERSION 3.21)
project(Atlas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ATLAS_BUILD_TESTS "Build unit tests" ON)
option(ATLAS_BUILD_UI "Build the Qt UI and app (requires Qt6)" ON)

option(ATLAS_WERROR "Treat compiler warnings as errors" OFF)
option(ATLAS_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if (ATLAS_WERROR)
        add_compile_options(-Werror)
    endif()
    if (ATLAS_SANITIZE)
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined)
    endif()
endif()

enable_testing()

add_subdirectory(modules/core)
add_subdirectory(modules/persistence)
add_subdirectory(modules/graph)
add_subdirectory(modules/learning)
add_subdirectory(modules/render)

if (ATLAS_BUILD_UI)
    add_subdirectory(modules/ui)
    add_subdirectory(modules/app)
endif()
