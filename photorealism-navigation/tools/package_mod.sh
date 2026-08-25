#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
dist_dir="$project_dir/dist"
mod_version=${MOD_VERSION:-0.2.0}
game_version=${GAME_VERSION:-1.60}

validate_version() {
	label=$1
	value=$2

	case "$value" in
		''|*[!0-9A-Za-z._-]*)
			echo "Erro: $label invalida para nome de arquivo: $value" >&2
			exit 1
			;;
	esac
}

validate_version "versao do mod" "$mod_version"
validate_version "versao do jogo" "$game_version"

python3 "$project_dir/tools/build_navigation.py" --theme all

mkdir -p "$dist_dir"

for theme in light dark; do
	build_dir="$project_dir/build/$theme"
	python3 "$project_dir/tools/validate_manifest.py" --mod-dir "$build_dir"

	package="$dist_dir/photorealism-navigation-${theme}-${mod_version}-${game_version}.scs"
	rm -f "$package"

	if command -v zip >/dev/null 2>&1; then
		(
			cd "$build_dir"
			zip -r -9 "$package" . \
				-x '*.DS_Store' \
				-x 'Thumbs.db'
		)
	elif command -v 7z >/dev/null 2>&1; then
		(
			cd "$build_dir"
			7z a -tzip -mx=9 "$package" . \
				-xr'!*.DS_Store' \
				-xr'!Thumbs.db'
		)
	else
		echo "Erro: instale o comando zip ou 7z para gerar o pacote." >&2
		exit 1
	fi

	echo "Pacote criado: $package"
done
