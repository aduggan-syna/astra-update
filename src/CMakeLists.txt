include(ExternalProject)

ExternalProject_Add(cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.0.0
    PREFIX ${CMAKE_BINARY_DIR}/cxxopts
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/cxxopts
)

add_executable(astra-update main.cpp)
add_dependencies(astra-update astraupdate)
add_dependencies(astra-update cxxopts)

include_directories(../include)
include_directories(${CMAKE_BINARY_DIR}/cxxopts/include)

target_link_libraries(astra-update astraupdate)