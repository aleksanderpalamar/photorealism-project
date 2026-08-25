"""Funções pequenas para transformar arrays numéricos dos perfis de clima SCS."""

from __future__ import annotations

import re
import struct


PROFILE_RE = re.compile(r"^sun_profile\s*:\s*default\.(nice|bad)\.(\d{2})\s*\{")
ARRAY_VALUE_RE = re.compile(r"^(\s*)([a-zA-Z_][a-zA-Z0-9_]*)\[\d+\]:\s*(.*?)\s*$")
NUMBER_RE = re.compile(
    r"&[0-9a-fA-F]{8}|[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
)


def parse_float(token: str) -> float:
    if token.startswith("&"):
        bits = int(token[1:], 16)
        return struct.unpack(">f", struct.pack(">I", bits))[0]
    return float(token)


def encode_float(value: float) -> str:
    if value == 0.0:
        return "0"
    bits = struct.unpack(">I", struct.pack(">f", value))[0]
    return f"&{bits:08x}"


def scale_numbers(value: str, factor: float) -> str:
    def replace(match: re.Match[str]) -> str:
        return encode_float(parse_float(match.group(0)) * factor)

    return NUMBER_RE.sub(replace, value)


def apply_profile_pass(
    text: str,
    first_profile: int,
    last_profile: int,
    field_scales: dict[str, float],
    weather_type: str = "nice",
) -> tuple[str, dict[str, int]]:
    """Aplica multiplicadores aos arrays de um intervalo de perfis."""

    current_profile: int | None = None
    changes = {field: 0 for field in field_scales}
    output_lines: list[str] = []

    for line in text.splitlines(keepends=True):
        profile_match = PROFILE_RE.match(line)
        if profile_match:
            profile_weather = profile_match.group(1)
            current_profile = (
                int(profile_match.group(2)) if profile_weather == weather_type else None
            )

        stripped = line.rstrip("\r\n")
        ending = line[len(stripped) :]
        value_match = ARRAY_VALUE_RE.match(stripped)

        if (
            current_profile is not None
            and first_profile <= current_profile <= last_profile
            and value_match
            and value_match.group(2) in field_scales
        ):
            indent, field, value = value_match.groups()
            index = stripped[len(indent) + len(field) :].split(":", 1)[0]
            scaled = scale_numbers(value, field_scales[field])
            line = f"{indent}{field}{index}: {scaled}{ending}"
            changes[field] += 1

        output_lines.append(line)

    missing = [field for field, count in changes.items() if count == 0]
    if missing:
        raise RuntimeError(f"Campos esperados não encontrados: {', '.join(missing)}")

    return "".join(output_lines), changes


def replace_profile_texture_paths(
    text: str,
    profile_number: int,
    replacements: dict[str, str],
    weather_type: str = "nice",
    expected_entries: int = 13,
) -> tuple[str, dict[str, int]]:
    """Substitui arrays de textura de um único perfil solar."""

    current_profile: int | None = None
    changes = {field: 0 for field in replacements}
    output_lines: list[str] = []

    for line in text.splitlines(keepends=True):
        profile_match = PROFILE_RE.match(line)
        if profile_match:
            profile_weather = profile_match.group(1)
            current_profile = (
                int(profile_match.group(2)) if profile_weather == weather_type else None
            )

        stripped = line.rstrip("\r\n")
        ending = line[len(stripped) :]
        value_match = ARRAY_VALUE_RE.match(stripped)

        if (
            current_profile == profile_number
            and value_match
            and value_match.group(2) in replacements
        ):
            indent, field, _ = value_match.groups()
            index = stripped[len(indent) + len(field) :].split(":", 1)[0]
            line = f'{indent}{field}{index}: "{replacements[field]}"{ending}'
            changes[field] += 1

        output_lines.append(line)

    unexpected = {
        field: count for field, count in changes.items() if count != expected_entries
    }
    if unexpected:
        details = ", ".join(f"{field}={count}" for field, count in unexpected.items())
        raise RuntimeError(
            f"Quantidade inesperada de texturas no perfil {profile_number:02d}: {details}"
        )

    return "".join(output_lines), changes
