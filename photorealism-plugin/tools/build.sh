#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
zig_bin="${ZIG_BIN:-zig}"
build_dir="${project_dir}/build"

mkdir -p "${build_dir}"

common_flags=(
  -target x86_64-windows-gnu
  -std=c++20
  -O2
  -Wno-nullability-completeness
  -DUNICODE
  -D_UNICODE
  -DWIN32_LEAN_AND_MEAN
  -DNOMINMAX
  -shared
)

"${zig_bin}" c++ \
  "${common_flags[@]}" \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-cast-function-type-mismatch \
  "${project_dir}/src/proxy.cpp" \
  "${project_dir}/src/native_aa_config.cpp" \
  "${project_dir}/src/dinput8.def" \
  -o "${build_dir}/dinput8.dll" \
  -lole32 \
  -lshell32

"${zig_bin}" c++ \
  "${common_flags[@]}" \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-cast-function-type-mismatch \
  "${project_dir}/src/fsr_module.cpp" \
  "${project_dir}/src/fsr_runtime.cpp" \
  "${project_dir}/src/photorealism-fsr.def" \
  -o "${build_dir}/photorealism-fsr.dll"

"${zig_bin}" c++ \
  "${common_flags[@]}" \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-cast-function-type-mismatch \
  "${project_dir}/src/dxgi_proxy.cpp" \
  "${project_dir}/src/hook.cpp" \
  "${project_dir}/src/postprocess.cpp" \
  "${project_dir}/src/fsr_bridge.cpp" \
  "${project_dir}/src/steam_screenshots.cpp" \
  "${project_dir}/src/config.cpp" \
  "${project_dir}/src/resource_observer.cpp" \
  "${project_dir}/src/runtime.cpp" \
  "${project_dir}/src/dxgi.def" \
  -o "${build_dir}/dxgi.dll" \
  -luser32 \
  -lole32

echo "Gerado: ${build_dir}/dinput8.dll"
echo "Gerado: ${build_dir}/dxgi.dll"
echo "Gerado: ${build_dir}/photorealism-fsr.dll (AA/FSR 0.6.0 ABI v1-v4 temporal+EASU+RCAS automatico)"
