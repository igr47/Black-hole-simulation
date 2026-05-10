cmake_minimum_required(VERSION 3.10)
project(BlackHole)

set(CMAKE_CXX_STANDARD 14)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)

add_executable(blackhole blackhole.cpp)

target_link_libraries(blackhole 
    OpenGL::GL 
    glfw 
    GLEW::GLEW 
    glm::glm
)
