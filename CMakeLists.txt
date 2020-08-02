cmake_minimum_required(VERSION 3.7)

project(WaveTableFactory)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include("CMakeLists.deps.txt")

add_executable(WaveTableFactory
  sources/main.cpp
  sources/application.h
  sources/application.cpp
  sources/wave_processor.h
  sources/wave_processor.cpp
  sources/wavetable.h
  sources/wavetable.cpp)
target_compile_definitions(WaveTableFactory PRIVATE
  "PROJECT_NAME=\"${PROJECT_NAME}\"")
target_link_libraries(WaveTableFactory PRIVATE
  Qt5::Widgets Qt5::DataVisualization PkgConfig::octinterp sys::qscintilla)
