cmake_minimum_required(VERSION 3.21)
project(NyakoMCL LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Network Concurrent)
qt_standard_project_setup(REQUIRES 6.8)
# MinGW 随附的 zlib 与当前编译器配套，优先显式定位，避免混入其他工具链的库。
if(MINGW)
    get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(MINGW_ROOT "${MINGW_BIN}" DIRECTORY)
    if(EXISTS "${MINGW_ROOT}/x86_64-w64-mingw32/include/zlib.h")
        set(ZLIB_INCLUDE_DIR "${MINGW_ROOT}/x86_64-w64-mingw32/include")
        set(ZLIB_LIBRARY "${MINGW_ROOT}/x86_64-w64-mingw32/lib/libz.a")
    endif()
endif()
find_package(ZLIB REQUIRED)
add_executable(NyakoMCL WIN32
    src/app/main.cpp
    src/app/cli.cpp
    src/app/cli.h
    src/app/launcher_controller.cpp
    src/app/launcher_controller.h
    src/instance/instance_store.cpp
    src/instance/instance_store.h
    src/install/launcher.cpp
    src/install/launcher.h
    src/version/version_model.cpp
    src/version/version_model.h
    src/version/version_status.cpp
    src/version/version_status.h
    src/java/java_discovery.cpp
    src/java/java_discovery.h
    src/java/java_runtime_provider.cpp
    src/java/java_runtime_provider.h
)
# 以 src 为私有包含根保持模块引用稳定；QML 资源别名保留模块入口 Main，不受磁盘目录布局影响。
target_include_directories(NyakoMCL PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# qt_add_qml_module(NyakoMCL URI NyakoMCL VERSION 1.0 OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_QML" QML_FILES qml/Main.qml)
# 任务页与主页面一起打包，部署后仍可从任意来源进入内部任务导航。
set(LAUNCHER_QML_FILES
    qml/Main.qml qml/LaunchPage.qml qml/DownloadPage.qml qml/InstancesPage.qml
    qml/SettingsPage.qml qml/InstanceSettingsWindow.qml qml/TasksPage.qml qml/TaskCard.qml
    qml/ActionButton.qml qml/SectionTitle.qml qml/FieldTitle.qml)
foreach(QML_FILE IN LISTS LAUNCHER_QML_FILES)
    get_filename_component(QML_BASENAME "${QML_FILE}" NAME)
    set_source_files_properties("${QML_FILE}" PROPERTIES QT_RESOURCE_ALIAS "${QML_BASENAME}")
endforeach()
qt_add_qml_module(NyakoMCL URI NyakoMCL VERSION 1.0 OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_QML" QML_FILES ${LAUNCHER_QML_FILES})
target_link_libraries(NyakoMCL PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Concurrent ZLIB::ZLIB)
