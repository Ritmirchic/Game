cmake_minimum_required(VERSION 3.16)
project(WallPulse VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WALLPULSE_SOURCES
    src/common.cpp
    src/config.cpp
    src/desktop_host.cpp
    src/smart_pause.cpp
    src/media_tools.cpp
    src/video_wallpaper.cpp
    src/studio_ui.cpp
    src/main.cpp
    resources/wallpulse.rc
)

add_executable(WallPulse WIN32 ${WALLPULSE_SOURCES})
target_include_directories(WallPulse PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(WallPulse PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(WallPulse PRIVATE /utf-8 /W4 /O2 /EHsc)
    set_property(TARGET WallPulse PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(WallPulse PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -municode)
    target_link_options(WallPulse PRIVATE -municode -Wl,--subsystem,windows -s)
endif()

target_link_libraries(WallPulse PRIVATE
    user32 gdi32 advapi32 shell32 ole32 oleaut32 uuid shlwapi comdlg32 dwmapi
    d3d11 dxgi d3dcompiler d2d1 dwrite mfplat mf mfreadwrite mfuuid propsys
)
