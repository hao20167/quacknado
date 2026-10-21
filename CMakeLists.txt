cmake_minimum_required(VERSION 4.4)
project(quacknado LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

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


set(TRIANGLE_SHADER "${CMAKE_SOURCE_DIR}/shaders/triangle.metal")
set(TRIANGLE_AIR "${CMAKE_CURRENT_BINARY_DIR}/triangle.air")
set(TRIANGLE_METALLIB "${CMAKE_CURRENT_BINARY_DIR}/triangle.metallib")
add_custom_command(
  OUTPUT "${TRIANGLE_METALLIB}"
  COMMAND xcrun -sdk macosx metal
    -c "${TRIANGLE_SHADER}"
    -o "${TRIANGLE_AIR}"
  COMMAND xcrun -sdk macosx metallib
    "${TRIANGLE_AIR}"
    -o "${TRIANGLE_METALLIB}"
  DEPENDS "${TRIANGLE_SHADER}"
  VERBATIM
)

set(SHADER_DESTINATION "$<TARGET_FILE_DIR:quacknado>")

add_custom_target(copy_triangle_shader ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${TRIANGLE_METALLIB}"
    "$<TARGET_FILE_DIR:quacknado>/triangle.metallib"
  DEPENDS "${TRIANGLE_METALLIB}"
  VERBATIM
)

add_dependencies(copy_triangle_shader quacknado)

target_link_libraries(
  quacknado 
  PRIVATE 
    SDL3::SDL3 
    "-framework Metal" 
    "-framework Foundation" 
    "-framework QuartzCore"
)

