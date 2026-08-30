#!/usr/bin/env bash
# Imprime o disassembly de todos os shaders do plugin, na ordem em que
# compile_shaders() os compila. Serve para provar que uma refatoracao de shader
# nao mudou o bytecode:
#
#   git stash && ./tools/shader_check.sh > /tmp/antes.txt
#   git stash pop && ./tools/shader_check.sh > /tmp/depois.txt
#   diff /tmp/antes.txt /tmp/depois.txt
#
# O compilador e o mesmo que o plugin resolve em tempo de execucao: o
# d3dcompiler_47.dll do Wine. Nao e o fxc da Microsoft, que nao roda aqui.
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
zig_bin="${ZIG_BIN:-zig}"
wine_bin="${WINE_BIN:-wine}"
build_dir="${project_dir}/build"
tool="${build_dir}/shader_disasm.exe"

if ! command -v "${wine_bin}" >/dev/null 2>&1; then
  echo "Erro: ${wine_bin} nao encontrado; o disassembly precisa do" \
    "d3dcompiler_47.dll do Wine." >&2
  exit 1
fi

mkdir -p "${build_dir}"

"${zig_bin}" c++ \
  -target x86_64-windows-gnu \
  -std=c++20 \
  -O2 \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-nullability-completeness \
  -DUNICODE \
  -D_UNICODE \
  -DWIN32_LEAN_AND_MEAN \
  -DNOMINMAX \
  "${project_dir}/tools/shader_disasm.cpp" \
  -o "${tool}" \
  -lole32

# Os mesmos pares (arquivo, entry point, perfil) declarados em
# compile_shaders(), na mesma ordem.
shaders=(
  "photorealism.hlsl VSMain vs_5_0"
  "photorealism.hlsl PSMain ps_5_0"
  "depth-preview.hlsl PSDepthPreview ps_5_0"
  "ssao.hlsl PSSSAO ps_5_0"
  "temporal.hlsl PSTemporal ps_5_0"
)

cd "${project_dir}/shaders"
status=0
for entry in "${shaders[@]}"; do
  # shellcheck disable=SC2086
  set -- ${entry}
  if [ ! -f "$1" ]; then
    echo "=== $1 : $2 : $3 === (ausente)"
    continue
  fi
  if ! WINEDEBUG="${WINEDEBUG:--all}" "${wine_bin}" "${tool}" "$1" "$2" "$3"; then
    status=1
  fi
done
exit "${status}"
