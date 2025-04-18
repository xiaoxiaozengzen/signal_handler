cmake_minimum_required(VERSION 3.21)
project(signal_handler)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(HAS_SIGACTION ON)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(signal_handler SHARED ${SOURCES})
target_include_directories(
  signal_handler PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                        $<INSTALL_INTERFACE:include>)
if(HAS_SIGACTION)
  target_compile_definitions(signal_handler PUBLIC HAS_SIGACTION)
endif()

add_executable(example example/example.cpp)
target_include_directories(
  example PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                 $<INSTALL_INTERFACE:include>)
target_link_libraries(example PRIVATE signal_handler pthread)
target_compile_definitions(example PRIVATE HAS_SIGACTION)