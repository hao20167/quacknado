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
add_subdirectory(external/JoltPhysics/Build ${CMAKE_BINARY_DIR}/jolt-build)

# target, requirement
add_executable(quacknado ${SRCS})

# copy assets to build folder
add_custom_target(copy_models ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
    "${CMAKE_SOURCE_DIR}/assets/models"
    "$<TARGET_FILE_DIR:quacknado>/assets/models"
  VERBATIM
)
add_dependencies(copy_models quacknado)

# where to lookup for headers
target_include_directories(
  quacknado
  PRIVATE 
    ${CMAKE_SOURCE_DIR}/external/metal-cpp
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/cgltf
    ${CMAKE_SOURCE_DIR}/external/stb
)


set(SHADER "${CMAKE_SOURCE_DIR}/shaders/object.metal")
set(AIR "${CMAKE_CURRENT_BINARY_DIR}/object.air")
set(METALLIB "${CMAKE_CURRENT_BINARY_DIR}/object.metallib")
add_custom_command(
  OUTPUT "${METALLIB}"
  COMMAND xcrun -sdk macosx metal
    "-fmodules-cache-path=${CMAKE_CURRENT_BINARY_DIR}/metal-module-cache"
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
    "$<TARGET_FILE_DIR:quacknado>/object.metallib"
  DEPENDS "${METALLIB}"
  VERBATIM
)

add_dependencies(copy_shader quacknado)

target_link_libraries(
  quacknado 
  PRIVATE 
    SDL3::SDL3 
    Jolt
    "-framework Metal" 
    "-framework Foundation" 
    "-framework QuartzCore"
)
