#!/usr/bin/env python3
"""Reconstrói bad.sii e aplica o primeiro passe de mau tempo diurno."""

from __future__ import annotations

from pathlib import Path

from climate_sii import apply_profile_pass


PROJECT_DIR = Path(__file__).resolve().parent.parent
BASELINE = PROJECT_DIR / "source/baseline/default/bad.sii"
OUTPUT = PROJECT_DIR / "mod/def/climate/default/bad.sii"

PASSES = (
    (
        "noite chuvosa (manhã)",
        1,
        4,
        {
            "fog_density": 1.08,
            "color_saturation": 0.95,
            "contrast": 0.98,
            "bloom_threshold": 1.40,
            "bloom_limit": 0.80,
            "bloom_intensity": 0.50,
            "bloom_standard_deviation": 0.88,
            "sunshaft_color": 0.60,
        },
    ),
    (
        "amanhecer chuvoso",
        5,
        13,
        {
            "ambient": 0.98,
            "sun_shadow_strength": 0.82,
            "fog_density": 1.10,
            "color_saturation": 0.97,
            "contrast": 0.98,
            "bloom_threshold": 1.25,
            "bloom_limit": 0.88,
            "bloom_intensity": 0.58,
            "sunshaft_color": 0.65,
        },
    ),
    (
        "mau tempo diurno",
        14,
        22,
        {
            "ambient": 0.98,
            "sun_shadow_strength": 0.80,
            "fog_density": 1.06,
            "color_saturation": 0.97,
            "contrast": 0.98,
            "bloom_threshold": 1.30,
            "bloom_limit": 0.85,
            "bloom_intensity": 0.52,
            "sunshaft_color": 0.60,
        },
    ),
    (
        "entardecer chuvoso",
        23,
        31,
        {
            "ambient": 0.97,
            "sun_shadow_strength": 0.82,
            "fog_density": 1.08,
            "color_saturation": 0.96,
            "contrast": 0.97,
            "bloom_threshold": 1.25,
            "bloom_limit": 0.86,
            "bloom_intensity": 0.55,
            "sunshaft_color": 0.62,
        },
    ),
    (
        "noite chuvosa (tarde)",
        32,
        34,
        {
            "fog_density": 1.08,
            "color_saturation": 0.95,
            "contrast": 0.98,
            "bloom_threshold": 1.40,
            "bloom_limit": 0.80,
            "bloom_intensity": 0.50,
            "bloom_standard_deviation": 0.88,
            "sunshaft_color": 0.60,
        },
    ),
)

# Passo 0.8.0 inspirado nas tendências de adaptação e tonemapping observadas
# no Orion Elite, mas mantendo todas as variações oficiais e a chuva aprovada.
ORION_REFERENCE_PASSES = (
    (
        "resposta ocular global",
        1,
        34,
        {
            "dark_adaptation_speed": 1.12,
            "bright_adaptation_speed": 0.96,
        },
    ),
    (
        "definição do amanhecer chuvoso",
        5,
        13,
        {
            "contrast": 1.03,
            "shoulder_length": 0.96,
        },
    ),
    (
        "definição diurna sob chuva",
        14,
        22,
        {
            "fog_density": 0.97,
            "contrast": 1.04,
            "shoulder_length": 0.92,
        },
    ),
    (
        "definição do entardecer chuvoso",
        23,
        31,
        {
            "contrast": 1.03,
            "shoulder_length": 0.96,
        },
    ),
)


def main() -> None:
    calibrated = BASELINE.read_text(encoding="utf-8")

    for name, first, last, scales in PASSES:
        calibrated, changes = apply_profile_pass(
            calibrated,
            first,
            last,
            scales,
            weather_type="bad",
        )
        print(f"{name}: perfis {first:02d}–{last:02d}")
        for field, factor in scales.items():
            print(f"  {field}: {factor:.2f}x ({changes[field]} entradas)")

    print("refinamento técnico 0.8.0:")
    for name, first, last, scales in ORION_REFERENCE_PASSES:
        calibrated, changes = apply_profile_pass(
            calibrated,
            first,
            last,
            scales,
            weather_type="bad",
        )
        print(f"  {name}: perfis {first:02d}–{last:02d}")
        for field, factor in scales.items():
            print(f"    {field}: {factor:.2f}x ({changes[field]} entradas)")

    OUTPUT.write_text(calibrated, encoding="utf-8")
    print(f"Arquivo atualizado: {OUTPUT}")


if __name__ == "__main__":
    main()
