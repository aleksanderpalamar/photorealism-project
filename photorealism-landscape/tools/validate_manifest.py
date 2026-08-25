#!/usr/bin/env python3
"""Valida os metadados essenciais do manifest do mod."""

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

    components = unit_name[1:].split(".")
    invalid = [component for component in components if not TOKEN_PATTERN.fullmatch(component)]
    if invalid:
        raise ValueError(
            "componente de unidade SII invalido; use 1-12 caracteres "
            f"[a-z0-9_]: {invalid}"
        )

    required_string(text, "package_version")
    required_string(text, "display_name")
    required_string(text, "author")
    icon = required_string(text, "icon")
    description = required_string(text, "description_file")

    for resource in (icon, description):
        if not (MOD_DIR / resource).is_file():
            raise ValueError(f"recurso referenciado pelo manifest nao existe: {resource}")

    categories = re.findall(r'(?m)^\s*category\[\]:\s*"([^"]+)"', text)
    if not categories:
        raise ValueError("nenhuma categoria definida no manifest")

    invalid_categories = sorted(set(categories) - VALID_CATEGORIES)
    if invalid_categories:
        raise ValueError(f"categorias invalidas: {invalid_categories}")

    print(
        "Manifest valido: "
        f"unidade {unit_name}; categorias {', '.join(categories)}; icone {icon}"
    )


if __name__ == "__main__":
    main()

