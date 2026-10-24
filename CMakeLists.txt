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


set(SHADER "${CMAKE_SOURCE_DIR}/shaders/duck.metal")
set(AIR "${CMAKE_CURRENT_BINARY_DIR}/duck.air")
set(METALLIB "${CMAKE_CURRENT_BINARY_DIR}/duck.metallib")
add_custom_command(
  OUTPUT "${METALLIB}"
  COMMAND xcrun -sdk macosx metal
    -c "${SHADER}"
    -o "${AIR}"
  COMMAND xcrun -sdk macosx metallib
    "${AIR}"
    -o "${METALLIB}"
  DEPENDS "${SHADER}"
  VERBATIM
)

set(SHADER_DESTINATION "$<TARGET_FILE_DIR:quacknado>")

add_custom_target(copy_shader ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${METALLIB}"
    "$<TARGET_FILE_DIR:quacknado>/duck.metallib"
  DEPENDS "${METALLIB}"
  VERBATIM
)

add_dependencies(copy_shader quacknado)

target_link_libraries(
  quacknado 
  PRIVATE 
    SDL3::SDL3 
    "-framework Metal" 
    "-framework Foundation" 
    "-framework QuartzCore"
)

