cmake_minimum_required(VERSION 3.16)
project(nemac-panel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_add_resources(RESOURCES resources.qrc)

add_executable(nemac-panel
    src/main.cpp
    src/panel.cpp
    ${RESOURCES}
)

target_link_libraries(nemac-panel PRIVATE
    Qt6::Quick
    xcb
)

install(TARGETS nemac-panel DESTINATION bin)
