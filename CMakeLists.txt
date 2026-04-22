cmake_minimum_required(VERSION 3.20)

project(tensor_planner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tensor_planner SHARED
  src/api.cpp
  src/domain.cpp
  src/search.cpp
  src/state.cpp
)

target_include_directories(tensor_planner
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(tensor_planner PUBLIC cxx_std_20)

add_executable(tensor_planner_solver_smoke tests/solver_smoke.cpp)
target_link_libraries(tensor_planner_solver_smoke PRIVATE tensor_planner)

add_executable(tensor_planner_runtime_example examples/runtime_solver_example.cpp)
target_link_libraries(tensor_planner_runtime_example PRIVATE tensor_planner)

add_executable(tensor_planner_large_capra_example examples/large_capra_example.cpp)
target_link_libraries(tensor_planner_large_capra_example PRIVATE tensor_planner)

add_executable(tensor_planner_capra_benchmark examples/capra_benchmark.cpp)
target_link_libraries(tensor_planner_capra_benchmark PRIVATE tensor_planner)

add_executable(tensor_planner_logistics_benchmark examples/logistics_benchmark.cpp)
target_link_libraries(tensor_planner_logistics_benchmark PRIVATE tensor_planner)

add_executable(tensor_planner_crafting_benchmark examples/crafting_benchmark.cpp)
target_link_libraries(tensor_planner_crafting_benchmark PRIVATE tensor_planner)

add_executable(tensor_planner_logistics_smoke tests/logistics_smoke.cpp)
target_link_libraries(tensor_planner_logistics_smoke PRIVATE tensor_planner)

enable_testing()
add_test(NAME tensor_planner_solver_smoke COMMAND tensor_planner_solver_smoke)
add_test(NAME tensor_planner_logistics_smoke COMMAND tensor_planner_logistics_smoke)
