#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIGURATION="Release"
OUTPUT_ROOT="$SCRIPT_DIR/dist"
TARGETS=""
TARGET_OSES=""
CLEAN_AFTER=1

print_usage() {
  cat <<'USAGE'
Usage:
  sh ./build.sh [-release|-debug] [-os <linux|windows>...] -target <unity|cpp|sharp|jai|unreal>... [-o <output-dir>] [-no-clean]

Examples:
  sh ./build.sh -release -os linux -target unity cpp sharp jai unreal -o ./dist
  sh ./build.sh -release -os windows -target unity cpp sharp unreal -o ./dist
  sh ./build.sh -target unity cpp -os linux windows -o ./dist

Options:
  -release       Build Release artifacts. This is the default.
  -debug         Build Debug artifacts.
  -os            One or more target OS values: linux windows. Defaults to host OS.
  -target        One or more targets: unity cpp sharp jai unreal.
  -o             Output directory. Defaults to ./dist.
  -no-clean      Keep generated build-process files after packaging.
  -h, --help     Show this help.
USAGE
}

fail() {
  printf 'error: %s\n' "$1" >&2
  exit 1
}

info() {
  printf '%s\n' "$1"
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

host_os() {
  os_name=$(uname -s 2>/dev/null || printf unknown)
  case "$os_name" in
    Linux*) printf 'linux\n' ;;
    MINGW*|MSYS*|CYGWIN*) printf 'windows\n' ;;
    *) fail "unsupported host OS: $os_name. Pass -os linux or -os windows." ;;
  esac
}

abs_path() {
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s/%s\n' "$(pwd)" "$1" ;;
  esac
}

append_unique_word() {
  current="$1"
  value="$2"
  for item in $current; do
    [ "$item" = "$value" ] && { printf '%s\n' "$current"; return; }
  done
  printf '%s %s\n' "$current" "$value"
}

append_target() {
  case "$1" in
    unity|cpp|sharp|jai|unreal) TARGETS=$(append_unique_word "$TARGETS" "$1") ;;
    *) fail "unknown target: $1" ;;
  esac
}

append_os() {
  case "$1" in
    linux|windows) TARGET_OSES=$(append_unique_word "$TARGET_OSES" "$1") ;;
    *) fail "unknown OS: $1" ;;
  esac
}

parse_args() {
  while [ "$#" -gt 0 ]; do
    case "$1" in
      -release)
        CONFIGURATION="Release"
        shift
        ;;
      -debug)
        CONFIGURATION="Debug"
        shift
        ;;
      -os)
        shift
        [ "$#" -gt 0 ] || fail "-os needs at least one value"
        while [ "$#" -gt 0 ]; do
          case "$1" in
            -*) break ;;
            *) append_os "$1"; shift ;;
          esac
        done
        ;;
      -target)
        shift
        [ "$#" -gt 0 ] || fail "-target needs at least one value"
        while [ "$#" -gt 0 ]; do
          case "$1" in
            -*) break ;;
            *) append_target "$1"; shift ;;
          esac
        done
        ;;
      -o)
        shift
        [ "$#" -gt 0 ] || fail "-o needs an output directory"
        OUTPUT_ROOT=$(abs_path "$1")
        shift
        ;;
      -no-clean)
        CLEAN_AFTER=0
        shift
        ;;
      -h|--help)
        print_usage
        exit 0
        ;;
      *)
        fail "unknown argument: $1"
        ;;
    esac
  done

  [ -n "$TARGETS" ] || fail "missing -target"
  [ -n "$TARGET_OSES" ] || TARGET_OSES=$(host_os)
}

word_count() {
  count=0
  for _ in $1; do count=$((count + 1)); done
  printf '%s\n' "$count"
}

has_target() {
  for target in $TARGETS; do
    [ "$target" = "$1" ] && return 0
  done
  return 1
}

