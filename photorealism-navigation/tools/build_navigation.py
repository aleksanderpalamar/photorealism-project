#!/usr/bin/env python3
"""Gera as variantes Light e Dark do Photorealism Navigation."""

from __future__ import annotations

import argparse
import json
import re
import shutil
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
BASELINE_DIR = PROJECT_DIR / "source" / "baseline"
PROFILES_DIR = PROJECT_DIR / "source" / "profiles"
MOD_DIR = PROJECT_DIR / "mod"
BUILD_DIR = PROJECT_DIR / "build"
COLOR_PATTERN = re.compile(r"^0x[0-9A-Fa-f]{8}$")
UI_COLOR_PATTERN = re.compile(r"^[0-9A-Fa-f]{8}$")

SCALAR_COLOR_FIELDS = (
    "road_color",
    "road_discovered_color",
    "job_road_color",
    "job_road_discovered_color",
    "world_road_color",
    "world_road_discovered_color",
    "fleet_manager_road_color",
    "fleet_manager_road_discovered_color",
    "prefab_color",
    "prefab_obstacle_color",
    "prefab_grass_color",
    "prefab_discovered_color",
    "prefab_obstacle_discovered_color",
    "prefab_grass_discovered_color",
    "outline_color",
    "navigation_color",
    "navigation_highlight_color",
    "navigation_fade_color",
    "navigation_arrow_color",
)

VOLVO_GPS_FILES = (
    "volvo_fh_2021_gps.sii",
    "volvo_fh_2021_mph_gps.sii",
    "volvo_fh_2024_gps.sii",
    "volvo_fh_2024_mph_gps.sii",
)


