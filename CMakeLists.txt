cmake_minimum_required(VERSION 4.4)
project(quacknado LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

file(
  GLOB_RECURSE 
  SRCS 
  CONFIGURE_DEPENDS 
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# target, requirement
add_executable(quacknado ${SRCS})

# where to lookup for headers
target_include_directories(
  quacknado
  PRIVATE 
    ${CMAKE_SOURCE_DIR}/external/metal-cpp
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
  quacknado 
  PRIVATE 
    SDL3::SDL3 
    "-framework Metal" 
    "-framework Foundation" 
    "-framework QuartzCore"
)

