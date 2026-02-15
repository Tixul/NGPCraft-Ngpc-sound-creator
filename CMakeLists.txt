cmake_minimum_required(VERSION 3.20)

project(NgpcSoundCreator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(NGPCSC_BUILD_APP "Build GUI app" ON)

find_package(Qt6 COMPONENTS Widgets Multimedia REQUIRED)

add_subdirectory(core)

if(NGPCSC_BUILD_APP)
    add_subdirectory(app)
endif()
