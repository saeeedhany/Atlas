find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick)

set(CMAKE_AUTOMOC ON)

qt_add_library(atlas_viewmodels STATIC)
qt_add_qml_module(atlas_viewmodels
    URI Atlas.ViewModels
    VERSION 1.0
    SOURCES
        include/atlas/viewmodels/app_settings.hpp
        include/atlas/viewmodels/clock.hpp
        include/atlas/viewmodels/controller_error.hpp
        include/atlas/viewmodels/memory_controller.hpp
        include/atlas/viewmodels/palette.hpp
        include/atlas/viewmodels/placement_controller.hpp
        include/atlas/viewmodels/provided_singleton.hpp
        include/atlas/viewmodels/workspace_controller.hpp
        src/app_settings.cpp
        src/clock.cpp
        src/memory_controller.cpp
        src/palette.cpp
        src/placement_controller.cpp
        src/workspace_controller.cpp
)

target_include_directories(atlas_viewmodels
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/atlas/viewmodels
)
target_compile_features(atlas_viewmodels PUBLIC cxx_std_20)
target_link_libraries(atlas_viewmodels PUBLIC
    atlas_core atlas_persistence atlas_graph atlas_learning atlas_render atlas_render_canvas
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
