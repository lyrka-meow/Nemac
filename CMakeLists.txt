cmake_minimum_required(VERSION 3.16)
project(Nemac VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NEMAC_SOURCES
    src/core/main.cpp
    src/core/launcher.cpp
    src/compositor/compositor.cpp
    src/compositor/texture.cpp
    src/compositor/render.cpp
    src/wm/wm.cpp
    src/monitor/monitor.cpp
    src/gpu/gpu.cpp
    src/wallpaper/wallpaper.cpp
    src/updater/updater.cpp
)

add_executable(nemac ${NEMAC_SOURCES})

target_link_libraries(nemac PRIVATE
    X11
    Xcomposite
    Xdamage
    Xfixes
    Xinerama
    GL
    GLEW
    Imlib2
)

target_compile_options(nemac PRIVATE
    -Wall -Wextra -O2
)

install(TARGETS nemac DESTINATION bin)

add_subdirectory(panel)