safe_remove_dir() {
  [ -n "$1" ] || fail "refusing to remove an empty path"
  case "$1" in
    /|"$SCRIPT_DIR"|"$OUTPUT_ROOT") fail "refusing to remove unsafe path: $1" ;;
  esac
  [ -e "$1" ] && rm -rf -- "$1"
  return 0
}

clean_dir() {
  safe_remove_dir "$1"
  mkdir -p -- "$1"
}

copy_file() {
  [ -f "$1" ] || fail "required file not found: $1"
  mkdir -p -- "$(dirname -- "$2")"
  cp -- "$1" "$2"
}

copy_dir() {
  [ -d "$1" ] || fail "required directory not found: $1"
  mkdir -p -- "$(dirname -- "$2")"
  cp -R -- "$1" "$2"
}

build_dir_for_os() {
  printf '%s/build/%s-%s\n' "$SCRIPT_DIR" "$CONFIGURATION" "$1"
}

runtime_id_for_os() {
  case "$1" in
    linux) printf 'linux-x64\n' ;;
    windows) printf 'win-x64\n' ;;
  esac
}

library_name_for_os() {
  case "$1" in
    linux) printf 'libtensor_planner.so\n' ;;
    windows) printf 'tensor_planner.dll\n' ;;
  esac
}

unity_plugin_path_for_os() {
  case "$1" in
    linux) printf 'Runtime/Plugins/x86_64/libtensor_planner.so\n' ;;
    windows) printf 'Runtime/Plugins/x86_64/tensor_planner.dll\n' ;;
  esac
}

unreal_platform_dir_for_os() {
  case "$1" in
    linux) printf 'Linux\n' ;;
    windows) printf 'Win64\n' ;;
  esac
}

native_link_library_path() {
  os="$1"
  build_dir=$(build_dir_for_os "$os")

  case "$os" in
    linux)
      candidate="$build_dir/$(library_name_for_os "$os")"
      [ -f "$candidate" ] && { printf '%s\n' "$candidate"; return; }
      ;;
    windows)
      for candidate in \
        "$build_dir/tensor_planner.lib" \
        "$build_dir/libtensor_planner.dll.a" \
        "$build_dir/Release/tensor_planner.lib" \
        "$build_dir/Release/libtensor_planner.dll.a" \
        "$build_dir/Debug/tensor_planner.lib" \
        "$build_dir/Debug/libtensor_planner.dll.a"; do
        [ -f "$candidate" ] && { printf '%s\n' "$candidate"; return; }
      done
      ;;
  esac

  fail "native link library for $os not found in $build_dir"
}

require_toolchain_for_os() {
  case "$1" in
    linux)
      require_command cmake
      ;;
    windows)
      require_command cmake
      require_command x86_64-w64-mingw32-g++
      ;;
  esac
}

configure_args_for_os() {
  case "$1" in
    linux)
      printf '%s\n' "-DCMAKE_BUILD_TYPE=$CONFIGURATION"
      ;;
    windows)
      printf '%s\n' "-DCMAKE_BUILD_TYPE=$CONFIGURATION -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++"
      ;;
  esac
}

native_library_path() {
  os="$1"
  build_dir=$(build_dir_for_os "$os")
  name=$(library_name_for_os "$os")

  for candidate in \
    "$build_dir/$name" \
    "$build_dir/lib$name" \
    "$build_dir/Release/$name" \
    "$build_dir/Debug/$name"; do
    [ -f "$candidate" ] && { printf '%s\n' "$candidate"; return; }
  done

  fail "native library for $os not found in $build_dir"
}

build_cpp_for_os() {
  os="$1"
  build_dir=$(build_dir_for_os "$os")
  require_toolchain_for_os "$os"
  info "Configuring C++ ($CONFIGURATION, $os)..."
  # shellcheck disable=SC2046
  cmake -S "$SCRIPT_DIR" -B "$build_dir" $(configure_args_for_os "$os")
  info "Building C++ ($CONFIGURATION, $os)..."
  cmake --build "$build_dir" --parallel
}

