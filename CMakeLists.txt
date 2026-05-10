cmake_minimum_required(VERSION 3.16)
project(BlackHole VERSION 1.0.0 LANGUAGES CXX C)   # <-- Add "C" language

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# Set GLAD paths
set(GLAD_BASE_DIR ${CMAKE_SOURCE_DIR}/external/glad_generated/glad)
set(GLAD_INCLUDE_DIR ${GLAD_BASE_DIR}/include)
set(GLAD_SOURCE_FILE ${GLAD_BASE_DIR}/src/glad.c)

# Verify files exist
if(NOT EXISTS ${GLAD_SOURCE_FILE})
    message(FATAL_ERROR "GLAD source file not found at: ${GLAD_SOURCE_FILE}")
endif()

if(NOT EXISTS ${GLAD_INCLUDE_DIR}/glad/glad.h)
    message(FATAL_ERROR "GLAD header not found at: ${GLAD_INCLUDE_DIR}/glad/glad.h")
endif()

# Create a static library for GLAD (this ensures it's compiled separately)
add_library(glad STATIC ${GLAD_SOURCE_FILE})
target_include_directories(glad PUBLIC ${GLAD_INCLUDE_DIR})

# Create executable
add_executable(blackhole blackhole.cpp)

# Link libraries - glad must come first
target_link_libraries(blackhole
    glad               # Now glad is a proper library target
    OpenGL::GL
    glfw
    glm::glm
    dl
)

# Also include the GLAD headers for atom
target_include_directories(blackhole PRIVATE ${GLAD_INCLUDE_DIR})

# Add compiler flags
target_compile_options(blackhole PRIVATE -fPIC)

# For debugging
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(blackhole PRIVATE -g -O0)
endif()

message(STATUS "==========================================")
message(STATUS "GLAD configuration:")
message(STATUS "  Include directory: ${GLAD_INCLUDE_DIR}")
message(STATUS "  Source file: ${GLAD_SOURCE_FILE}")
message(STATUS "==========================================")
