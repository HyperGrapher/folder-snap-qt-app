cmake_minimum_required(VERSION 3.21)
project(Aura VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickControls2 ShaderTools)
qt_standard_project_setup(REQUIRES 6.11)
include(CTest)

set(aura_qml
    src/qml/Main.qml src/qml/Theme.qml src/qml/MotionPolicy.qml
    src/qml/shell/TitleBar.qml
    src/qml/navigation/Sidebar.qml src/qml/navigation/PageHost.qml src/qml/navigation/PageFrame.qml
    src/qml/controls/ActionButton.qml src/qml/controls/WindowButton.qml
    src/qml/controls/NavigationButton.qml src/qml/controls/ToggleSwitch.qml
    src/qml/controls/DemoCard.qml src/qml/controls/ProgressRing.qml
    src/qml/controls/StatusCard.qml src/qml/controls/PageHeading.qml src/qml/controls/BodyText.qml
    src/qml/pages/OverviewPage.qml src/qml/pages/CollectionPage.qml
    src/qml/pages/ActivityPage.qml src/qml/pages/SettingsPage.qml
    src/qml/effects/AmbientBackground.qml
)
foreach(qml_file IN LISTS aura_qml)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
set_source_files_properties(src/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_library(aura_ui STATIC)
qt_add_qml_module(aura_ui URI Aura VERSION 1.0 RESOURCE_PREFIX /qt/qml OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Aura
    QML_FILES ${aura_qml} SOURCES src/AppState.h src/AppState.cpp)
target_include_directories(aura_ui PUBLIC src)
target_link_libraries(aura_ui PUBLIC Qt6::Quick Qt6::QuickControls2)
qt_add_resources(aura_ui icons PREFIX / FILES
    resources/icons/overview.svg resources/icons/collection.svg
    resources/icons/activity.svg resources/icons/settings.svg
    resources/icons/arrow.svg resources/icons/spark.svg
    resources/icons/minimize.svg resources/icons/maximize.svg
    resources/icons/restore.svg resources/icons/close.svg)
qt_add_shaders(aura_ui ambient_shaders PREFIX /shaders BASE resources/shaders
    FILES resources/shaders/ambient.frag)
qt_add_executable(appAura WIN32 src/main.cpp src/WindowsWindowController.h src/WindowsWindowController.cpp)
target_link_libraries(appAura PRIVATE aura_ui aura_uiplugin)
if(WIN32)
    target_link_libraries(appAura PRIVATE dwmapi user32 gdi32)
endif()
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)
    qt_add_executable(tst_appstate tests/tst_appstate.cpp)
    target_link_libraries(tst_appstate PRIVATE aura_ui Qt6::Test)
    add_test(NAME appstate COMMAND tst_appstate)
    qt_add_executable(tst_ui tests/quicktest.cpp)
    target_link_libraries(tst_ui PRIVATE aura_ui aura_uiplugin Qt6::QuickTest)
    target_compile_definitions(tst_ui PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/qml")
    add_test(NAME ui COMMAND tst_ui -platform offscreen)
    if(WIN32)
        qt_add_executable(tst_window tests/tst_window.cpp src/WindowsWindowController.cpp src/WindowsWindowController.h)
        target_link_libraries(tst_window PRIVATE aura_ui aura_uiplugin Qt6::Test dwmapi user32 gdi32 psapi)
        add_test(NAME window COMMAND tst_window)
        set_tests_properties(window PROPERTIES LABELS desktop TIMEOUT 90)
    endif()
endif()
