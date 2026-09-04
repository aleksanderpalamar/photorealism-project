#!/usr/bin/env python3
"""Mede o perfil de glow em volta das fontes de luz de uma captura.

Irmao do grade_report.py, e pela mesma razao: a serie 0.13.x inteira foi
calibrada no olho, e um efeito de cinco niveis em 255 sobreviveu tres versoes
sem ninguem notar que era invisivel. Bloom e ainda mais facil de errar no olho
-- exagerado ele parece "cinematografico" numa captura e vira nevoa em todas as
outras.

    ./tools/bloom_report.py referencia/*.png -- capturas/*.png

O que ele mede, e como isso vira parametro do modulo:

  nucleo      luminancia media do centro da fonte (o sol, o farol).
  fundo       luminancia longe da fonte, fora do alcance do glow. E o piso
              contra o qual o glow e medido.
  meia-queda  raio onde o glow cai a metade entre nucleo e fundo, em FRACAO DA
              ALTURA da imagem. Vira `radius`, e por ser fracao ele independe
              da resolucao da captura.
  lift        quanto o glow levanta o fundo no raio de meia-queda, em 0-255.
              Vira `intensity`.
  limiar      luminancia onde o perfil deixa de cair, ou seja onde o glow
              termina e sobra so a cena. Vira `threshold`.

O perfil e radial em volta da fonte mais brilhante de cada imagem. Se a imagem
tiver varias fontes separadas (dois farois, sol e reflexo), a coluna `fontes`
avisa: o perfil ali mistura as duas e o raio sai inflado.

LIMITE CONHECIDO, aprendido na 0.17.0 -- ESTE METODO SO VALE PARA FONTE
PONTUAL. Rodado sobre as cinco referencias do ATS ele devolveu `fontes` entre 4
e 9 e raios de 0,06 a 0,19 da altura, numeros sem sentido para um kernel. O
motivo: naquelas imagens o que e claro nao e uma fonte, e o CEU visto pela
janela -- uma area grande. Perfilar radialmente em volta dela mede o gradiente
geral da cena (a cabine escurece longe da janela), e nao o espalhamento da luz.

Quando `fontes` vier alto, o numero NAO e um raio de bloom. O que serve nesse
caso e medir o perfil atravessando uma borda de alto contraste em resolucao
cheia: com bloom o lado escuro sobe suavemente antes da borda, com uma cauda;
sem bloom ele fica plano ate a transicao. Foi assim que se estabeleceu que as
referencias do ATS nao tem bloom.
"""

import math
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Erro: este script precisa do Pillow (pip install Pillow).")

# Resolucao de trabalho. Menor que isto e o perfil radial fica com poucos
# pixels por anel e vira ruido; maior nao muda a forma da curva e so custa
# tempo. A altura e o que normaliza o raio, entao ela e o numero que importa.
SAMPLE = (640, 360)

# Um pixel entra no nucleo se estiver nesta fracao do topo da imagem. Fontes de
# luz reais na cena (sol, farol, reflexo especular) ficam bem acima disso; ceu
# claro e capo branco nao.
CORE_QUANTILE = 0.999

# Aneis do perfil, em pixels da imagem reduzida.
RING_COUNT = 48


def luminance_map(path):
    """Devolve (largura, altura, lista de luminancia 0-255)."""
    image = Image.open(path).convert("L").resize(SAMPLE)
    return image.width, image.height, list(image.tobytes())


