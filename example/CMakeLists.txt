
option (BUILD_FUZZ "Build binaries with tooling for fuzzing." OFF)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/example)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})
add_compile_options(-O2 -Wall)

add_executable(dump dump.c)
add_executable(example1 example1.c)
add_executable(example2 example2.c)

target_link_libraries(dump m )
target_link_libraries(example1 m GL ${GLFW_STATIC_LIBRARIES})
target_link_libraries(example2 m GL ${GLFW_STATIC_LIBRARIES})

#
# if fuzzing is enabled
#
if (BUILD_FUZZ)
  include (ExternalProject)
  #
  # Afl
  #
  ExternalProject_Add (aflX
    PREFIX aflX
    URL http://lcamtuf.coredump.cx/afl/releases/afl-2.52b.tgz
    CONFIGURE_COMMAND ""
    BUILD_COMMAND "make"
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
    )
  ExternalProject_Get_Property (aflX source_dir)
  set (AFL_DIR "${source_dir}")
  message ("American Fuzzy Lop in ${AFL_DIR}")
  #set (CMAKE_C_COMPILER ${AFL_DIR}/afl-gcc)
  #set (CMAKE_CXX_COMPILER ${AFL_DIR}/afl-g++)
  set (CMAKE_C_COMPILER ${AFL_DIR}/afl-clang)
  set (CMAKE_CXX_COMPILER ${AFL_DIR}/afl-clang++)

  add_dependencies (example1 aflX)
  add_dependencies (example2 aflX)

  add_custom_target (fuzz
    COMMAND mkdir -p fuzz-data
    COMMAND cp nano.svg fuzz-data/
    COMMAND ${AFL_DIR}/afl-fuzz -i fuzz-data -o fuzz-out -x fuzz.dict $<TARGET_FILE:example2> @@ ''
    )
  add_dependencies (fuzz example1)

endif (BUILD_FUZZ)
