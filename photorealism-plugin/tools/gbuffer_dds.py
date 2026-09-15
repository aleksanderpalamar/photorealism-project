import struct
from pathlib import Path

import numpy as np

FORMAT_NAMES = {
    2: "R32G32B32A32_FLOAT", 9: "R16G16B16A16_TYPELESS", 10: "R16G16B16A16_FLOAT",
    11: "R16G16B16A16_UNORM", 19: "R32G8X24_TYPELESS", 20: "D32_FLOAT_S8X24_UINT",
    23: "R10G10B10A2_TYPELESS", 24: "R10G10B10A2_UNORM", 26: "R11G11B10_FLOAT",
    27: "R8G8B8A8_TYPELESS", 28: "R8G8B8A8_UNORM", 29: "R8G8B8A8_UNORM_SRGB",
    33: "R16G16_TYPELESS", 34: "R16G16_FLOAT", 35: "R16G16_UNORM", 36: "R16G16_UINT",
    37: "R16G16_SNORM", 39: "R32_TYPELESS", 40: "D32_FLOAT", 41: "R32_FLOAT",
    44: "R24G8_TYPELESS", 45: "D24_UNORM_S8_UINT", 48: "R8G8_TYPELESS", 49: "R8G8_UNORM",
    53: "R16_TYPELESS", 54: "R16_FLOAT", 56: "R16_UNORM", 57: "R16_UINT", 60: "R8_TYPELESS",
    61: "R8_UNORM", 62: "R8_UINT", 87: "B8G8R8A8_UNORM", 88: "B8G8R8X8_UNORM",
    90: "B8G8R8A8_TYPELESS", 91: "B8G8R8A8_UNORM_SRGB",
}

FLOAT_FORMATS = {2, 10, 26, 34, 41, 54, 19, 20, 40}
DEPTH_FORMATS = {19, 20, 39, 40, 44, 45}


def typed_format(texture_format, view_format):
    typeless = {9: 10, 19: 20, 23: 24, 27: 28, 33: 34, 39: 41, 44: 45, 48: 49, 53: 54, 60: 61, 90: 87}
    if texture_format in typeless:
        known = view_format if view_format not in (0, texture_format) else typeless[texture_format]
        return known
    return texture_format


def decode_r11g11b10(packed):
    def component(bits, mantissa_bits):
        mantissa = bits & ((1 << mantissa_bits) - 1)
        exponent = bits >> mantissa_bits
        fraction = mantissa / float(1 << mantissa_bits)
        small = fraction * 2.0 ** -14
        normal = (1.0 + fraction) * np.power(2.0, exponent.astype(np.float64) - 15.0)
        return np.where(exponent == 0, small, normal).astype(np.float32)
    red = component(packed & 0x7FF, 6)
    green = component((packed >> 11) & 0x7FF, 6)
    blue = component((packed >> 22) & 0x3FF, 5)
    return np.stack([red, green, blue], axis=-1)


def decode_pixels(raw, width, height, fmt):
    count = width * height
    if fmt == 2:
        return np.frombuffer(raw, np.float32, count * 4).reshape(height, width, 4)
    if fmt == 10:
        return np.frombuffer(raw, np.float16, count * 4).reshape(height, width, 4).astype(np.float32)
    if fmt == 11:
        return np.frombuffer(raw, np.uint16, count * 4).reshape(height, width, 4) / 65535.0
    if fmt == 24:
        packed = np.frombuffer(raw, np.uint32, count).reshape(height, width)
        channels = [(packed >> shift) & 0x3FF for shift in (0, 10, 20)] + [packed >> 30]
        return np.stack([channels[0] / 1023.0, channels[1] / 1023.0, channels[2] / 1023.0, channels[3] / 3.0], axis=-1)
    if fmt == 26:
        return decode_r11g11b10(np.frombuffer(raw, np.uint32, count).reshape(height, width))
    if fmt in (28, 29):
        return np.frombuffer(raw, np.uint8, count * 4).reshape(height, width, 4) / 255.0
    if fmt in (87, 88, 91):
        bgra = np.frombuffer(raw, np.uint8, count * 4).reshape(height, width, 4) / 255.0
        return bgra[..., [2, 1, 0, 3]]
    if fmt == 34:
        return np.frombuffer(raw, np.float16, count * 2).reshape(height, width, 2).astype(np.float32)
    if fmt == 35:
        return np.frombuffer(raw, np.uint16, count * 2).reshape(height, width, 2) / 65535.0
    if fmt == 36:
        return np.frombuffer(raw, np.uint16, count * 2).reshape(height, width, 2).astype(np.float32)
    if fmt == 37:
        return np.maximum(np.frombuffer(raw, np.int16, count * 2).reshape(height, width, 2) / 32767.0, -1.0)
    if fmt in (20,):
        pixels = np.frombuffer(raw, np.uint8, count * 8).reshape(height, width, 8)
        depth = pixels[..., :4].copy().view(np.float32)[..., 0]
        return np.stack([depth, pixels[..., 4] / 255.0], axis=-1)
    if fmt in (40, 41):
        return np.frombuffer(raw, np.float32, count).reshape(height, width, 1)
    if fmt == 45:
        packed = np.frombuffer(raw, np.uint32, count).reshape(height, width)
        return np.stack([(packed & 0xFFFFFF) / 16777215.0, (packed >> 24) / 255.0], axis=-1)
    if fmt == 49:
        return np.frombuffer(raw, np.uint8, count * 2).reshape(height, width, 2) / 255.0
    if fmt == 54:
        return np.frombuffer(raw, np.float16, count).reshape(height, width, 1).astype(np.float32)
    if fmt == 56:
        return np.frombuffer(raw, np.uint16, count).reshape(height, width, 1) / 65535.0
    if fmt == 57:
        return np.frombuffer(raw, np.uint16, count).reshape(height, width, 1).astype(np.float32)
    if fmt == 61:
        return np.frombuffer(raw, np.uint8, count).reshape(height, width, 1) / 255.0
    if fmt == 62:
        return np.frombuffer(raw, np.uint8, count).reshape(height, width, 1).astype(np.float32)
    return None


def read_dds(path, view_format):
    data = Path(path).read_bytes()
    height, width = struct.unpack_from("<II", data, 12)
    texture_format = struct.unpack_from("<I", data, 128)[0]
    fmt = typed_format(texture_format, view_format)
    return decode_pixels(data[148:], width, height, fmt), fmt


def write_dds(path, pixels_bytes, width, height, fmt):
    header = bytearray(148)
    struct.pack_into("<4sIIII", header, 0, b"DDS ", 124, 0x1007, height, width)
    struct.pack_into("<I", header, 28, 1)
    struct.pack_into("<II4s", header, 76, 32, 4, b"DX10")
    struct.pack_into("<I", header, 108, 0x1000)
    struct.pack_into("<IIIII", header, 128, fmt, 3, 0, 1, 0)
    Path(path).write_bytes(bytes(header) + pixels_bytes)


def encode_r11g11b10(rgb):
    def component(values, mantissa_bits):
        values = np.maximum(values, 1e-6)
        exponent = np.clip(np.floor(np.log2(values)) + 15, 1, 30)
        mantissa = np.clip(np.round((values / np.power(2.0, exponent - 15) - 1.0) * (1 << mantissa_bits)), 0, (1 << mantissa_bits) - 1)
        return (exponent.astype(np.uint32) << mantissa_bits) | mantissa.astype(np.uint32)
    return (component(rgb[..., 0], 6) | (component(rgb[..., 1], 6) << 11) | (component(rgb[..., 2], 5) << 22)).astype(np.uint32)
