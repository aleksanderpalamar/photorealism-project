#!/usr/bin/env python3
"""Reconstrói nice.sii a partir da baseline e aplica todos os passes aprovados."""

from __future__ import annotations

from pathlib import Path

from climate_sii import apply_profile_pass, replace_profile_texture_paths


PROJECT_DIR = Path(__file__).resolve().parent.parent
BASELINE = PROJECT_DIR / "source/baseline/default/nice.sii"
OUTPUT = PROJECT_DIR / "mod/def/climate/default/nice.sii"

PASSES = (
    (
        "noite profunda (manhã)",
        1,
        4,
        {
            "fog_density": 1.04,
            "color_saturation": 0.96,
            "contrast": 0.98,
            "bloom_threshold": 1.35,
            "bloom_limit": 0.82,
            "bloom_intensity": 0.55,
            "bloom_standard_deviation": 0.90,
            "sunshaft_color": 0.65,
        },
    ),
    (
        "amanhecer",
        5,
        13,
        {
            "ambient": 0.96,
            "sun_shadow_strength": 0.90,
            "fog_density": 1.15,
            "color_saturation": 0.97,
            "contrast": 0.94,
            "bloom_threshold": 1.20,
            "bloom_limit": 0.90,
            "bloom_intensity": 0.65,
            "sunshaft_color": 0.72,
        },
    ),
    (
        "meio-dia",
        14,
        22,
        {
            "ambient": 0.97,
            "sun_shadow_strength": 0.92,
            "fog_density": 1.08,
            "color_saturation": 0.96,
            "contrast": 0.95,
            "bloom_threshold": 1.25,
            "bloom_limit": 0.90,
            "bloom_intensity": 0.60,
            "sunshaft_color": 0.70,
        },
    ),
    (
        "entardecer",
        23,
        31,
        {
            "ambient": 0.95,
            "sun_shadow_strength": 0.90,
            "fog_density": 1.08,
            "color_saturation": 0.96,
            "contrast": 0.93,
            "bloom_threshold": 1.20,
            "bloom_limit": 0.88,
            "bloom_intensity": 0.62,
            "sunshaft_color": 0.68,
        },
    ),
    (
        "noite profunda (tarde)",
        32,
        34,
        {
            "fog_density": 1.04,
            "color_saturation": 0.96,
            "contrast": 0.98,
            "bloom_threshold": 1.35,
            "bloom_limit": 0.82,
            "bloom_intensity": 0.55,
            "bloom_standard_deviation": 0.90,
            "sunshaft_color": 0.65,
        },
    ),
)

# Passo 0.8.0 inspirado nas tendências observadas no Orion Elite. Os valores
# extremos e a estrutura reduzida de variações do mod de referência não são
# reproduzidos aqui; preservamos a baseline oficial e os passes já aprovados.
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
        "roll-off do amanhecer",
        5,
        13,
        {
            "shoulder_length": 0.94,
            "contrast": 1.02,
        },
    ),
    (
        "separação solar diurna",
        14,
        22,
        {
            "diffuse": 1.04,
            "specular": 1.04,
            "env_static_mod": 1.02,
            "fog_density": 0.96,
            "contrast": 1.02,
            "shoulder_length": 0.90,
        },
    ),
    (
        "roll-off do entardecer",
        23,
        31,
        {
            "diffuse": 1.02,
            "specular": 1.02,
            "env_static_mod": 1.01,
            "contrast": 1.02,
            "shoulder_length": 0.94,
        },
    ),
)

MIDDAY_SKYBOX_PROFILES = {
    16: "/asset/skybox/photorealism/clear_midday_p16.tobj",
    17: "/asset/skybox/photorealism/clear_midday_02.tobj",
    18: "/asset/skybox/photorealism/clear_midday_p18.tobj",
    19: "/asset/skybox/photorealism/clear_midday_p19.tobj",
    20: "/asset/skybox/photorealism/clear_midday_p20.tobj",
    21: "/asset/skybox/photorealism/clear_midday_p21.tobj",
}
MIDDAY_SKYCLOUD_MASK = "/asset/skybox/photorealism/clear_midday_02-mask.tobj"


def main() -> None:
    calibrated = BASELINE.read_text(encoding="utf-8")

    for name, first, last, scales in PASSES:
        calibrated, changes = apply_profile_pass(calibrated, first, last, scales)
        print(f"{name}: perfis {first:02d}–{last:02d}")
        for field, factor in scales.items():
            print(f"  {field}: {factor:.2f}x ({changes[field]} entradas)")

    print("refinamento técnico 0.8.0:")
    for name, first, last, scales in ORION_REFERENCE_PASSES:
        calibrated, changes = apply_profile_pass(calibrated, first, last, scales)
        print(f"  {name}: perfis {first:02d}–{last:02d}")
        for field, factor in scales.items():
            print(f"    {field}: {factor:.2f}x ({changes[field]} entradas)")

    print("família de skyboxes claros:")
    for profile_number, skybox_path in MIDDAY_SKYBOX_PROFILES.items():
        textures = {
            "skybox_texture": skybox_path,
            "skycloud_mask_texture": MIDDAY_SKYCLOUD_MASK,
        }
        calibrated, texture_changes = replace_profile_texture_paths(
            calibrated,
            profile_number,
            textures,
        )
        print(f"  perfil {profile_number:02d}: {skybox_path}")
        print(
            "    "
            f"skybox={texture_changes['skybox_texture']}, "
            f"máscara={texture_changes['skycloud_mask_texture']} entradas"
        )

    OUTPUT.write_text(calibrated, encoding="utf-8")
    print(f"Arquivo atualizado: {OUTPUT}")


if __name__ == "__main__":
    main()
