#!/usr/bin/env python3
"""Reconstrói o conteúdo do mod a partir das baselines oficiais do ETS2 1.60."""

from __future__ import annotations

import hashlib
import json
import re
import shutil
from decimal import Decimal
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
SOURCE_DIR = PROJECT_DIR / "source"
BASELINE_DIR = SOURCE_DIR / "baseline"
PROFILE_PATH = SOURCE_DIR / "lights_profile.json"
MOD_DIR = PROJECT_DIR / "mod"
GENERATED_ROOTS = ("def", "unit")
STREET_FLARE_FILES = (
    "unit/hookup/lights/street_lamp.sii",
    "unit/hookup/lights/street_lamp_b.sii",
    "unit/hookup/lights/street_lamp_n.sii",
    "unit/hookup/lights/street_lamp_y.sii",
    "unit/hookup/lights/flares/flare_street_lamp_b.sii",
    "unit/hookup/lights/flares/flare_street_lamp_b_wide.sii",
    "unit/hookup/lights/flares/flare_street_lamp_n.sii",
    "unit/hookup/lights/flares/flare_street_lamp_n_s1.sii",
    "unit/hookup/lights/flares/flare_street_lamp_n_wide.sii",
    "unit/hookup/lights/flares/flare_street_lamp_y.sii",
    "unit/hookup/lights/flares/flare_street_lamp_y_s1.sii",
    "unit/hookup/lights/flares/flare_street_lamp_y_wide.sii",
)
EXPECTED_STREET_FLARE_VALUES = 69
STREET_LIGHT_SOURCE_FILES = (
    "unit/hookup/lights/street_lamp.sii",
    "unit/hookup/lights/street_lamp_b.sii",
    "unit/hookup/lights/street_lamp_n.sii",
    "unit/hookup/lights/street_lamp_y.sii",
)
EXPECTED_SOURCE_UNITS = 61
TRAFFIC_LIGHT_FLARE_FILE = "unit/hookup/tr_light_flares.sii"
EXPECTED_TRAFFIC_LIGHT_FLARES = 71
VEHICLE_REAR_SIGNAL_FLARE_FILES = (
    "unit/hookup/vehicle/flare/vehicle_rearl.sii",
    "unit/hookup/vehicle/flare/vehicle_brakel.sii",
    "unit/hookup/vehicle/flare/vehicle_lblinkerl.sii",
    "unit/hookup/vehicle/flare/vehicle_rblinkerl.sii",
)
EXPECTED_VEHICLE_REAR_SIGNAL_FLARES = 4
VEHICLE_FRONT_LIGHT_FLARE_FILES = (
    "unit/hookup/vehicle/flare/vehicle_parkl.sii",
    "unit/hookup/vehicle/flare/vehicle_headl.sii",
    "unit/hookup/vehicle/flare/vehicle_high_beam.sii",
    "unit/hookup/vehicle/flare/vehicle_aux_lights.sii",
)
EXPECTED_VEHICLE_FRONT_LIGHT_FLARES = 4
HEADLIGHT_PROFILE_GLOB = "def/vehicle/truck/*/head_light/*.sii"
EXPECTED_HEADLIGHT_PROFILES = 47
SCALE_PATTERN = re.compile(
    r"(?m)^(?P<prefix>\s*scale_factor:\s*)(?P<value>[0-9]+(?:\.[0-9]+)?)"
)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_profile(profile: dict[str, object]) -> None:
    if profile.get("profile_version") != "0.8.0":
        raise ValueError("o passe de auxiliares deve usar profile_version 0.8.0")
    if profile.get("game_version") != "1.60":
        raise ValueError("o perfil foi preparado para o ETS2 1.60")
    if profile.get("mode") != "vehicle_auxiliary_projection_pass":
        raise ValueError("o passe 0.8.0 exige mode=vehicle_auxiliary_projection_pass")

    for group_name in ("street_lamps", "traffic_lights", "vehicle_lights"):
        group = profile.get(group_name)
        if not isinstance(group, dict) or not group:
            raise ValueError(f"grupo de perfil ausente: {group_name}")
        for field, value in group.items():
            if not isinstance(value, (int, float)) or isinstance(value, bool):
                raise ValueError(f"multiplicador invalido em {group_name}.{field}")

    street_lamps = profile["street_lamps"]
    if not isinstance(street_lamps, dict):
        raise TypeError("street_lamps deve ser um objeto")
    flare_multiplier = street_lamps.get("flare_scale_multiplier")
    if flare_multiplier != 1.15:
        raise ValueError("o flare consolidado deve permanecer em 1.15x")

    range_multiplier = street_lamps.get("light_range_multiplier")
    if not isinstance(range_multiplier, (int, float)) or not 1.0 < range_multiplier <= 1.1:
        raise ValueError("light_range_multiplier deve estar entre 1.0 e 1.1")

    flux_multiplier = street_lamps.get("luminous_flux_multiplier")
    if not isinstance(flux_multiplier, (int, float)) or not 1.0 < flux_multiplier <= 1.1:
        raise ValueError("luminous_flux_multiplier deve estar entre 1.0 e 1.1")

    traffic_lights = profile["traffic_lights"]
    if not isinstance(traffic_lights, dict):
        raise TypeError("traffic_lights deve ser um objeto")
    traffic_flare_multiplier = traffic_lights.get("flare_scale_multiplier")
    if (
        not isinstance(traffic_flare_multiplier, (int, float))
        or not 1.0 < traffic_flare_multiplier <= 1.15
    ):
        raise ValueError("traffic_lights.flare_scale_multiplier deve estar entre 1.0 e 1.15")
    if traffic_lights.get("fade_distance_multiplier") != 1.0:
        raise ValueError("a distancia de visibilidade dos semaforos deve permanecer oficial")

    vehicle_lights = profile["vehicle_lights"]
    if not isinstance(vehicle_lights, dict):
        raise TypeError("vehicle_lights deve ser um objeto")
    rear_signal_multiplier = vehicle_lights.get("rear_signal_flare_multiplier")
    if (
        not isinstance(rear_signal_multiplier, (int, float))
        or rear_signal_multiplier != 1.08
    ):
        raise ValueError("o passe traseiro consolidado deve permanecer em 1.08x")

    front_light_multiplier = vehicle_lights.get("front_light_flare_multiplier")
    if (
        not isinstance(front_light_multiplier, (int, float))
        or not 1.0 < front_light_multiplier <= 1.08
    ):
        raise ValueError("front_light_flare_multiplier deve estar entre 1.0 e 1.08")
    if vehicle_lights.get("default_scale_multiplier") != 1.0:
        raise ValueError("a escala-base das luzes dos veiculos deve permanecer oficial")

    for field in ("low_beam_range_multiplier", "high_beam_range_multiplier"):
        value = vehicle_lights.get(field)
        if not isinstance(value, (int, float)) or not 1.0 < value <= 1.08:
            raise ValueError(f"{field} deve estar entre 1.0 e 1.08")

    projected_flux_multiplier = vehicle_lights.get("projected_beam_flux_multiplier")
    if (
        not isinstance(projected_flux_multiplier, (int, float))
        or not 1.0 < projected_flux_multiplier <= 1.05
    ):
        raise ValueError("projected_beam_flux_multiplier deve estar entre 1.0 e 1.05")

    auxiliary_limits = {
        "front_aux_range_multiplier": 1.05,
        "roof_aux_range_multiplier": 1.06,
        "combined_aux_range_multiplier": 1.06,
        "front_aux_flux_multiplier": 1.04,
        "roof_aux_flux_multiplier": 1.04,
        "combined_aux_flux_multiplier": 1.04,
    }
    for field, upper_limit in auxiliary_limits.items():
        value = vehicle_lights.get(field)
        if not isinstance(value, (int, float)) or not 1.0 < value <= upper_limit:
            raise ValueError(f"{field} deve estar entre 1.0 e {upper_limit}")


