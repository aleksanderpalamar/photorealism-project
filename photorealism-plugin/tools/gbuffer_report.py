#!/usr/bin/env python3
import json
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gbuffer_classify import channel_stats, classify
from gbuffer_dds import FORMAT_NAMES, encode_r11g11b10, read_dds, write_dds


def normalized(image):
    finite = image[np.isfinite(image)]
    if finite.size == 0:
        return np.zeros(image.shape, np.uint8)
    low, high = np.percentile(finite, [1, 99])
    scale = high - low if high - low > 1e-8 else 1.0
    return (np.clip((np.nan_to_num(image) - low) / scale, 0.0, 1.0) * 255.0).astype(np.uint8)


def save_previews(pixels, folder, stem):
    channels = pixels.shape[-1]
    height, width = pixels.shape[:2]
    scale = min(1.0, 480.0 / max(width, 1))
    size = (max(1, int(width * scale)), max(1, int(height * scale)))
    if channels >= 3:
        composite = normalized(pixels[..., :3])
    else:
        composite = np.stack([normalized(pixels[..., index % channels]) for index in range(3)], axis=-1)
        composite[..., 2] = 0 if channels == 2 else composite[..., 2]
    Image.fromarray(composite).resize(size, Image.NEAREST).save(folder / (stem + ".png"))
    strips = [np.pad(normalized(pixels[..., index]), ((0, 0), (0, 4)), constant_values=40) for index in range(channels)]
    Image.fromarray(np.concatenate(strips, axis=1)).resize((size[0] * channels, size[1]), Image.NEAREST).save(
        folder / (stem + "_canais.png"))


def report(capture_folder):
    folder = Path(capture_folder)
    manifest = json.loads((folder / "manifesto.json").read_text())
    previews = folder / "previews"
    previews.mkdir(exist_ok=True)
    lines = ["# Relatorio da captura %s" % folder.name, "",
             "%d binds, %d capturas, truncado=%s." % (len(manifest["binds"]), len(manifest["capturas"]), manifest["truncado"]),
             "", "| arquivo | binds | id | slot | tamanho | formato | etiquetas |", "|---|---|---|---|---|---|---|"]
    details = []
    for capture in manifest["capturas"]:
        if capture["falha"]:
            lines.append("| %s | %d-%d | %d | %s | %dx%d | %s | falha: %s |" % (
                capture["arquivo"], capture["primeiro_bind"], capture["ultimo_bind"], capture["id"], capture["slot"],
                capture["largura"], capture["altura"], capture["formato"], capture["falha"]))
            continue
        pixels, fmt = read_dds(folder / capture["arquivo"], capture["formato_view"])
        if pixels is None:
            lines.append("| %s | | | | | %d | formato sem decodificador |" % (capture["arquivo"], fmt))
            continue
        tags = classify(pixels, fmt, capture)
        stem = Path(capture["arquivo"]).stem
        save_previews(pixels, previews, stem)
        kind = "ds" if capture["depth"] else "rt"
        lines.append("| %s | %d-%d | %d | %s%d | %dx%d | %s | %s |" % (
            capture["arquivo"], capture["primeiro_bind"], capture["ultimo_bind"], capture["id"], kind, capture["slot"],
            capture["largura"], capture["altura"], FORMAT_NAMES.get(fmt, str(fmt)), " ".join(tags)))
        for index in range(pixels.shape[-1]):
            item = channel_stats(pixels[..., index])
            details.append("- `%s` c%d: min=%.4g p5=%.4g p50=%.4g p95=%.4g max=%.4g media=%.4g distintos=%d" % (
                stem, index, item["min"], item["p5"], item["p50"], item["p95"], item["max"], item["media"], item["distintos"]))
    lines += ["", "## Canais", ""] + details + ["", "## Binds", ""]
    for bind in manifest["binds"]:
        targets = ", ".join("%s%d=#%d %dx%d f%d" % ("ds" if t["depth"] else "rt", t["slot"], t["id"], t["largura"], t["altura"], t["formato"])
                            for t in bind["alvos"])
        lines.append("- b%03d: %s" % (bind["bind"], targets or "(nenhum alvo)"))
    (folder / "relatorio.md").write_text("\n".join(lines) + "\n")
    return folder / "relatorio.md"


