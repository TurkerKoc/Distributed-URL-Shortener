cmake_minimum_required(VERSION 3.16)
project(cbdp LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(protos)
add_subdirectory(src)