def format_decimal(value: Decimal) -> str:
    formatted = format(value.quantize(Decimal("0.001")), "f")
    return formatted.rstrip("0").rstrip(".")


def format_decimal_precise(value: Decimal) -> str:
    formatted = format(value.quantize(Decimal("0.000001")), "f")
    return formatted.rstrip("0").rstrip(".")


def multiply_scale_factors(path: Path, multiplier: Decimal) -> int:
    text = path.read_text(encoding="utf-8")

    def replacement(match: re.Match[str]) -> str:
        original = Decimal(match.group("value"))
        calibrated = format_decimal(original * multiplier)
        return f"{match.group('prefix')}{calibrated}"

    output, count = SCALE_PATTERN.subn(replacement, text)
    if count == 0:
        raise ValueError(f"nenhum scale_factor encontrado em {path}")
    path.write_text(output, encoding="utf-8")
    return count


def multiply_scalar_field(path: Path, field: str, multiplier: Decimal) -> int:
    pattern = re.compile(
        rf"(?m)^(?P<prefix>\s*{re.escape(field)}:\s*)"
        r"(?P<value>[0-9]+(?:\.[0-9]+)?)"
    )
    text = path.read_text(encoding="utf-8")

    def replacement(match: re.Match[str]) -> str:
        original = Decimal(match.group("value"))
        return f"{match.group('prefix')}{format_decimal(original * multiplier)}"

    output, count = pattern.subn(replacement, text)
    if count == 0:
        raise ValueError(f"campo {field} ausente em {path}")
    path.write_text(output, encoding="utf-8")
    return count


