#!/usr/bin/env python3
"""Gera o primeiro passe original de gotas e spray do Photorealism."""

from __future__ import annotations

from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
GLASS_OUTPUT = PROJECT_DIR / "mod/def/vehicle/interior_glass_config_rain.sii"
PARTICLE_DIR = PROJECT_DIR / "mod/unit/hookup/vehicle/particle"


GLASS_CONFIG = """SiiNunit
{
core_interior_glass_config : .glass.config.rain {
    drop_material: "/material/environment/blobs_rain.mat"

    min_drop_size: 0.007
    max_drop_size: 0.021
    drop_count_per_meter: 9000

    min_initial_splash_scale: 2.8
    max_initial_splash_scale: 3.2
    min_splash_duration: 0.14
    max_splash_duration: 0.18

    minimal_lifetime: 0.8
    maximal_lifetime: 6.5
    minimal_fadeout: 0.25
    maximal_fadeout: 1.8

    min_distortion_strength: 0.9
    max_distortion_strength: 1.0
    blur_strength: 0.9

    deformation_duration: 0.35
    maximal_deformation: 1.9

    moving_size_threshold: 0.38
    vertical_acceleration_factor: -0.32
    movement_acceleration_factor: 0.040
    min_adhesion_factor: 0.45
    max_adhesion_factor: 0.92

    min_speed_effect_mps: 4.2
    max_speed_effect_mps: 25.0

    min_angle_cosine: 0.05
    max_angle_cosine: 0.35
    min_angle_factor: 0.15
    max_angle_factor: 0.60

    drops_wiping_factor: 0.07
    drops_moving_offset: 0.012
}
}
"""


SPRAY_LEVELS = (
    ("wheel_particle_rain.sii", "wheel.particle.rain", "0001", 0.18, 0.28, (0.12, 0.28), 1.2, 55, None),
    ("wheel_particle_rain1.sii", "wheel.particle.rain1", "0002", 0.28, 0.30, (0.14, 0.31), 1.5, 80, None),
    ("wheel_particle_rain2.sii", "wheel.particle.rain2", "0003", 0.38, 0.34, (0.17, 0.36), 1.8, 110, None),
    ("wheel_particle_rain3.sii", "wheel.particle.rain3", "0004", 0.48, 0.38, (0.20, 0.42), 2.1, 140, None),
    ("wheel_particle_asp_l_rain.sii", "wheel.particle.asp_l_rain", "0005", 0.28, 0.30, (0.14, 0.31), 1.5, 80, 35),
    ("wheel_particle_asp_m_rain.sii", "wheel.particle.asp_m_rain", "0006", 0.38, 0.34, (0.17, 0.36), 1.8, 110, 45),
    ("wheel_particle_asp_h_rain.sii", "wheel.particle.asp_h_rain", "0007", 0.48, 0.38, (0.20, 0.42), 2.1, 140, 55),
)


def particle_config(
    unit_name: str,
    nameless_id: str,
    alpha: float,
    scale: float,
    size: tuple[float, float],
    lifetime: float,
    maximum: int,
    secondary_maximum: int | None,
) -> str:
    """Monta um gerador leve que usa o modelo de spray do jogo-base."""

    generator_count = 2 if secondary_maximum is not None else 1
    secondary_array = ""
    secondary_generator = ""
    if secondary_maximum is not None:
        secondary_id = f"1{nameless_id}"
        secondary_size = {"0005": 0.025, "0006": 0.035, "0007": 0.045}[
            nameless_id
        ]
        secondary_alpha = {"0005": 0.24, "0006": 0.30, "0007": 0.36}[
            nameless_id
        ]
        secondary_array = f"\n    gen_array[1]: _nameless.prain.{secondary_id}"
        secondary_generator = f"""

particle_gen : _nameless.prain.{secondary_id} {{
    list_order_up: true
    blend_alpha: true
    gen_pos: (-0.15, -0.2, -1.6)
    scale_acc: 0.32
    rotation_speed: 54
    alpha_blend_coef: 0.30
    base_alpha: {secondary_alpha:.2f}
    alpha_attack_time: 0
    flow_speed: (0, 0.4, -0.4)
    flow_speed_acc: (0, -0.7, 0)
    particle_size: ({secondary_size:.3f}, {secondary_size:.3f})
    particle_size_var: (0.30, 0.30)
    particle_displacement: (0.35, 0.30, 0.35)
    color_multiplier: nil
    key_pos: 5
    key_pos[0]: 0
    key_pos[1]: 64
    key_pos[2]: 128
    key_pos[3]: 192
    key_pos[4]: 255
    key_color: 5
    key_color[0]: nil
    key_color[1]: nil
    key_color[2]: nil
    key_color[3]: nil
    key_color[4]: nil
    time_to_live: {lifetime:.1f}
    time_to_live_rand: 0.30
    particles_max_count: {secondary_maximum}
    model_path: "/asset/particles/wheels/w_smoke.pmd"
    model_look: rain
    relative_space: false
    use_sprites: true
}}"""

    return f"""SiiNunit
{{
vehicle_particle : {unit_name} {{
    gen_array: {generator_count}
    gen_array[0]: _nameless.prain.{nameless_id}{secondary_array}
    auto_disable_delay: 0
    editor: false
}}

particle_gen : _nameless.prain.{nameless_id} {{
    list_order_up: false
    blend_alpha: true
    gen_pos: (0.08, 0, 0.08)
    scale_acc: {scale:.2f}
    rotation_speed: 72
    alpha_blend_coef: 0.12
    base_alpha: {alpha:.2f}
    alpha_attack_time: 0.12
    flow_speed: (-0.35, 1.4, 0.02)
    flow_speed_acc: (0, -2.4, -1.6)
    particle_size: ({size[0]:.2f}, {size[1]:.2f})
    particle_size_var: (0.45, 0.30)
    particle_displacement: (-0.18, 0.09, -0.08)
    color_multiplier: nil
    key_pos: 5
    key_pos[0]: 0
    key_pos[1]: 96
    key_pos[2]: 176
    key_pos[3]: 224
    key_pos[4]: 255
    key_color: 5
    key_color[0]: nil
    key_color[1]: nil
    key_color[2]: nil
    key_color[3]: nil
    key_color[4]: nil
    time_to_live: {lifetime:.1f}
    time_to_live_rand: 0.35
    particles_max_count: {maximum}
    model_path: "/model/particle/smoke.pmd"
    model_look: rain
    relative_space: false
    use_sprites: true
}}{secondary_generator}
}}
"""


def main() -> None:
    GLASS_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    PARTICLE_DIR.mkdir(parents=True, exist_ok=True)
    GLASS_OUTPUT.write_text(GLASS_CONFIG, encoding="utf-8")
    print(f"Vidro atualizado: {GLASS_OUTPUT}")

    for (
        filename,
        unit,
        nameless,
        alpha,
        scale,
        size,
        lifetime,
        maximum,
        secondary_maximum,
    ) in SPRAY_LEVELS:
        output = PARTICLE_DIR / filename
        output.write_text(
            particle_config(
                unit,
                nameless,
                alpha,
                scale,
                size,
                lifetime,
                maximum,
                secondary_maximum,
            ),
            encoding="utf-8",
        )
        secondary = (
            f", gotas secundárias={secondary_maximum}"
            if secondary_maximum is not None
            else ""
        )
        print(
            f"Spray atualizado: {unit} "
            f"(alpha={alpha:.2f}, vida={lifetime:.1f}s, máximo={maximum}{secondary})"
        )


if __name__ == "__main__":
    main()
