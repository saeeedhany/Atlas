# atlas-app: the composition root. Owns main(), opens the real
# database file in the user's app-data directory, wires the controller
# and main window together, and runs the Qt event loop. Everything
# below this is a library deliberately kept ignorant of where the
# database file lives or that a window even exists.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(atlas_app src/main.cpp)
target_link_libraries(atlas_app PRIVATE atlas_ui Qt6::Widgets)
target_compile_features(atlas_app PRIVATE cxx_std_20)