def multiply_color_flux(path: Path, field: str, multiplier: Decimal) -> int:
    pattern = re.compile(
        rf"(?m)^(?P<prefix>\s*{re.escape(field)}:\s*\()"
        r"(?P<value>[0-9]+(?:\.[0-9]+)?)(?P<suffix>,)"
    )
    text = path.read_text(encoding="utf-8")

    def replacement(match: re.Match[str]) -> str:
        original = Decimal(match.group("value"))
        return (
            f"{match.group('prefix')}"
            f"{format_decimal(original * multiplier)}{match.group('suffix')}"
        )

    output, count = pattern.subn(replacement, text)
    if count == 0:
        raise ValueError(f"campo {field} ausente em {path}")
    path.write_text(output, encoding="utf-8")
    return count


def multiply_rgb_field(path: Path, field: str, multiplier: Decimal) -> int:
    number = r"-?[0-9]+(?:\.[0-9]+)?"
    pattern = re.compile(
        rf"(?m)^(?P<prefix>\s*{re.escape(field)}:\s*\()"
        rf"(?P<red>{number})(?P<sep1>,\s*)"
        rf"(?P<green>{number})(?P<sep2>,\s*)"
        rf"(?P<blue>{number})(?P<suffix>\)\s*)$"
    )
    text = path.read_text(encoding="utf-8")

    def replacement(match: re.Match[str]) -> str:
        red = format_decimal_precise(Decimal(match.group("red")) * multiplier)
        green = format_decimal_precise(Decimal(match.group("green")) * multiplier)
        blue = format_decimal_precise(Decimal(match.group("blue")) * multiplier)
        return (
            f"{match.group('prefix')}{red}{match.group('sep1')}"
            f"{green}{match.group('sep2')}{blue}{match.group('suffix')}"
        )

    output, count = pattern.subn(replacement, text)
    if count == 0:
        raise ValueError(f"campo RGB {field} ausente em {path}")
    path.write_text(output, encoding="utf-8")
    return count


