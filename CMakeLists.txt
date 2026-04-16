cmake_minimum_required(VERSION 3.20)
project(GameBoyEmu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(gbemu ${SOURCES})

target_include_directories(gbemu PRIVATE src)