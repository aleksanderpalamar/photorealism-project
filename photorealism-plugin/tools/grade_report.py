#!/usr/bin/env python3
"""Mede o histograma de capturas e imprime a tabela que julga o grading.

Toda a serie 0.13.x foi calibrada no olho, e foi assim que um efeito de cinco
niveis em 255 sobreviveu tres versoes sem ninguem notar que era invisivel. Este
script existe para que a proxima rodada seja medida.

    ./tools/grade_report.py referencia/*.png -- capturas/*.png

Os numeros que importam, e por que:

  p1    o 1% mais escuro. E o unico numero que sozinho separa o visual da
        referencia do nosso: as referencias ficam em 8-11, e o plugin em 0
        porque o saturate() final esmaga sem toe nenhum.
  med   mediana. A referencia e MAIS ESCURA que o plugin (11-40 contra 47-70),
        o que contradiz a leitura de que faltava luz na cabine.
  p99   o topo. Rolando (valores distintos perto do teto) e diferente de
        cortado (um plato em 255).
  R/G/B media por canal. A referencia puxa verde nas quatro imagens; o plugin
        puxa azul nas tres.
"""

import statistics
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Erro: este script precisa do Pillow (pip install Pillow).")

# Amostra reduzida: o histograma de uma imagem 4K nao muda de forma util em
# 480x270, e a leitura fica instantanea.
SAMPLE = (480, 270)


def measure(path):
    image = Image.open(path)
    # tobytes() em vez de getdata(): mesmo resultado, sem o iterador lento e
    # sem o aviso de depreciacao do Pillow 14.
    grey = sorted(image.convert("L").resize(SAMPLE).tobytes())
    count = len(grey)

    def percentile(fraction):
        return grey[min(int(count * fraction), count - 1)]

    packed = image.convert("RGB").resize(SAMPLE).tobytes()
    channels = [
        sum(packed[offset::3]) / (len(packed) / 3) for offset in range(3)
    ]

    # Plato no topo: quantos pixels ja estao no maximo. Distingue um ceu que
    # rola de um ceu que corta -- e a diferenca entre ombro e saturate().
    clipped = sum(1 for value in grey if value >= 254) / count * 100.0

    return {
        "p1": percentile(0.01),
        "p5": percentile(0.05),
        "med": percentile(0.50),
        "p95": percentile(0.95),
        "p99": percentile(0.99),
        "mean": sum(grey) / count,
        "rms": statistics.pstdev(grey),
        "rgb": channels,
        "clipped": clipped,
    }


def report(paths, title):
    if not paths:
        return
    print(f"=== {title} ===")
    print(
        f"{'arquivo':38s} {'p1':>4s} {'p5':>4s} {'med':>4s} {'p95':>4s} "
        f"{'p99':>4s} {'media':>6s} {'rms':>5s} {'topo%':>6s}  R/G/B"
    )
    for path in paths:
        data = measure(path)
        red, green, blue = data["rgb"]
        dominant = "RGB"[data["rgb"].index(max(data["rgb"]))]
        print(
            f"{Path(path).name[:38]:38s} "
            f"{data['p1']:4d} {data['p5']:4d} {data['med']:4d} "
            f"{data['p95']:4d} {data['p99']:4d} {data['mean']:6.1f} "
            f"{data['rms']:5.1f} {data['clipped']:6.2f}  "
            f"{red:5.1f}/{green:5.1f}/{blue:5.1f} -> {dominant}"
        )
    print()


def main(argv):
    if not argv:
        sys.exit(__doc__)
    # "--" separa referencia de captura. Sem ele, tudo e uma lista so.
    if "--" in argv:
        split = argv.index("--")
        report(argv[:split], "REFERENCIA")
        report(argv[split + 1:], "PLUGIN")
    else:
        report(argv, "CAPTURAS")
    print("Alvo medido nas referencias: p1 entre 6 e 12, G como canal mais")
    print("alto de dia, e topo% baixo (ceu rolando, nao cortado).")


if __name__ == "__main__":
    main(sys.argv[1:])
