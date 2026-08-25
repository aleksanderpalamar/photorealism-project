#!/usr/bin/env python3
"""Compatibilidade: use build_nice.py para reconstruir todos os passes."""

from build_nice import main


if __name__ == "__main__":
    print("Aviso: calibrate_midday.py agora executa a pipeline completa de nice.sii.")
    main()

