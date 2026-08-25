#!/usr/bin/env python3
"""Gera game_data.sii preservando a baseline oficial do ETS2 1.60."""

from __future__ import annotations

import json
import re
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
BASELINE_PATH = PROJECT_DIR / "source" / "baseline" / "game_data.sii"
PROFILE_PATH = PROJECT_DIR / "source" / "landscape_profile.json"
OUTPUT_PATH = PROJECT_DIR / "mod" / "def" / "game_data.sii"


def load_profile() -> tuple[float, float, float, float]:
    profile = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
    tree_start = float(profile["tree_lod_start"])
    tree_end = float(profile["tree_lod_end"])
    grass_start = float(profile["grass_lod_start"])
    grass_end = float(profile["grass_lod_end"])

    if tree_start < 0.0 or tree_end <= tree_start:
        raise ValueError("tree_lod_end deve ser maior que tree_lod_start")

    if grass_start < 0.0 or grass_end <= grass_start:
        raise ValueError("grass_lod_end deve ser maior que grass_lod_start")

    grass_fade_distance = grass_end - grass_start
    if grass_fade_distance > grass_end / 9.0:
        raise ValueError(
            "a faixa de transicao da grama viola o limite end / 9.0 do jogo"
        )

    return tree_start, tree_end, grass_start, grass_end


def replace_lod_component(
    text: str, field: str, component_index: int, value: float
) -> str:
    pattern = re.compile(
        rf"(?m)^(?P<prefix>\s*{re.escape(field)}:\s*)"
        r"\((?P<values>[^)]*)\)"
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError(f"campo ausente na baseline: {field}")

    components = [float(item.strip()) for item in match.group("values").split(",")]
    if len(components) != 3:
        raise ValueError(f"vetor inesperado em {field}: {components}")

    components[component_index] = value
    vector = f"({components[0]:.1f}, {components[1]:.1f}, {components[2]:.1f})"
    replacement = f"{match.group('prefix')}{vector}"
    return text[: match.start()] + replacement + text[match.end() :]


def main() -> None:
    tree_start, tree_end, grass_start, grass_end = load_profile()
    output = BASELINE_PATH.read_text(encoding="utf-8")
    output = replace_lod_component(output, "leaves_lod_start", 0, tree_start)
    output = replace_lod_component(output, "leaves_lod_end", 0, tree_end)
    output = replace_lod_component(output, "leaves_lod_start", 1, grass_start)
    output = replace_lod_component(output, "leaves_lod_end", 1, grass_end)

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(output, encoding="utf-8")
    print(
        "game_data.sii gerado: "
        f"arvores {tree_start:.1f}-{tree_end:.1f} m; "
        f"grama {grass_start:.1f}-{grass_end:.1f} m -> {OUTPUT_PATH}"
    )


if __name__ == "__main__":
    main()
