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


add_custom_target(copy_models ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
    "${CMAKE_SOURCE_DIR}/assets/models" "$<TARGET_FILE_DIR:quacknado>/assets/models"
  VERBATIM)
add_dependencies(copy_models quacknado)
add_dependencies(render_regression copy_shader)