build_sharp() {
  require_command dotnet
  info "Building C# ($CONFIGURATION)..."
  dotnet build "$SCRIPT_DIR/csharp/TensorPlanner/TensorPlanner.csproj" -c "$CONFIGURATION"
}

cpp_output_dir_for_os() {
  if [ "$(word_count "$TARGET_OSES")" -gt 1 ]; then
    printf '%s/cpp/%s\n' "$OUTPUT_ROOT" "$1"
  else
    printf '%s/cpp\n' "$OUTPUT_ROOT"
  fi
}

stage_cpp_for_os() {
  os="$1"
  out=$(cpp_output_dir_for_os "$os")
  build_dir=$(build_dir_for_os "$os")
  lib=$(native_library_path "$os")
  name=$(library_name_for_os "$os")
  clean_dir "$out"
  copy_dir "$SCRIPT_DIR/include" "$out/include"
  copy_file "$lib" "$out/lib/$name"
  if [ -f "$build_dir/tensor_plannerConfig.cmake" ]; then
    copy_file "$build_dir/tensor_plannerConfig.cmake" "$out/lib/cmake/tensor_planner/tensor_plannerConfig.cmake"
  fi
  if [ -f "$build_dir/tensor_plannerConfigVersion.cmake" ]; then
    copy_file "$build_dir/tensor_plannerConfigVersion.cmake" "$out/lib/cmake/tensor_planner/tensor_plannerConfigVersion.cmake"
  fi
  info "Created $out"
}

prepare_unity_package() {
  out="$OUTPUT_ROOT/unity/dev.nick.tensor-planner"
  clean_dir "$out"
  copy_file "$SCRIPT_DIR/dev.nick.tensor-planner/package.json" "$out/package.json"
  copy_dir "$SCRIPT_DIR/dev.nick.tensor-planner/Runtime" "$out/Runtime"
  copy_dir "$SCRIPT_DIR/dev.nick.tensor-planner/Samples~" "$out/Samples~"
}

stage_unity_for_os() {
  os="$1"
  out="$OUTPUT_ROOT/unity/dev.nick.tensor-planner"
  lib=$(native_library_path "$os")
  plugin_path=$(unity_plugin_path_for_os "$os")
  copy_file "$lib" "$out/$plugin_path"
  info "Added Unity native plugin for $os"
}

stage_sharp() {
  out="$OUTPUT_ROOT/sharp"
  dll="$SCRIPT_DIR/csharp/TensorPlanner/bin/$CONFIGURATION/netstandard2.1/TensorPlanner.dll"
  pdb="$SCRIPT_DIR/csharp/TensorPlanner/bin/$CONFIGURATION/netstandard2.1/TensorPlanner.pdb"
  deps="$SCRIPT_DIR/csharp/TensorPlanner/bin/$CONFIGURATION/netstandard2.1/TensorPlanner.deps.json"
  clean_dir "$out"
  copy_file "$dll" "$out/lib/netstandard2.1/TensorPlanner.dll"
  [ -f "$pdb" ] && copy_file "$pdb" "$out/lib/netstandard2.1/TensorPlanner.pdb"
  [ -f "$deps" ] && copy_file "$deps" "$out/lib/netstandard2.1/TensorPlanner.deps.json"
  copy_file "$SCRIPT_DIR/README.md" "$out/README.md"
  for os in $TARGET_OSES; do
    rid=$(runtime_id_for_os "$os")
    name=$(library_name_for_os "$os")
    lib=$(native_library_path "$os")
    copy_file "$lib" "$out/runtimes/$rid/native/$name"
  done
  info "Created $out"
}

stage_jai_for_os() {
  os="$1"
  out="$OUTPUT_ROOT/jai/Tensor_Planner"
  lib=$(native_library_path "$os")
  name=$(library_name_for_os "$os")
  copy_file "$lib" "$out/lib/$os/$name"
}

