cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(astra-update)

include_directories(include)

add_subdirectory(lib)
add_subdirectory(src)