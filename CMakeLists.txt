cmake_minimum_required(VERSION 3.24)

option(
    FOLDERSNAP_INSTALL_DEPENDENCIES
    "Allow CMake's vcpkg manifest integration to install or rebuild dependencies"
    OFF
)
# This must be set before project() loads the vcpkg toolchain. Normal CMake
# regeneration stays fast because dependency installation is opt-in.
set(
    VCPKG_MANIFEST_INSTALL
    "${FOLDERSNAP_INSTALL_DEPENDENCIES}"
    CACHE BOOL "Install vcpkg manifest dependencies during configure" FORCE
)

project(FolderSnap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(
    Qt6 6.11 REQUIRED
    COMPONENTS Core Concurrent Network Quick QuickControls2 ShaderTools Widgets
)
find_package(ZLIB REQUIRED)

qt_standard_project_setup(REQUIRES 6.11)
include(CTest)

add_library(folder_snap_core STATIC
    src/domain/DomainError.h
    src/domain/Configuration.h src/domain/Configuration.cpp
    src/domain/JsonCodec.h src/domain/JsonCodec.cpp
    src/domain/Snapshot.h src/domain/Snapshot.cpp
    src/domain/Timestamp.h src/domain/Timestamp.cpp
    src/paths/WindowsPaths.h src/paths/WindowsPaths.cpp
    src/ignore/IgnoreMatcher.h src/ignore/IgnoreMatcher.cpp
    src/storage/AtomicFile.h src/storage/AtomicFile.cpp
    src/storage/ConfigurationStore.h src/storage/ConfigurationStore.cpp
    src/storage/StoragePaths.h src/storage/StoragePaths.cpp
    src/storage/SnapshotStore.h src/storage/SnapshotStore.cpp
    src/storage/HistoryStore.h src/storage/HistoryStore.cpp
)
target_include_directories(folder_snap_core PUBLIC src)
target_link_libraries(folder_snap_core PUBLIC Qt6::Core ZLIB::ZLIB)

set(folder_snap_qml_files
    src/qml/Main.qml
    src/qml/Theme.qml
    src/qml/MotionPolicy.qml
    src/qml/shell/TitleBar.qml
    src/qml/navigation/Sidebar.qml
    src/qml/navigation/PageHost.qml
    src/qml/navigation/PageFrame.qml
    src/qml/controls/ActionButton.qml
    src/qml/controls/WindowButton.qml
    src/qml/controls/ToggleSwitch.qml
    src/qml/controls/PageHeading.qml
    src/qml/controls/BodyText.qml
    src/qml/pages/OverviewPage.qml
    src/qml/pages/FoldersPage.qml
    src/qml/pages/ComparePage.qml
    src/qml/pages/SettingsPage.qml
    src/qml/controls/LabelText.qml
    src/qml/controls/Glyph.qml
    src/qml/controls/Panel.qml
    src/qml/controls/Badge.qml
    src/qml/controls/SearchField.qml
    src/qml/controls/SelectBox.qml
    src/qml/controls/SettingRow.qml
    src/qml/controls/EmptyState.qml
    src/qml/controls/Sparkline.qml
    src/qml/controls/SnapshotArtwork.qml
    src/qml/controls/FolderCard.qml
    src/qml/preview/UiPreviewState.qml
    src/qml/dialogs/PreviewDialog.qml
    src/qml/effects/AmbientBackground.qml
)

foreach(qml_file IN LISTS folder_snap_qml_files)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()

set_source_files_properties(src/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(folder_snap_ui STATIC)
qt_add_qml_module(
    folder_snap_ui
    URI FolderSnap
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/FolderSnap"
    QML_FILES ${folder_snap_qml_files}
    SOURCES src/AppState.h src/AppState.cpp
)
target_include_directories(folder_snap_ui PUBLIC src)
target_link_libraries(folder_snap_ui PUBLIC Qt6::Quick Qt6::QuickControls2)

qt_add_resources(
    folder_snap_ui app_resources
    PREFIX /
    FILES
        resources/icons/close.svg
        resources/icons/foldersnap-icon.png
        resources/icons/maximize.svg
        resources/icons/minimize.svg
        resources/icons/restore.svg
)
qt_add_shaders(
    folder_snap_ui ambient_shaders
    PREFIX /shaders
    BASE resources/shaders
    FILES resources/shaders/ambient.frag
)

qt_add_executable(
    FolderSnap WIN32
    src/main.cpp
    src/WindowsWindowController.h
    src/WindowsWindowController.cpp
)
target_link_libraries(
    FolderSnap
    PRIVATE
        folder_snap_core
        folder_snap_ui
        folder_snap_uiplugin
        Qt6::Concurrent
        Qt6::Network
        Qt6::Widgets
        ZLIB::ZLIB
)

if(WIN32)
    target_sources(FolderSnap PRIVATE resources/app.rc)
    target_include_directories(FolderSnap PRIVATE resources/icons)
    target_link_libraries(
        FolderSnap
        PRIVATE advapi32 dwmapi gdi32 ole32 shell32 user32
    )
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS QuickTest Test)

    foreach(core_test IN ITEMS paths ignore domain storage)
        qt_add_executable(tst_${core_test} tests/tst_${core_test}.cpp)
        target_link_libraries(tst_${core_test} PRIVATE folder_snap_core Qt6::Test)
        target_compile_definitions(tst_${core_test} PRIVATE
            FOLDERSNAP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        add_test(NAME ${core_test} COMMAND tst_${core_test})
    endforeach()

    qt_add_executable(tst_appstate tests/tst_appstate.cpp)
    target_link_libraries(tst_appstate PRIVATE folder_snap_ui Qt6::Test)
    add_test(NAME appstate COMMAND tst_appstate)

    qt_add_executable(tst_ui tests/quicktest.cpp)
    target_link_libraries(
        tst_ui
        PRIVATE folder_snap_ui folder_snap_uiplugin Qt6::QuickTest
    )
    target_compile_definitions(
        tst_ui
        PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/qml"
    )
    add_test(NAME ui COMMAND tst_ui -platform offscreen)

    if(WIN32)
        qt_add_executable(
            tst_window
            tests/tst_window.cpp
            src/WindowsWindowController.cpp
            src/WindowsWindowController.h
        )
        target_link_libraries(
            tst_window
            PRIVATE
                folder_snap_ui
                folder_snap_uiplugin
                Qt6::Test
                dwmapi
                gdi32
                psapi
                user32
        )
        add_test(NAME window COMMAND tst_window)
        set_tests_properties(window PROPERTIES LABELS desktop TIMEOUT 90)
    endif()
endif()
