add_executable(astra-update main.cpp)
add_dependencies(astra-update astraupdate)

include_directories(../include)

target_link_libraries(astra-update astraupdate)