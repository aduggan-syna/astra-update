include(ExternalProject)
include(FetchContent)

ExternalProject_Add(libusb
    GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
    GIT_TAG v1.0.26
    PREFIX ${CMAKE_BINARY_DIR}/libusb
    CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF
               -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libusb
               -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
)

ExternalProject_Add(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.7.0
    PREFIX ${CMAKE_BINARY_DIR}/yaml-cpp
    CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF
               -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/yaml-cpp
               -DYAML_CPP_BUILD_TESTS=OFF
)

file(GLOB SRC astra_boot_image.cpp
                astra_device.cpp
                astra_update.cpp
                boot_image_collection.cpp
                usb_transport.cpp
)
add_library(astraupdate STATIC ${SRC})

add_dependencies(astraupdate yaml-cpp)
add_dependencies(astraupdate libusb)

message(CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(PLATFORM_LINK_LIBRARIES udev)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(PLATFORM_LINK_LIBRARIES objc "-framework Foundation" "-framework IOKit" "-framework Security")
endif()

target_include_directories(astraupdate PRIVATE ${CMAKE_BINARY_DIR}/yaml-cpp/include)

target_link_libraries(astraupdate PRIVATE ${CMAKE_BINARY_DIR}/libusb/lib/libusb-1.0${CMAKE_STATIC_LIBRARY_SUFFIX} ${PLATFORM_LINK_LIBRARIES})
target_link_libraries(astraupdate PRIVATE ${CMAKE_BINARY_DIR}/yaml-cpp/lib/libyaml-cpp${CMAKE_STATIC_LIBRARY_SUFFIX})

# Link the filesystem library if necessary
if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_libraries(astraupdate PRIVATE stdc++fs)
endif()