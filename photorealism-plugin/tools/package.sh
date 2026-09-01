#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="0.17.1"
package_name="photorealism-plugin-${version}-ets2-ats-1.60-proton"
output_dir="${project_dir}/dist"
staging_dir="$(mktemp -d)"

cleanup() {
  rm -rf -- "${staging_dir}"
}
trap cleanup EXIT

"${project_dir}/tools/build.sh"

mkdir -p "${staging_dir}/${package_name}/photorealism-plugin/shaders"
mkdir -p "${output_dir}"
cp "${project_dir}/build/dinput8.dll" "${staging_dir}/${package_name}/dinput8.dll"
cp "${project_dir}/build/dxgi.dll" "${staging_dir}/${package_name}/dxgi.dll"
cp "${project_dir}/config/photorealism-plugin.cfg" \
  "${staging_dir}/${package_name}/photorealism-plugin/photorealism-plugin.cfg"
cp "${project_dir}/shaders/depth_view_space.hlsli" \
  "${staging_dir}/${package_name}/photorealism-plugin/shaders/depth_view_space.hlsli"
cp "${project_dir}/shaders/photorealism.hlsl" \
  "${staging_dir}/${package_name}/photorealism-plugin/shaders/photorealism.hlsl"
cp "${project_dir}/shaders/depth-preview.hlsl" \
  "${staging_dir}/${package_name}/photorealism-plugin/shaders/depth-preview.hlsl"
cp "${project_dir}/shaders/ssao.hlsl" \
  "${staging_dir}/${package_name}/photorealism-plugin/shaders/ssao.hlsl"
cp "${project_dir}/shaders/temporal.hlsl" \
  "${staging_dir}/${package_name}/photorealism-plugin/shaders/temporal.hlsl"
cp "${project_dir}/shaders/bloom.hlsl" \
  "${staging_dir}/${package_name}/photorealism-plugin/shaders/bloom.hlsl"
cp "${project_dir}/README.md" "${staging_dir}/${package_name}/README.md"
cp "${project_dir}/CHANGELOG.md" \
  "${staging_dir}/${package_name}/CHANGELOG.md"
cp "${project_dir}/ROADMAP.md" \
  "${staging_dir}/${package_name}/ROADMAP.md"

if command -v zip >/dev/null 2>&1; then
  (
    cd "${staging_dir}"
    zip -q -r "${output_dir}/${package_name}.zip" "${package_name}"
  )
elif command -v 7z >/dev/null 2>&1; then
  (
    cd "${staging_dir}"
    7z a -tzip -bd "${output_dir}/${package_name}.zip" "${package_name}" >/dev/null
  )
elif command -v bsdtar >/dev/null 2>&1; then
  (
    cd "${staging_dir}"
    bsdtar -a -cf "${output_dir}/${package_name}.zip" "${package_name}"
  )
else
  echo "Erro: instale zip, 7z ou bsdtar para gerar o pacote." >&2
  exit 1
fi

echo "Pacote: ${output_dir}/${package_name}.zip"
