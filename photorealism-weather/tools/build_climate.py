#!/usr/bin/env python3
"""Reconstrói clima, iluminação, gotas no vidro e spray do Photorealism."""

from build_bad import main as build_bad
from build_climate_profile import main as build_climate_profile
from build_nice import main as build_nice
from build_rain import main as build_rain


if __name__ == "__main__":
    print("== Frequência, molhamento e secagem ==")
    build_climate_profile()
    print("== Tempo bom ==")
    build_nice()
    print("\n== Mau tempo ==")
    build_bad()
    print("\n== Gotas e spray ==")
    build_rain()
