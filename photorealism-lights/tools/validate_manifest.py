#!/usr/bin/env python3
"""Valida os metadados e os arquivos essenciais do Photorealism Lights."""

from __future__ import annotations

import re
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
MOD_DIR = PROJECT_DIR / "mod"
MANIFEST_PATH = MOD_DIR / "manifest.sii"
TOKEN_PATTERN = re.compile(r"^[a-z0-9_]{1,12}$")
VALID_CATEGORIES = {
    "truck",
    "trailer",
    "interior",
    "tuning_parts",
    "ai_traffic",
    "sound",
    "paint_job",
    "cargo_pack",
    "map",
    "ui",
    "weather_setup",
    "physics",
    "graphics",
    "models",
    "movers",
    "walkers",
    "prefabs",
    "other",
}
REQUIRED_BASELINES = (
    "def/traffic_light_lamp_colors.sii",
    "def/aux_lamp_colors.sii",
    "def/default_vehicle_lamp_setup.sii",
    "unit/hookup/tr_light_flares.sii",
    "unit/hookup/lights/street_lamp.sii",
    "unit/hookup/lights/street_lamp_b.sii",
    "unit/hookup/lights/street_lamp_n.sii",
    "unit/hookup/lights/street_lamp_y.sii",
    "unit/hookup/vehicle/flare/vehicle_headl.sii",
    "unit/hookup/vehicle/flare/vehicle_brakel.sii",
    "unit/hookup/vehicle/flare/vehicle_rearl.sii",
)
EXPECTED_HEADLIGHT_PROFILES = 47


def required_string(text: str, field: str) -> str:
    match = re.search(rf'(?m)^\s*{re.escape(field)}:\s*"([^"]+)"', text)
    if match is None:
        raise ValueError(f"campo obrigatorio ausente no manifest: {field}")
    return match.group(1)


def main() -> None:
    text = MANIFEST_PATH.read_text(encoding="utf-8")
    unit_match = re.search(r"(?m)^\s*mod_package\s*:\s*([^\s{]+)", text)
    if unit_match is None:
        raise ValueError("unidade mod_package ausente no manifest")

    unit_name = unit_match.group(1)
    if not unit_name.startswith("."):
        raise ValueError("mod_package deve usar um nome anonimo iniciado por ponto")
    invalid = [
        component
        for component in unit_name[1:].split(".")
        if not TOKEN_PATTERN.fullmatch(component)
    ]
    if invalid:
        raise ValueError(f"componentes SII invalidos: {invalid}")

    version = required_string(text, "package_version")
    display_name = required_string(text, "display_name")
    author = required_string(text, "author")
    icon = required_string(text, "icon")
    description = required_string(text, "description_file")

    for resource in (icon, description):
        if not (MOD_DIR / resource).is_file():
            raise ValueError(f"recurso referenciado nao existe: {resource}")

    categories = re.findall(r'(?m)^\s*category\[\]:\s*"([^"]+)"', text)
    if not categories:
        raise ValueError("nenhuma categoria definida no manifest")
    invalid_categories = sorted(set(categories) - VALID_CATEGORIES)
    if invalid_categories:
        raise ValueError(f"categorias invalidas: {invalid_categories}")

    missing = [relative for relative in REQUIRED_BASELINES if not (MOD_DIR / relative).is_file()]
    if missing:
        raise ValueError(f"arquivos essenciais ausentes: {missing}")

    headlight_profiles = list(
        MOD_DIR.glob("def/vehicle/truck/*/head_light/*.sii")
    )
    if len(headlight_profiles) != EXPECTED_HEADLIGHT_PROFILES:
        raise ValueError(
            "quantidade inesperada de perfis de farois no mod: "
            f"{len(headlight_profiles)} (esperado {EXPECTED_HEADLIGHT_PROFILES})"
        )

    print(
        f"Manifest valido: {display_name} {version}; unidade {unit_name}; "
        f"autor {author}; categoria {', '.join(categories)}"
    )


if __name__ == "__main__":
    main()
