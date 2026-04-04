cmake_minimum_required(VERSION 3.16)
project(Nemac VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE2 REQUIRED freetype2)
pkg_check_modules(DBUS REQUIRED dbus-1)

set(NEMAC_SOURCES
    src/core/main.cpp
    src/compositor/compositor.cpp
    src/wm/wm.cpp
    src/monitor/monitor.cpp
    src/gpu/gpu.cpp
    src/wallpaper/wallpaper.cpp
    src/updater/updater.cpp
)

add_executable(nemac ${NEMAC_SOURCES})

target_include_directories(nemac PRIVATE
    ${FREETYPE2_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
)

target_link_libraries(nemac PRIVATE
    X11
    Xcomposite
    Xdamage
    Xfixes
    Xinerama
    Xft
    GL
    GLEW
    Imlib2
    ${DBUS_LIBRARIES}
)

target_compile_options(nemac PRIVATE
    -Wall -Wextra -O2
    ${FREETYPE2_CFLAGS_OTHER}
    ${DBUS_CFLAGS_OTHER}
)

install(TARGETS nemac DESTINATION bin)
install(DIRECTORY assets/wallpapers/ DESTINATION share/nemac/wallpapers)

add_subdirectory(panel)