def replace_scalar_color(text: str, field: str, color: str) -> str:
    pattern = re.compile(
        rf"(?m)^(?P<prefix>\s*{re.escape(field)}:\s*)0x[0-9A-Fa-f]{{8}}"
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError(f"campo ausente na baseline: {field}")
    return text[: match.start()] + f"{match.group('prefix')}{color}" + text[match.end() :]


def replace_array_colors(text: str, field: str, colors: list[str]) -> str:
    pattern = re.compile(
        rf"(?m)^(?P<prefix>\s*{re.escape(field)}\[\]:\s*)0x[0-9A-Fa-f]{{8}}"
    )
    matches = list(pattern.finditer(text))
    if len(matches) != len(colors):
        raise ValueError(
            f"{field}: esperado {len(colors)} valores, encontrados {len(matches)}"
        )

    parts: list[str] = []
    cursor = 0
    for match, color in zip(matches, colors, strict=True):
        parts.append(text[cursor : match.start()])
        parts.append(f"{match.group('prefix')}{color}")
        cursor = match.end()
    parts.append(text[cursor:])
    return "".join(parts)


def validate_profile(profile: dict[str, object], theme: str) -> None:
    if profile.get("theme") != theme:
        raise ValueError(f"perfil {theme}: campo theme divergente")

    for field in SCALAR_COLOR_FIELDS:
        color = profile.get(field)
        if not isinstance(color, str) or not COLOR_PATTERN.fullmatch(color):
            raise ValueError(f"cor 0xFFBBGGRR invalida em {field}: {color!r}")

    for field in ("map_area_color", "map_area_discovered_color"):
        colors = profile.get(field)
        if not isinstance(colors, list) or len(colors) != 4:
            raise ValueError(f"{field} deve conter exatamente quatro cores")
        for color in colors:
            if not isinstance(color, str) or not COLOR_PATTERN.fullmatch(color):
                raise ValueError(f"cor 0xFFBBGGRR invalida em {field}: {color!r}")

    ui_background = profile.get("ui_map_background")
    if not isinstance(ui_background, str) or not UI_COLOR_PATTERN.fullmatch(
        ui_background
    ):
        raise ValueError(f"cor de interface invalida: {ui_background!r}")


def build_map_data(profile: dict[str, object]) -> str:
    text = (BASELINE_DIR / "map_data.sii").read_text(encoding="utf-8")
    for field in SCALAR_COLOR_FIELDS:
        text = replace_scalar_color(text, field, str(profile[field]))
    text = replace_array_colors(text, "map_area_color", profile["map_area_color"])
    text = replace_array_colors(
        text, "map_area_discovered_color", profile["map_area_discovered_color"]
    )
    return text


def add_adviser_map_background(text: str, ui_color: str) -> str:
    group_pattern = re.compile(
        r"(?P<header>ui::group\s*:\s*_nameless\._\.adviser\.widget\s*\{.*?"
        r"\n\s*my_children:\s*)9(?P<children>.*?"
        r"\n\s*my_children\[8\]:\s*_nameless\._\.current\.status)",
        re.DOTALL,
    )
    match = group_pattern.search(text)
    if match is None:
        raise ValueError("grupo adviser.widget nao encontrado")

    replacement = (
        f"{match.group('header')}10{match.group('children')}\n"
        " my_children[9]: _nameless._.pr.nav.map.background"
    )
    text = text[: match.start()] + replacement + text[match.end() :]

    marker = "ui_map : _nameless.16d.35c0 {"
    if marker not in text:
        raise ValueError("ui_map do Route Advisor nao encontrado")

    background_unit = f"""ui::text_common : _nameless._.pr.nav.map.background {{
 value: {ui_color}
 look_template: txt.window.bcg_rect4
 text: ""
 coords_l: 1150
 coords_r: 1420
 coords_t: 232
 coords_b: 42
 area_l: 1
 area_r: 0
 area_t: 0
 area_b: 1
 id: 0
 layer: 5
 tab: -1
 pointer: -1
 my_parent: _nameless._.adviser.widget
}}

"""
    return text.replace(marker, background_unit + marker, 1)


def patch_common_gps(text: str, ui_color: str) -> str:
    pattern = re.compile(
        r"(ui::text\s*:\s*_nameless\._\.background\s*\{.*?"
        r"color=)[0-9A-Fa-f]{8}(\s+xscale=stretch)",
        re.DOTALL,
    )
    text, count = pattern.subn(rf"\g<1>{ui_color}\g<2>", text, count=1)
    if count != 1:
        raise ValueError("fundo do GPS comum nao encontrado")
    return text


def patch_volvo_gps(text: str, ui_color: str, filename: str) -> str:
    pattern = re.compile(
        r"(ui::text\s*:\s*_nameless\._\.bare_map\s*\{\s*"
        r"text:\s*\"<color value=)[0-9A-Fa-f]{8}(>)",
        re.DOTALL,
    )
    text, count = pattern.subn(rf"\g<1>{ui_color}\g<2>", text, count=1)
    if count != 1:
        raise ValueError(f"fundo do GPS Volvo nao encontrado: {filename}")
    return text


def generate_manifest(profile: dict[str, object]) -> str:
    theme = str(profile["theme"])
    title = str(profile["display_name"])
    unit = ".pr_nav_l" if theme == "light" else ".pr_nav_d"
    return f'''SiiNunit
{{
mod_package : {unit}
{{
\tpackage_version: "{profile['profile_version']}"
\tdisplay_name: "{title}"
\tauthor: "Palamar"

\tcategory[]: "ui"

\ticon: "mod_icon.jpg"
\tdescription_file: "mod_description.txt"
\tcompatible_versions[]: "{profile['game_version']}.*"
}}
}}
'''


def generate_description(profile: dict[str, object]) -> str:
    title = str(profile["display_name"])
    theme_name = "clara" if profile["theme"] == "light" else "escura"
    return f"""[white]{title} {profile['profile_version']}

[normal]Interface de navegação inspirada na clareza dos mapas modernos, com identidade própria da coleção Photorealism.

Esta variante aplica:
- paleta {theme_name} no GPS e nos mapas
- rota azul com destaque e segmento percorrido calibrados
- fundo dedicado no Route Advisor e no GPS comum
- suporte visual aos GPS Volvo FH 2021 e FH 2024, incluindo MPH
- compatibilidade com Photorealism e Photorealism Landscape

[orange]Importante:
[normal]Ative somente uma variante do Photorealism Navigation por vez: Light ou Dark.
"""


def build_theme(theme: str) -> Path:
    profile_path = PROFILES_DIR / f"{theme}.json"
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    validate_profile(profile, theme)

    output_dir = BUILD_DIR / theme
    if output_dir.exists():
        shutil.rmtree(output_dir)
    shutil.copytree(MOD_DIR, output_dir)

    (output_dir / "def").mkdir(parents=True, exist_ok=True)
    (output_dir / "def" / "map_data.sii").write_text(
        build_map_data(profile), encoding="utf-8"
    )

    ui_color = str(profile["ui_map_background"])
    ui_dir = output_dir / "ui"
    dashboard_dir = ui_dir / "dashboard"
    dashboard_dir.mkdir(parents=True, exist_ok=True)

    adviser = (BASELINE_DIR / "ui" / "adviser_gps.sii").read_text(
        encoding="utf-8"
    )
    (ui_dir / "adviser_gps.sii").write_text(
        add_adviser_map_background(adviser, ui_color), encoding="utf-8"
    )

    common_gps = (BASELINE_DIR / "ui" / "gps.sii").read_text(encoding="utf-8")
    (ui_dir / "gps.sii").write_text(
        patch_common_gps(common_gps, ui_color), encoding="utf-8"
    )

    for filename in VOLVO_GPS_FILES:
        source = (BASELINE_DIR / "ui" / "dashboard" / filename).read_text(
            encoding="utf-8"
        )
        (dashboard_dir / filename).write_text(
            patch_volvo_gps(source, ui_color, filename), encoding="utf-8"
        )

    (output_dir / "manifest.sii").write_text(
        generate_manifest(profile), encoding="utf-8"
    )
    (output_dir / "mod_description.txt").write_text(
        generate_description(profile), encoding="utf-8"
    )

    print(
        f"Variante {theme} {profile['profile_version']} gerada em: {output_dir}"
    )
    return output_dir


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--theme",
        choices=("light", "dark", "all"),
        default="all",
        help="variante a gerar (padrao: all)",
    )
    args = parser.parse_args()

    themes = ("light", "dark") if args.theme == "all" else (args.theme,)
    for theme in themes:
        build_theme(theme)


if __name__ == "__main__":
    main()