def main() -> None:
    profile = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
    validate_profile(profile)

    for root_name in GENERATED_ROOTS:
        source_root = BASELINE_DIR / root_name
        target_root = MOD_DIR / root_name
        if not source_root.is_dir():
            raise FileNotFoundError(f"baseline ausente: {source_root}")
        if target_root.exists():
            shutil.rmtree(target_root)
        shutil.copytree(source_root, target_root)

    baseline_files = sorted(path for path in BASELINE_DIR.rglob("*") if path.is_file())
    if not baseline_files:
        raise ValueError("nenhum arquivo encontrado na baseline")

    for source_path in baseline_files:
        relative_path = source_path.relative_to(BASELINE_DIR)
        target_path = MOD_DIR / relative_path
        if not target_path.is_file():
            raise FileNotFoundError(f"arquivo gerado ausente: {relative_path}")
        if file_sha256(source_path) != file_sha256(target_path):
            raise ValueError(f"baseline alterada durante a copia: {relative_path}")

    street_lamps = profile["street_lamps"]
    if not isinstance(street_lamps, dict):
        raise TypeError("street_lamps deve ser um objeto")
    multiplier = Decimal(str(street_lamps["flare_scale_multiplier"]))
    changed_values = 0
    for relative_path in STREET_FLARE_FILES:
        changed_values += multiply_scale_factors(MOD_DIR / relative_path, multiplier)

    if changed_values != EXPECTED_STREET_FLARE_VALUES:
        raise ValueError(
            "quantidade inesperada de flares de postes alterados: "
            f"{changed_values} (esperado {EXPECTED_STREET_FLARE_VALUES})"
        )

    range_multiplier = Decimal(str(street_lamps["light_range_multiplier"]))
    flux_multiplier = Decimal(str(street_lamps["luminous_flux_multiplier"]))
    range_values = 0
    cut_range_values = 0
    diffuse_values = 0
    specular_values = 0
    for relative_path in STREET_LIGHT_SOURCE_FILES:
        target_path = MOD_DIR / relative_path
        range_values += multiply_scalar_field(target_path, "range", range_multiplier)
        cut_range_values += multiply_scalar_field(
            target_path, "cut_range", range_multiplier
        )
        diffuse_values += multiply_color_flux(
            target_path, "diffuse_color", flux_multiplier
        )
        specular_values += multiply_color_flux(
            target_path, "specular_color", flux_multiplier
        )

    projection_counts = (
        range_values,
        cut_range_values,
        diffuse_values,
        specular_values,
    )
    if any(count != EXPECTED_SOURCE_UNITS for count in projection_counts):
        raise ValueError(
            "quantidade inesperada de fontes alteradas: "
            f"range={range_values}, cut_range={cut_range_values}, "
            f"diffuse={diffuse_values}, specular={specular_values}"
        )

    traffic_lights = profile["traffic_lights"]
    if not isinstance(traffic_lights, dict):
        raise TypeError("traffic_lights deve ser um objeto")
    traffic_flare_multiplier = Decimal(
        str(traffic_lights["flare_scale_multiplier"])
    )
    traffic_flare_values = multiply_scale_factors(
        MOD_DIR / TRAFFIC_LIGHT_FLARE_FILE, traffic_flare_multiplier
    )
    if traffic_flare_values != EXPECTED_TRAFFIC_LIGHT_FLARES:
        raise ValueError(
            "quantidade inesperada de flares de semaforos alterados: "
            f"{traffic_flare_values} (esperado {EXPECTED_TRAFFIC_LIGHT_FLARES})"
        )

    vehicle_lights = profile["vehicle_lights"]
    if not isinstance(vehicle_lights, dict):
        raise TypeError("vehicle_lights deve ser um objeto")
    rear_signal_multiplier = Decimal(
        str(vehicle_lights["rear_signal_flare_multiplier"])
    )
    vehicle_flare_values = 0
    for relative_path in VEHICLE_REAR_SIGNAL_FLARE_FILES:
        vehicle_flare_values += multiply_scale_factors(
            MOD_DIR / relative_path, rear_signal_multiplier
        )
    if vehicle_flare_values != EXPECTED_VEHICLE_REAR_SIGNAL_FLARES:
        raise ValueError(
            "quantidade inesperada de flares de sinalizacao traseira: "
            f"{vehicle_flare_values} "
            f"(esperado {EXPECTED_VEHICLE_REAR_SIGNAL_FLARES})"
        )

    front_light_multiplier = Decimal(
        str(vehicle_lights["front_light_flare_multiplier"])
    )
    front_light_flare_values = 0
    for relative_path in VEHICLE_FRONT_LIGHT_FLARE_FILES:
        front_light_flare_values += multiply_scale_factors(
            MOD_DIR / relative_path, front_light_multiplier
        )
    if front_light_flare_values != EXPECTED_VEHICLE_FRONT_LIGHT_FLARES:
        raise ValueError(
            "quantidade inesperada de flares dianteiros: "
            f"{front_light_flare_values} "
            f"(esperado {EXPECTED_VEHICLE_FRONT_LIGHT_FLARES})"
        )

    headlight_profiles = sorted(BASELINE_DIR.glob(HEADLIGHT_PROFILE_GLOB))
    if len(headlight_profiles) != EXPECTED_HEADLIGHT_PROFILES:
        raise ValueError(
            "quantidade inesperada de perfis de farois na baseline: "
            f"{len(headlight_profiles)} (esperado {EXPECTED_HEADLIGHT_PROFILES})"
        )

    low_range_multiplier = Decimal(
        str(vehicle_lights["low_beam_range_multiplier"])
    )
    high_range_multiplier = Decimal(
        str(vehicle_lights["high_beam_range_multiplier"])
    )
    projected_flux_multiplier = Decimal(
        str(vehicle_lights["projected_beam_flux_multiplier"])
    )
    projection_counts = {
        "low_beam_range": 0,
        "hi_beam_range": 0,
        "low_beam_color": 0,
        "low_beam_color_specular": 0,
        "hi_beam_color": 0,
        "hi_beam_color_specular": 0,
    }
    for baseline_path in headlight_profiles:
        target_path = MOD_DIR / baseline_path.relative_to(BASELINE_DIR)
        projection_counts["low_beam_range"] += multiply_scalar_field(
            target_path, "low_beam_range", low_range_multiplier
        )
        projection_counts["hi_beam_range"] += multiply_scalar_field(
            target_path, "hi_beam_range", high_range_multiplier
        )
        for field in (
            "low_beam_color",
            "low_beam_color_specular",
            "hi_beam_color",
            "hi_beam_color_specular",
        ):
            projection_counts[field] += multiply_rgb_field(
                target_path, field, projected_flux_multiplier
            )

    if any(
        count != EXPECTED_HEADLIGHT_PROFILES
        for count in projection_counts.values()
    ):
        raise ValueError(
            "quantidade inesperada de campos projetados alterados: "
            f"{projection_counts}"
        )

    auxiliary_groups = {
        "front": (
            "front_beam_range",
            Decimal(str(vehicle_lights["front_aux_range_multiplier"])),
            Decimal(str(vehicle_lights["front_aux_flux_multiplier"])),
        ),
        "roof": (
            "roof_beam_range",
            Decimal(str(vehicle_lights["roof_aux_range_multiplier"])),
            Decimal(str(vehicle_lights["roof_aux_flux_multiplier"])),
        ),
        "front_roof": (
            "front_roof_beam_range",
            Decimal(str(vehicle_lights["combined_aux_range_multiplier"])),
            Decimal(str(vehicle_lights["combined_aux_flux_multiplier"])),
        ),
    }
    auxiliary_counts: dict[str, dict[str, int]] = {}
    for group_name, (range_field, aux_range_multiplier, aux_flux_multiplier) in (
        auxiliary_groups.items()
    ):
        color_field = f"{group_name}_beam_color"
        specular_field = f"{group_name}_beam_color_specular"
        group_counts = {"range": 0, "color": 0, "specular": 0}
        for baseline_path in headlight_profiles:
            target_path = MOD_DIR / baseline_path.relative_to(BASELINE_DIR)
            group_counts["range"] += multiply_scalar_field(
                target_path, range_field, aux_range_multiplier
            )
            group_counts["color"] += multiply_rgb_field(
                target_path, color_field, aux_flux_multiplier
            )
            group_counts["specular"] += multiply_rgb_field(
                target_path, specular_field, aux_flux_multiplier
            )
        auxiliary_counts[group_name] = group_counts

    invalid_auxiliary_counts = {
        group_name: counts
        for group_name, counts in auxiliary_counts.items()
        if any(count != EXPECTED_HEADLIGHT_PROFILES for count in counts.values())
    }
    if invalid_auxiliary_counts:
        raise ValueError(
            "quantidade inesperada de campos auxiliares alterados: "
            f"{invalid_auxiliary_counts}"
        )

    print(
        f"Photorealism Lights {profile['profile_version']} gerado: "
        f"flare {multiplier}x preservado; {EXPECTED_SOURCE_UNITS} fontes com "
        f"alcance {range_multiplier}x e fluxo direto/especular {flux_multiplier}x; "
        f"{traffic_flare_values} flares de semaforos em "
        f"{traffic_flare_multiplier}x; {vehicle_flare_values} flares veiculares "
        f"traseiros em {rear_signal_multiplier}x; {front_light_flare_values} "
        f"flares dianteiros em {front_light_multiplier}x; "
        f"{EXPECTED_HEADLIGHT_PROFILES} perfis com alcance baixo/alto em "
        f"{low_range_multiplier}x/{high_range_multiplier}x e fluxo em "
        f"{projected_flux_multiplier}x; auxiliares dianteiros/teto/combinados "
        f"com alcance em "
        f"{auxiliary_groups['front'][1]}x/{auxiliary_groups['roof'][1]}x/"
        f"{auxiliary_groups['front_roof'][1]}x e fluxo em "
        f"{auxiliary_groups['front'][2]}x/{auxiliary_groups['roof'][2]}x/"
        f"{auxiliary_groups['front_roof'][2]}x"
    )


if __name__ == "__main__":
    main()
