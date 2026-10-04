# atlas: the composition root. Owns main(), opens the real database
# file in the user's app-data directory, and runs the Qt event loop.

find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Quick)

qt_add_executable(atlas src/main_qml.cpp)
target_link_libraries(atlas PRIVATE
    atlas_qml atlas_qmlplugin atlas_viewmodels atlas_viewmodelsplugin Qt6::Gui Qt6::Quick
)
target_compile_features(atlas PRIVATE cxx_std_20)