def synthetic_capture(folder):
    height, width = 90, 160
    y, x = np.mgrid[0:height, 0:width].astype(np.float32)
    sky = y < 20
    angle = x / width * 3.0
    normals = np.stack([np.sin(angle) * 0.6, np.cos(y / height * 2.0) * 0.5, -np.ones_like(x)], axis=-1)
    normals /= np.linalg.norm(normals, axis=-1, keepdims=True)
    normals[sky] = 0.0
    albedo = np.stack([0.2 + 0.5 * x / width, 0.3 + 0.2 * y / height, 0.25 + 0.1 * np.sin(x / 9.0)], axis=-1)
    material = np.floor(x / width * 4.0) / 3.0
    lighting = albedo * (1.0 + 5.0 * (x > 120))[..., None]
    velocity = np.zeros((height, width, 2), np.float32)
    velocity[40:60, 50:80] = (0.01, 0.003)
    depth = np.where(sky, 0.0, 0.1 / (2.0 + y)).astype(np.float32)
    captures = [
        ("normal_float", 10, np.concatenate([normals, np.ones_like(x)[..., None]], axis=-1).astype(np.float16).tobytes(), ["normal_3c"], ["cor_ldr"]),
        ("normal_unorm", 28, (np.concatenate([normals * 0.5 + 0.5, np.ones_like(x)[..., None]], axis=-1) * 255).round().astype(np.uint8).tobytes(), ["normal_3c"], ["cor_ldr"]),
        ("albedo", 28, (np.concatenate([albedo, material[..., None]], axis=-1) * 255).round().astype(np.uint8).tobytes(), ["cor_ldr", "id_material_c3"], ["normal_3c", "hdr"]),
        ("iluminacao", 26, encode_r11g11b10(lighting).tobytes(), ["hdr"], ["cor_ldr", "normal_3c"]),
        ("velocidade", 34, velocity.astype(np.float16).tobytes(), ["possivel_velocidade"], ["hdr"]),
        ("profundidade", 19, np.concatenate([depth[..., None].view(np.uint8), np.zeros((height, width, 4), np.uint8)], axis=-1).tobytes(), ["profundidade"], []),
    ]
    entries = []
    for index, (name, fmt, data, _, _) in enumerate(captures):
        file_name = "%03d_%s.dds" % (index, name)
        write_dds(folder / file_name, data, width, height, fmt)
        entries.append({"primeiro_bind": index + 1, "ultimo_bind": index + 1, "slot": 0, "id": index + 1,
                        "depth": name == "profundidade", "largura": width, "altura": height, "formato": fmt,
                        "formato_view": fmt, "mip": 0, "fatia": 0, "amostras": 1, "arquivo": file_name, "falha": None})
    manifest = {"versao": "0.24.0", "truncado": False, "binds": [], "capturas": entries}
    (folder / "manifesto.json").write_text(json.dumps(manifest))
    return captures, lighting


def self_test():
    with tempfile.TemporaryDirectory() as temporary:
        folder = Path(temporary)
        captures, lighting = synthetic_capture(folder)
        decoded, _ = read_dds(folder / "003_iluminacao.dds", 26)
        assert np.max(np.abs(decoded - lighting) / np.maximum(lighting, 1e-3)) < 0.05
        manifest = json.loads((folder / "manifesto.json").read_text())
        for entry, (name, _, _, wanted, unwanted) in zip(manifest["capturas"], captures):
            pixels, fmt = read_dds(folder / entry["arquivo"], entry["formato_view"])
            tags = classify(pixels, fmt, entry)
            for tag in wanted:
                assert tag in tags, (name, tag, tags)
            for tag in unwanted:
                assert tag not in tags, (name, tag, tags)
        report_path = report(folder)
        assert "normal_3c" in report_path.read_text()
        assert (folder / "previews" / "000_normal_float.png").exists()
    print("gbuffer_report auto-teste ok")


def main(arguments):
    if arguments == ["--auto-teste"]:
        self_test()
        return 0
    if len(arguments) != 1:
        print("uso: gbuffer_report.py <pasta captura-quadro-...> | --auto-teste")
        return 2
    print(report(arguments[0]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