def find_core(width, height, grey):
    """Centroide das fontes mais brilhantes, e quantos grupos separados ha.

    O centroide e ponderado pela luminancia, entao um sol grande domina um
    reflexo pequeno em vez de a media cair no meio dos dois. A contagem de
    grupos existe justamente para denunciar quando isso nao basta.
    """
    ordered = sorted(grey)
    cutoff = ordered[min(int(len(ordered) * CORE_QUANTILE), len(ordered) - 1)]
    # Um teto uniforme (imagem chapada) nao tem fonte identificavel.
    if cutoff <= ordered[0]:
        return None

    points = [
        (index % width, index // width, value)
        for index, value in enumerate(grey)
        if value >= cutoff
    ]
    if not points:
        return None

    total = sum(weight for _, _, weight in points)
    center_x = sum(x * weight for x, _, weight in points) / total
    center_y = sum(y * weight for _, y, weight in points) / total

    # Grupos separados: dois pontos do nucleo a mais de 5% da altura um do
    # outro contam como fontes distintas. E uma contagem grosseira de uniao
    # por proximidade, suficiente para levantar a bandeira.
    span = height * 0.05
    clusters = []
    for x, y, _ in points:
        for cluster in clusters:
            if math.hypot(x - cluster[0], y - cluster[1]) <= span:
                break
        else:
            clusters.append((x, y))

    return center_x, center_y, len(clusters)


def radial_profile(width, height, grey, center_x, center_y):
    """Luminancia media por anel de distancia em volta do centro."""
    # O raio maximo util e a maior distancia do centro a um canto: alem disso
    # os aneis ficam parciais e a media se enviesa para o lado mais proximo.
    limit = min(
        max(center_x, width - center_x), max(center_y, height - center_y)
    )
    step = limit / RING_COUNT

    sums = [0.0] * RING_COUNT
    counts = [0] * RING_COUNT
    for index, value in enumerate(grey):
        x = index % width
        y = index // width
        distance = math.hypot(x - center_x, y - center_y)
        ring = int(distance / step)
        if ring < RING_COUNT:
            sums[ring] += value
            counts[ring] += 1

    return [
        (sums[ring] / counts[ring] if counts[ring] else None)
        for ring in range(RING_COUNT)
    ], step


def measure(path):
    width, height, grey = luminance_map(path)
    core = find_core(width, height, grey)
    if core is None:
        return None

    center_x, center_y, sources = core
    profile, step = radial_profile(width, height, grey, center_x, center_y)
    valid = [value for value in profile if value is not None]
    if len(valid) < 4:
        return None

    core_level = valid[0]
    # O fundo e o quarto externo do perfil, onde o glow ja acabou. Media em vez
    # do ultimo anel para nao pendurar a medida num unico valor ruidoso.
    tail = valid[len(valid) * 3 // 4:]
    background = sum(tail) / len(tail)

    span = core_level - background
    if span <= 0.0:
        return None

    # Meia-queda: primeiro anel abaixo do ponto medio entre nucleo e fundo.
    half = background + span * 0.5
    half_ring = next(
        (ring for ring, value in enumerate(profile)
         if value is not None and value <= half),
        None,
    )
    if half_ring is None:
        return None
    half_radius = half_ring * step

    # Onde o perfil para de cair: primeiro anel a menos de 2 niveis acima do
    # fundo. E ali que o glow termina, e a luminancia daquele ponto e o piso
    # que o limiar tem que respeitar.
    edge_ring = next(
        (ring for ring, value in enumerate(profile)
         if value is not None and value - background <= 2.0),
        len(profile) - 1,
    )

    lift = (profile[half_ring] or background) - background

    return {
        "core": core_level,
        "background": background,
        "half_fraction": half_radius / height,
        "lift": lift,
        "edge_fraction": edge_ring * step / height,
        "threshold": background + span * 0.5,
        "sources": sources,
    }


def print_table(title, paths):
    print(f"\n{title}")
    header = (
        f"{'arquivo':<34}{'nucleo':>8}{'fundo':>8}{'meia':>8}"
        f"{'lift':>7}{'borda':>8}{'limiar':>8}{'fontes':>8}"
    )
    print(header)
    print("-" * len(header))
    for path in paths:
        try:
            result = measure(path)
        except OSError as error:
            print(f"{Path(path).name:<34}  erro: {error}")
            continue
        if result is None:
            print(f"{Path(path).name:<34}  sem fonte identificavel")
            continue
        print(
            f"{Path(path).name:<34}"
            f"{result['core']:>8.1f}"
            f"{result['background']:>8.1f}"
            f"{result['half_fraction']:>8.3f}"
            f"{result['lift']:>7.1f}"
            f"{result['edge_fraction']:>8.3f}"
            f"{result['threshold']:>8.1f}"
            f"{result['sources']:>8d}"
        )


def main(argv):
    if "--" in argv:
        split = argv.index("--")
        references = argv[:split]
        captures = argv[split + 1:]
    else:
        references = argv
        captures = []

    if not references:
        sys.exit(__doc__)

    print_table("REFERENCIA", references)
    if captures:
        print_table("CAPTURA", captures)

    print(
        "\nmeia/borda sao FRACAO DA ALTURA: 0.050 quer dizer que o glow cai a\n"
        "metade a 5% da altura da tela do centro da fonte. E o numero que vira\n"
        "`radius`, e por ser fracao ele vale em qualquer resolucao.\n"
        "\nfontes>1 quer dizer que o perfil mistura fontes separadas e o raio\n"
        "sai inflado; nesse caso vale recortar a imagem em volta de uma so."
    )


if __name__ == "__main__":
    main(sys.argv[1:])
