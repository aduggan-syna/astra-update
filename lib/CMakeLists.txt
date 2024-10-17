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

file(GLOB SRC *.cpp)
add_library(astraupdate STATIC ${SRC})

message(CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(PLATFORM_LINK_LIBRARIES udev)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(PLATFORM_LINK_LIBRARIES objc "-framework Foundation" "-framework IOKit" "-framework Security")
endif()

target_link_libraries(astraupdate ${CMAKE_BINARY_DIR}/libusb/lib/libusb-1.0${CMAKE_STATIC_LIBRARY_SUFFIX} ${PLATFORM_LINK_LIBRARIES})