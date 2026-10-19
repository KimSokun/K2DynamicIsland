cmake_minimum_required(VERSION 3.16)
project(K2DynamicIsland VERSION 1.1.1 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Directories
set(WEBVIEW2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/packages/build/native/include")
set(WEBVIEW2_DLL "${CMAKE_CURRENT_SOURCE_DIR}/packages/build/native/x64/WebView2Loader.dll")
set(PACKAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/packages")
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${WEBVIEW2_INCLUDE_DIR}"
    "${PACKAGES_DIR}"
)

# Common Win32 system libraries
set(WIN32_LIBS
    ole32
    shell32
    shlwapi
    user32
    gdi32
    advapi32
    dwmapi
    uuid
    comdlg32
    winhttp
    wintrust
    crypt32
)

# Application Executable
add_executable(K2DynamicIsland WIN32
    src/main.cpp
    src/app_window.h
    src/app_window.cpp
    src/webview_manager.h
    src/webview_manager.cpp
    src/bridge_dispatcher.h
    src/bridge_dispatcher.cpp
    src/storage_manager.h
    src/storage_manager.cpp
    src/system_stats.h
    src/system_stats.cpp
    src/media_controller.h
    src/media_controller.cpp
    src/update_manager.h
    src/update_manager.cpp
    src/com_callbacks.h
    src/resource.rc
)

target_link_libraries(K2DynamicIsland PRIVATE ${WIN32_LIBS})

# Post-build: Copy assets and WebView2Loader.dll
add_custom_command(TARGET K2DynamicIsland POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBVIEW2_DLL}"
        "$<TARGET_FILE_DIR:K2DynamicIsland>/WebView2Loader.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSETS_DIR}"
        "$<TARGET_FILE_DIR:K2DynamicIsland>/assets"
    COMMENT "Copying assets and WebView2 runtime loader..."
)

# Test Executable
enable_testing()

add_executable(K2DynamicIslandTests
    tests/unit_tests.cpp
    src/storage_manager.cpp
    src/bridge_dispatcher.cpp
    src/system_stats.cpp
    src/media_controller.cpp
    src/update_manager.cpp
)

target_link_libraries(K2DynamicIslandTests PRIVATE ${WIN32_LIBS})

add_test(NAME UnitTests COMMAND K2DynamicIslandTests)
