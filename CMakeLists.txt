cmake_minimum_required(VERSION 3.20)

project(tensor_planner LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(tensor_planner PUBLIC cxx_std_20)

install(TARGETS tensor_planner
  EXPORT tensor_plannerTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
  include/tensor_planner.h
  include/tensor_planner.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT tensor_plannerTargets
  FILE tensor_plannerTargets.cmake
  NAMESPACE tensor_planner::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tensor_planner
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tensor_plannerConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/tensor_plannerConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tensor_planner
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/tensor_plannerConfigVersion.cmake
  VERSION 0.1.0
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/tensor_plannerConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/tensor_plannerConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tensor_planner
)

add_executable(tensor_planner_solver_smoke tests/solver_smoke.cpp)
target_link_libraries(tensor_planner_solver_smoke PRIVATE tensor_planner)

add_executable(tensor_planner_logistics_smoke tests/logistics_smoke.cpp)
target_link_libraries(tensor_planner_logistics_smoke PRIVATE tensor_planner)

add_executable(tensor_planner_crafting_smoke tests/crafting_smoke.cpp)
target_link_libraries(tensor_planner_crafting_smoke PRIVATE tensor_planner)

add_executable(tensor_planner_cpp_fluent_smoke tests/cpp_fluent_smoke.cpp)
target_link_libraries(tensor_planner_cpp_fluent_smoke PRIVATE tensor_planner)

add_executable(tensor_planner_guidance_experiment tests/guidance_experiment.cpp)
target_link_libraries(tensor_planner_guidance_experiment PRIVATE tensor_planner)

add_executable(tensor_planner_crafting_guidance_experiment tests/crafting_guidance_experiment.cpp)
target_link_libraries(tensor_planner_crafting_guidance_experiment PRIVATE tensor_planner)

enable_testing()
add_test(NAME tensor_planner_solver_smoke COMMAND tensor_planner_solver_smoke)
add_test(NAME tensor_planner_logistics_smoke COMMAND tensor_planner_logistics_smoke)
add_test(NAME tensor_planner_crafting_smoke COMMAND tensor_planner_crafting_smoke)
add_test(NAME tensor_planner_cpp_fluent_smoke COMMAND tensor_planner_cpp_fluent_smoke)
add_test(NAME tensor_planner_guidance_experiment COMMAND tensor_planner_guidance_experiment)
add_test(NAME tensor_planner_crafting_guidance_experiment COMMAND tensor_planner_crafting_guidance_experiment)
