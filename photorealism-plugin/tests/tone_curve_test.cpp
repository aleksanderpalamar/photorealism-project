#include <cassert>
#include <cmath>

double linear_to_srgb(double value) {
    if (value <= 0.0031308) {
        return value * 12.92;
    }
    return 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
}

double apply_black_lift(double color, double lift) {
    const double floor_value = lift < 0.0 ? 0.0 : (lift > 0.02 ? 0.02 : lift);
    return floor_value + (1.0 - floor_value) * color;
}

double apply_contrast(double color, double contrast) {
    const double pivot = 0.18;
    const double floored = color < 1e-6 ? 1e-6 : color;
    return pivot * std::pow(floored / pivot, contrast);
}

double apply_contrast_affine(double color, double contrast) {
    const double pivot = 0.18;
    const double value = (color - pivot) * contrast + pivot;
    return value < 0.0 ? 0.0 : value;
}

double apply_highlight_rolloff(double color, double strength) {
    const double amount =
        strength < 0.0 ? 0.0 : (strength > 1.0 ? 1.0 : strength);
    if (amount <= 0.0) {
        return color;
    }
    const double knee = 1.0 + (0.5 - 1.0) * amount;
    const double headroom = (1.0 - knee) < 0.0001 ? 0.0001 : (1.0 - knee);
    const double excess = color - knee > 0.0 ? color - knee : 0.0;
    const double compressed =
        knee + headroom * (1.0 - std::exp(-excess / headroom));
    return color < compressed ? color : compressed;
}

int output_code(double linear) {
    const double clamped = linear < 0.0 ? 0.0 : (linear > 1.0 ? 1.0 : linear);
    return static_cast<int>(linear_to_srgb(clamped) * 255.0 + 0.5);
}

const double kApprovedBlackLiftR = 0.001017;
const double kApprovedBlackLiftG = 0.001982;
const double kApprovedBlackLiftB = 0.001888;
const double kEffectiveBlackLiftR = 0.001398;
const double kEffectiveBlackLiftG = 0.002480;
const double kEffectiveBlackLiftB = 0.002268;
const double kApprovedContrast = 1.07;
const double kApprovedHighlightRolloff = 0.35;

int main() {
    const int floor_code = output_code(apply_black_lift(0.0, 0.0027));
    assert(floor_code >= 6 && floor_code <= 12);
    assert(floor_code == 9);

    assert(apply_black_lift(0.0, 0.0027) < apply_black_lift(0.1, 0.0027));

    assert(output_code(apply_black_lift(1.0, 0.0027)) == 255);

    assert(output_code(apply_black_lift(0.0, kApprovedBlackLiftR)) == 3);
    assert(output_code(apply_black_lift(0.0, kApprovedBlackLiftG)) == 7);
    assert(output_code(apply_black_lift(0.0, kApprovedBlackLiftB)) == 6);

    assert(output_code(apply_black_lift(0.0, kEffectiveBlackLiftR)) == 5);
    assert(output_code(apply_black_lift(0.0, kEffectiveBlackLiftG)) == 8);
    assert(output_code(apply_black_lift(0.0, kEffectiveBlackLiftB)) == 7);

    assert(kEffectiveBlackLiftR / kEffectiveBlackLiftG > 0.44);
    assert(kEffectiveBlackLiftR / kEffectiveBlackLiftG < 0.80);
    assert(kEffectiveBlackLiftB / kEffectiveBlackLiftG > 0.87);
    assert(kEffectiveBlackLiftB / kEffectiveBlackLiftG < 1.16);

    assert(kApprovedBlackLiftR < kApprovedBlackLiftG);
    assert(kEffectiveBlackLiftR < kEffectiveBlackLiftG);

    const double affine_crush_limit =
        0.18 * (kApprovedContrast - 1.0) / kApprovedContrast;
    assert(affine_crush_limit > 0.0117 && affine_crush_limit < 0.0118);
    assert(apply_contrast_affine(0.002, kApprovedContrast) == 0.0);
    assert(apply_contrast_affine(0.010, kApprovedContrast) == 0.0);
    assert(apply_contrast_affine(0.002, kApprovedContrast) ==
           apply_contrast_affine(0.010, kApprovedContrast));

    assert(apply_contrast_affine(0.012, kApprovedContrast) > 0.0);

    assert(apply_contrast(0.002, kApprovedContrast) > 0.0);
    assert(apply_contrast(0.002, kApprovedContrast) <
           apply_contrast(0.010, kApprovedContrast));

    assert(output_code(apply_black_lift(
               apply_contrast(0.002, kApprovedContrast),
               kApprovedBlackLiftG)) <
           output_code(apply_black_lift(
               apply_contrast(0.010, kApprovedContrast),
               kApprovedBlackLiftG)));

    assert(std::fabs(apply_contrast(0.18, kApprovedContrast) - 0.18) < 1e-9);
    assert(apply_contrast(0.0, kApprovedContrast) < 1e-6);

    for (int i = 1; i < 512; ++i) {
        const double lo = static_cast<double>(i - 1) / 512.0;
        const double hi = static_cast<double>(i) / 512.0;
        assert(apply_contrast(lo, kApprovedContrast) <=
               apply_contrast(hi, kApprovedContrast));
    }

    int distinct = 0;
    int previous = -1;
    for (int code = 0; code <= 40; ++code) {
        const double srgb = static_cast<double>(code) / 255.0;
        const double linear = srgb <= 0.04045
                                  ? srgb / 12.92
                                  : std::pow((srgb + 0.055) / 1.055, 2.4);
        const int out = output_code(apply_black_lift(
            apply_contrast(linear, kApprovedContrast), kApprovedBlackLiftG));
        if (out != previous) {
            ++distinct;
            previous = out;
        }
    }
    assert(distinct >= 24);

    assert(output_code(apply_black_lift(0.0, 5.0)) < 64);
    assert(output_code(apply_black_lift(0.0, -1.0)) == 0);

    assert(apply_highlight_rolloff(2.0, 0.0) == 2.0);

    assert(apply_highlight_rolloff(50.0, 0.35) <= 1.0);
    assert(apply_highlight_rolloff(1000.0, 0.35) <= 1.0);

    assert(apply_highlight_rolloff(1.06, 0.35) < 1.0);
    assert(output_code(apply_highlight_rolloff(1.06, 0.35)) < 255);
    assert(output_code(apply_highlight_rolloff(1.0, 0.35)) < 255);

    assert(apply_highlight_rolloff(0.2, 0.35) == 0.2);

    assert(apply_highlight_rolloff(0.9, 0.35) <=
           apply_highlight_rolloff(1.5, 0.35));

    assert(output_code(
               apply_highlight_rolloff(1.06, kApprovedHighlightRolloff)) < 255);
    return 0;
}
