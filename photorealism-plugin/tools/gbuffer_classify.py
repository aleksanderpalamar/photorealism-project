import numpy as np

from gbuffer_dds import DEPTH_FORMATS, FLOAT_FORMATS


def channel_stats(channel):
    values = channel[np.isfinite(channel)]
    if values.size == 0:
        return {"min": 0.0, "p5": 0.0, "p50": 0.0, "p95": 0.0, "max": 0.0, "media": 0.0, "desvio": 0.0, "distintos": 0}
    sample = values[:: max(1, values.size // 200000)]
    percentiles = np.percentile(sample, [5, 50, 95])
    return {
        "min": float(values.min()), "p5": float(percentiles[0]), "p50": float(percentiles[1]),
        "p95": float(percentiles[2]), "max": float(values.max()), "media": float(values.mean()),
        "desvio": float(values.std()), "distintos": int(np.unique(np.round(sample * 255.0)).size),
    }


def background_mask(rgb):
    quantized = np.round(rgb * 64.0).astype(np.int64)
    keys = quantized[..., 0] * 1000003 + quantized[..., 1] * 1009 + quantized[..., 2]
    values, counts = np.unique(keys, return_counts=True)
    return keys != values[np.argmax(counts)]


def unit_normal_fraction(rgb):
    unorm = rgb.min() >= -0.01 and rgb.max() <= 1.01
    vectors = rgb * 2.0 - 1.0 if unorm else rgb
    lengths = np.linalg.norm(vectors, axis=-1)
    mask = background_mask(rgb)
    if mask.sum() < 16:
        return 0.0
    return float(np.mean(np.abs(lengths[mask] - 1.0) < 0.08))


def encoded_normal_2c(pixels, stats):
    first, second = stats[0], stats[1]
    in_unit = min(first["min"], second["min"]) >= -0.01 and max(first["max"], second["max"]) <= 1.01
    centered = 0.3 <= first["media"] <= 0.7 and 0.3 <= second["media"] <= 0.7
    spread = first["desvio"] > 0.05 and second["desvio"] > 0.05
    if not (in_unit and centered and spread):
        return False
    xy = pixels[..., :2] * 2.0 - 1.0
    return float(np.mean(np.sum(xy * xy, axis=-1) <= 1.02)) > 0.98


def velocity_like(pixels, fmt, stats):
    if pixels.shape[-1] != 2 or min(stats[0]["desvio"], stats[1]["desvio"]) <= 1e-5:
        return False
    center = 0.5 if fmt in (35, 49) else 0.0
    offsets = np.abs(pixels - center)
    return float(np.mean(np.max(offsets, axis=-1) < 0.02)) > 0.6 and float(offsets.max()) < 0.5


def classify(pixels, fmt, target):
    if target.get("depth") or fmt in DEPTH_FORMATS:
        return ["profundidade"]
    channels = pixels.shape[-1]
    stats = [channel_stats(pixels[..., index]) for index in range(channels)]
    active = [index for index in range(channels) if stats[index]["desvio"] > 1e-4]
    tags = []
    if target["largura"] * target["altura"] <= 64:
        tags.append("valor_medio")
    constant = [str(index) for index in range(channels) if index not in active]
    if constant and active:
        tags.append("canais_constantes=" + ",".join(constant))
    hdr = fmt in FLOAT_FORMATS and any(stats[index]["p95"] > 1.05 or stats[index]["max"] > 1.5 for index in active)
    if hdr:
        tags.append("hdr")
    normal = channels >= 3 and len(active) >= 2 and unit_normal_fraction(pixels[..., :3]) > 0.7
    if normal:
        tags.append("normal_3c")
    if not normal and channels >= 2 and len(active) >= 2 and encoded_normal_2c(pixels, stats):
        tags.append("possivel_normal_2c")
    for index in active:
        if stats[index]["distintos"] <= 12 and not normal:
            tags.append("id_material_c%d" % index)
    if velocity_like(pixels, fmt, stats):
        tags.append("possivel_velocidade")
    ldr_color = channels >= 3 and not hdr and not normal and all(
        stats[index]["min"] >= -0.01 and stats[index]["max"] <= 1.01 for index in range(3)) and len(active) >= 2
    if ldr_color:
        tags.append("cor_ldr")
    return tags or ["sem_padrao"]
