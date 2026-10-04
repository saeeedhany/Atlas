find_package(Qt6 6.7 REQUIRED COMPONENTS Quick QuickControls2 QuickShapes)

set(CMAKE_AUTOMOC ON)

set_source_files_properties(Theme.qml Motion.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(atlas_qml STATIC)
qt_add_qml_module(atlas_qml
    URI Atlas.Ui
    VERSION 1.0
    QML_FILES
        AppButton.qml
        AppTextArea.qml
        AppTextField.qml
        ConceptPanel.qml
        HoverCard.qml
        ListEditor.qml
        Main.qml
        MapOverlay.qml
        NavRail.qml
        Motion.qml
        Ring.qml
        SectionLabel.qml
        SettingsScreen.qml
        StartupError.qml
        Theme.qml
        TodayOverlay.qml
        Toast.qml
        TopicBar.qml
)

target_link_libraries(atlas_qml PRIVATE
    atlas_viewmodels atlas_viewmodelsplugin Qt6::Quick Qt6::QuickControls2 Qt6::QuickShapes
)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
