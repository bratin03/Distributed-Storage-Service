cmake_minimum_required(VERSION 3.10)
project(DistributedStorageService)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

# Global compiler flags (common to all build types)
add_compile_options(-Wall)

# Build type specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Build type: Debug")
    add_compile_options(-g -O0)  # Debug symbols, no optimization
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Build type: Release")
    add_compile_options(-O3 -DNDEBUG)  # Maximum optimization, disable asserts
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Build type: Release with Debug Info")
    add_compile_options(-O2 -g)  # Moderate optimization with debug symbols
elseif(CMAKE_BUILD_TYPE STREQUAL "Profile")
    message(STATUS "Build type: Profile")
    add_compile_options(-O2 -g -pg)  # Optimization with profiling
endif()

# Find dependencies
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/utils/libraries/cpp-httplib
    ${CMAKE_SOURCE_DIR}/utils/libraries/jwt-cpp/include
    ${OPENSSL_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
)

# Add subdirectories
add_subdirectory(utils/libraries)
add_subdirectory(src)
