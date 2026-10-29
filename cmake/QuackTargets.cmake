# Instrument project targets, never the vendored dependencies. Link flags propagate
# from static libraries so each executable receives the required runtime.
function(quack_sanitize target)
  if(QUACK_SANITIZE)
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  endif()
endfunction()

set(QUACK_SIM_SOURCES
  src/simulation/env/tornado.cpp
  src/simulation/simulation.cpp
  src/simulation/spawn_position.cpp
  src/simulation/physics/physics_world.cpp)
function(quack_simulation target)
  add_library(${target} STATIC ${QUACK_SIM_SOURCES})
  target_include_directories(${target} PUBLIC src)
  target_link_libraries(${target} PUBLIC SDL3::SDL3 Jolt)
endfunction()
quack_simulation(quack_sim)
quack_sanitize(quack_sim)

add_library(quack_render STATIC
  src/assets/glb_asset.cpp
  src/draw/model_draw.cpp src/draw/object_draw.cpp src/draw/tornado_draw.cpp
  src/renderer/metal_renderer.cpp src/renderer/metal_cpp_impl.cpp)
target_include_directories(quack_render PUBLIC src external/metal-cpp
  PRIVATE external/cgltf external/stb)
target_link_libraries(quack_render PUBLIC SDL3::SDL3 Jolt
  "-framework Metal" "-framework Foundation" "-framework QuartzCore")
quack_sanitize(quack_render)

add_executable(quacknado src/main.cpp src/app/application.cpp)
target_link_libraries(quacknado PRIVATE quack_sim quack_render)
target_compile_definitions(quacknado PRIVATE QUACK_BUILD_TYPE="$<CONFIG>")
quack_sanitize(quacknado)

add_library(quack_reference STATIC benchmarks/tornado_reference.cpp)
target_link_libraries(quack_reference PUBLIC quack_sim)
quack_sanitize(quack_reference)
add_executable(quack_bench benchmarks/main.cpp benchmarks/gpu.cpp benchmarks/glb.cpp)
target_include_directories(quack_bench PRIVATE benchmarks)
target_link_libraries(quack_bench PRIVATE quack_reference quack_render)
quack_sanitize(quack_bench)

# A separate uninstrumented simulation is necessary only in sanitizer builds:
# custom global new and Jolt hooks must not mix with allocator interception.
if(QUACK_SANITIZE)
  quack_simulation(quack_sim_alloc)
  set(QUACK_ALLOC_SIM quack_sim_alloc)
else()
  set(QUACK_ALLOC_SIM quack_sim)
endif()
add_executable(quack_alloc benchmarks/allocations.cpp)
target_link_libraries(quack_alloc PRIVATE ${QUACK_ALLOC_SIM})

add_executable(spawn_tests tests/spawn_tests.cpp)
target_link_libraries(spawn_tests PRIVATE quack_sim)
quack_sanitize(spawn_tests)
add_executable(wind_thread_tests tests/wind_thread_tests.cpp)
target_include_directories(wind_thread_tests PRIVATE benchmarks)
target_link_libraries(wind_thread_tests PRIVATE quack_reference)
quack_sanitize(wind_thread_tests)
add_executable(metal_ptr_tests tests/metal_ptr_tests.cpp)
target_include_directories(metal_ptr_tests PRIVATE src)
quack_sanitize(metal_ptr_tests)
add_executable(render_regression tests/render_regression.cpp)
target_link_libraries(render_regression PRIVATE quack_render)
quack_sanitize(render_regression)

add_executable(env_tests tests/env_tests.cpp)
target_link_libraries(env_tests PRIVATE quack_sim)
quack_sanitize(env_tests)

add_executable(interaction_bench benchmarks/interactions.cpp)
target_link_libraries(interaction_bench PRIVATE quack_sim)
quack_sanitize(interaction_bench)

add_executable(physics_workers_tests tests/physics_workers_tests.cpp)
target_link_libraries(physics_workers_tests PRIVATE quack_sim)
quack_sanitize(physics_workers_tests)

add_executable(fixed_step_tests tests/fixed_step_tests.cpp)
target_include_directories(fixed_step_tests PRIVATE src)
quack_sanitize(fixed_step_tests)
add_executable(collision_stress_bench benchmarks/collision_stress.cpp)
target_link_libraries(collision_stress_bench PRIVATE quack_sim)
quack_sanitize(collision_stress_bench)

add_executable(large_spawn_tests tests/large_spawn_tests.cpp)
target_link_libraries(large_spawn_tests PRIVATE quack_sim)
quack_sanitize(large_spawn_tests)

add_executable(pile_workers_tests tests/pile_workers_tests.cpp)
target_link_libraries(pile_workers_tests PRIVATE quack_sim)
quack_sanitize(pile_workers_tests)
