#!/usr/bin/env python3
"""Gera a frequência climática e a resposta de molhamento do Photorealism."""

from __future__ import annotations

import hashlib
import re
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
BASELINE = PROJECT_DIR / "source/baseline/climate.sii"
OUTPUT = PROJECT_DIR / "mod/def/climate.sii"
BASELINE_SHA256 = "7adc623f4278576c034217b5bb0696ee3e08d8971c7e628ee115400f195ca358"

FIELDS = ("bad_weather_factor", "wetting_factor", "drying_factor")
CALIBRATION = {
    "default": {
        "bad_weather_factor": "0.08",
        "wetting_factor": "0.11",
        "drying_factor": "0.0075",
    },
    "cold": {
        "bad_weather_factor": "0.09",
        "wetting_factor": "0.115",
        "drying_factor": "0.0065",
    },
    "arid": {
        "bad_weather_factor": "0.03",
        "wetting_factor": "0.10",
        "drying_factor": "0.012",
    },
    "desert": {
        "bad_weather_factor": "0.01",
        "wetting_factor": "0.09",
        "drying_factor": "0.016",
    },
}
TECHNICAL_PROFILES = ("reference", "albedo", "black", "integrity", "grayscale")


def profile_pattern(name: str) -> re.Pattern[str]:
    return re.compile(
        rf"(climate_profile\s*:\s*climate\.{re.escape(name)}\s*\{{)(.*?)(\n\}})",
        re.DOTALL,
    )


def profile_block(text: str, name: str) -> str:
    matches = list(profile_pattern(name).finditer(text))
    if len(matches) != 1:
        raise ValueError(f"Esperado exatamente um perfil climate.{name}; encontrados: {len(matches)}")
    return matches[0].group(0)


def replace_profile(text: str, name: str, values: dict[str, str]) -> str:
    pattern = profile_pattern(name)
    match = pattern.search(text)
    if match is None:
        raise ValueError(f"Perfil ausente: climate.{name}")

    body = match.group(2)
    for field in FIELDS:
        field_pattern = re.compile(rf"(^\s*{field}:\s*)\S+", re.MULTILINE)
        body, count = field_pattern.subn(rf"\g<1>{values[field]}", body)
        if count != 1:
            raise ValueError(
                f"Esperado exatamente um campo {field} em climate.{name}; encontrado: {count}"
            )

    return text[: match.start()] + match.group(1) + body + match.group(3) + text[match.end() :]


def values_from_profile(text: str, name: str) -> dict[str, str]:
    block = profile_block(text, name)
    values: dict[str, str] = {}
    for field in FIELDS:
        match = re.search(rf"^\s*{field}:\s*(\S+)", block, re.MULTILINE)
        if match is None:
            raise ValueError(f"Campo ausente: climate.{name}.{field}")
        values[field] = match.group(1)
    return values


def main() -> None:
    baseline = BASELINE.read_text(encoding="utf-8")
    digest = hashlib.sha256(baseline.encode("utf-8")).hexdigest()
    if digest != BASELINE_SHA256:
        raise ValueError(
            "A baseline de climate.sii não corresponde ao arquivo oficial esperado: "
            f"{digest}"
        )

    generated = baseline
    for name, values in CALIBRATION.items():
        generated = replace_profile(generated, name, values)

    for name, expected in CALIBRATION.items():
        actual = values_from_profile(generated, name)
        if actual != expected:
            raise ValueError(f"Validação falhou para climate.{name}: {actual}")

    for name in TECHNICAL_PROFILES:
        if profile_block(generated, name) != profile_block(baseline, name):
            raise ValueError(f"Perfil técnico alterado indevidamente: climate.{name}")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(generated, encoding="utf-8")

    print(f"Perfil climático atualizado: {OUTPUT}")
    for name in CALIBRATION:
        values = CALIBRATION[name]
        print(
            f"climate.{name}: mau tempo={values['bad_weather_factor']}, "
            f"molhamento={values['wetting_factor']}, secagem={values['drying_factor']}"
        )
    print("Perfis técnicos preservados sem alterações.")


if __name__ == "__main__":
    main()
