cmake_minimum_required(VERSION 3.16)
project(nemac-panel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)

set(PANEL_SOURCES
    main.cpp
    PanelWindow.cpp
    widgets/Clock.cpp
    widgets/MediaPlayer.cpp
    widgets/FileBrowser.cpp
    widgets/WallpaperPicker.cpp
    widgets/AppManager.cpp
    widgets/Terminal.cpp
)

add_executable(nemac-panel ${PANEL_SOURCES})

target_include_directories(nemac-panel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DBUS_INCLUDE_DIRS}
)

target_link_libraries(nemac-panel PRIVATE
    Qt6::Widgets
    X11
    ${DBUS_LIBRARIES}
)

target_compile_options(nemac-panel PRIVATE
    -Wall -Wextra -O2
    ${DBUS_CFLAGS_OTHER}
)

install(TARGETS nemac-panel DESTINATION bin)
