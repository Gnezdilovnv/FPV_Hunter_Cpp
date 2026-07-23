cmake_minimum_required(VERSION 3.10)
project(FPV_Hunter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Multimedia TextToSpeech)
find_package(PkgConfig REQUIRED)
pkg_check_modules(IIO REQUIRED libiio)

if(NOT IIO_FOUND)
    message(FATAL_ERROR "libiio not found")
endif()

add_executable(FPV_Hunter main.cpp)

target_link_libraries(FPV_Hunter
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Multimedia
    Qt5::TextToSpeech
    ${IIO_LIBRARIES}
)

target_include_directories(FPV_Hunter PRIVATE
    ${IIO_INCLUDE_DIRS}
)
