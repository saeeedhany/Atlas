find_package(Qt6 6.7 REQUIRED COMPONENTS Quick QuickControls2 QuickShapes)

set(CMAKE_AUTOMOC ON)

set_source_files_properties(Theme.qml Motion.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(atlas_qml STATIC src/fonts.cpp)
qt_add_qml_module(atlas_qml
    URI Atlas.Ui
    VERSION 1.0
    QML_FILES
        AppButton.qml
        AppComboBox.qml
        AppSpinBox.qml
        AppSwitch.qml
        AppTextArea.qml
        AppTextField.qml
        ConceptCard.qml
        ConceptPanel.qml
        HoverCard.qml
        IconButton.qml
        LinksEditor.qml
        ListEditor.qml
        Main.qml
        MapOverlay.qml
        NotesLayer.qml
        PinnedCard.qml
        Panel.qml
        PanelLayer.qml
        RegionLabels.qml
        Motion.qml
        CertaintyPicker.qml
        Chevron.qml
        Ring.qml
        SectionLabel.qml
        SessionScreen.qml
        SettingsScreen.qml
        StartupError.qml
        StickyNote.qml
        Theme.qml
        TodayPanel.qml
        Toast.qml
        TopBar.qml
        TopicBar.qml
    RESOURCES
        fonts/Inter.ttf
        fonts/Newsreader.ttf
        fonts/IBMPlexMono-Regular.ttf
)

target_include_directories(atlas_qml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(atlas_qml PUBLIC Qt6::Gui)
target_link_libraries(atlas_qml PRIVATE
    atlas_viewmodels atlas_viewmodelsplugin Qt6::Quick Qt6::QuickControls2 Qt6::QuickShapes
)

if (ATLAS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