stage_jai() {
  out="$OUTPUT_ROOT/jai/Tensor_Planner"
  clean_dir "$out"
  copy_file "$SCRIPT_DIR/modules/Tensor_Planner/module.jai" "$out/module.jai"
  copy_file "$SCRIPT_DIR/modules/Tensor_Planner/platform.jai" "$out/platform.jai"
  copy_file "$SCRIPT_DIR/modules/Tensor_Planner/status.jai" "$out/status.jai"
  copy_file "$SCRIPT_DIR/modules/Tensor_Planner/schema_spec.jai" "$out/schema_spec.jai"
  copy_file "$SCRIPT_DIR/modules/Tensor_Planner/runtime.jai" "$out/runtime.jai"
  copy_file "$SCRIPT_DIR/modules/Tensor_Planner/generate.jai" "$out/generate.jai"
  copy_dir "$SCRIPT_DIR/modules/Tensor_Planner/generated" "$out/generated"
  for os in $TARGET_OSES; do
    stage_jai_for_os "$os"
  done
  info "Created $out"
}

prepare_unreal_package() {
  out="$OUTPUT_ROOT/unreal/TensorPlanner"
  clean_dir "$out"
  copy_file "$SCRIPT_DIR/unreal/TensorPlanner/TensorPlanner.uplugin" "$out/TensorPlanner.uplugin"
  copy_file "$SCRIPT_DIR/unreal/TensorPlanner/README.md" "$out/README.md"
  copy_file "$SCRIPT_DIR/LICENSE" "$out/LICENSE"
  copy_dir "$SCRIPT_DIR/unreal/TensorPlanner/Source/TensorPlanner" "$out/Source/TensorPlanner"
  copy_dir "$SCRIPT_DIR/include" "$out/Source/ThirdParty/TensorPlanner/include"
}

stage_unreal_for_os() {
  os="$1"
  out="$OUTPUT_ROOT/unreal/TensorPlanner"
  platform_dir=$(unreal_platform_dir_for_os "$os")
  runtime_lib=$(native_library_path "$os")
  runtime_name=$(library_name_for_os "$os")
  link_lib=$(native_link_library_path "$os")
  link_name=$(basename "$link_lib")

  copy_file "$runtime_lib" "$out/Binaries/ThirdParty/TensorPlanner/$platform_dir/$runtime_name"
  copy_file "$link_lib" "$out/Source/ThirdParty/TensorPlanner/lib/$platform_dir/$link_name"
  info "Added Unreal native files for $os"
}

cleanup_build_process_files() {
  [ "$CLEAN_AFTER" -eq 1 ] || return 0
  info "Cleaning build-process files..."
  for os in $TARGET_OSES; do
    safe_remove_dir "$(build_dir_for_os "$os")"
  done
  safe_remove_dir "$SCRIPT_DIR/csharp/TensorPlanner/bin/$CONFIGURATION"
  safe_remove_dir "$SCRIPT_DIR/csharp/TensorPlanner/obj"
}

main() {
  cd "$SCRIPT_DIR"
  parse_args "$@"
  mkdir -p -- "$OUTPUT_ROOT"
  for os in $TARGET_OSES; do
    build_cpp_for_os "$os"
  done
  has_target sharp && build_sharp
  if has_target unity; then
    prepare_unity_package
    for os in $TARGET_OSES; do stage_unity_for_os "$os"; done
    info "Created $OUTPUT_ROOT/unity/dev.nick.tensor-planner"
  fi
  if has_target unreal; then
    prepare_unreal_package
    for os in $TARGET_OSES; do stage_unreal_for_os "$os"; done
    info "Created $OUTPUT_ROOT/unreal/TensorPlanner"
  fi
  if has_target cpp; then
    for os in $TARGET_OSES; do stage_cpp_for_os "$os"; done
  fi
  has_target sharp && stage_sharp
  has_target jai && stage_jai
  cleanup_build_process_files
  info "Done. Output root: $OUTPUT_ROOT"
}

main "$@"
