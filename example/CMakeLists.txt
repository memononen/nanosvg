
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/example)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})

#add_executable(dump dump.c)
add_executable(example1 example1.c)
add_executable(example2 example2.c)

#target_link_libraries(dump m GL ${GLFW_STATIC_LIBRARIES})
target_link_libraries(example1 m GL ${GLFW_STATIC_LIBRARIES})
target_link_libraries(example2 m GL ${GLFW_STATIC_LIBRARIES})

