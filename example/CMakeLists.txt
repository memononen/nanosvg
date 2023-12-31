cmake_minimum_required(VERSION 3.10)

project(NanoSVG-examples C)

find_package(NanoSVG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3)

add_executable(example1 example1.c)
target_link_libraries(example1 OpenGL::GL glfw NanoSVG::nanosvg)

add_executable(example2 example2.c)
target_link_libraries(example2 NanoSVG::nanosvgrast)