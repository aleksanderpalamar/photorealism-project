#include <cassert>
#include <cmath>

double srgb_to_linear(double value) {
    if (value <= 0.04045) {
        return value / 12.92;
    }
    return std::pow((value + 0.055) / 1.055, 2.4);
}

double contribution(double brightness, double threshold_srgb,
                    double knee_srgb) {
    const double threshold = srgb_to_linear(threshold_srgb);
    const double raw = srgb_to_linear(threshold_srgb + knee_srgb) - threshold;
    const double knee = raw < 0.0001 ? 0.0001 : raw;

    double soft = brightness - threshold + knee;
    soft = soft < 0.0 ? 0.0 : (soft > 2.0 * knee ? 2.0 * knee : soft);
    soft = soft * soft / (4.0 * knee);

    const double hard = brightness - threshold;
    const double best = soft > hard ? soft : hard;
    const double result = best / (brightness > 0.0001 ? brightness : 0.0001);
    return result < 0.0 ? 0.0 : result;
}

const double kThreshold = 0.85;
const double kKnee = 0.06;

int main() {
    const double threshold_linear = srgb_to_linear(kThreshold);
    const double knee_linear =
        srgb_to_linear(kThreshold + kKnee) - threshold_linear;
    const double glow_starts = threshold_linear - knee_linear;

    assert(contribution(0.0, kThreshold, kKnee) == 0.0);
    assert(contribution(glow_starts * 0.5, kThreshold, kKnee) == 0.0);
    assert(contribution(glow_starts - 0.001, kThreshold, kKnee) == 0.0);

    for (int step = 0; step < 100; ++step) {
        const double brightness = glow_starts * (step / 100.0);
        assert(contribution(brightness, kThreshold, kKnee) == 0.0);
    }

    const double just_below = contribution(glow_starts + 1e-6, kThreshold, kKnee);
    assert(just_below >= 0.0 && just_below < 0.001);

    double previous = 0.0;
    for (int step = 0; step <= 200; ++step) {
        const double brightness = step / 100.0;
        const double current = contribution(brightness, kThreshold, kKnee);
        assert(current >= previous - 1e-9);
        previous = current;
    }

    const double high = 4.0;
    const double expected = (high - threshold_linear) / high;
    assert(std::fabs(contribution(high, kThreshold, kKnee) - expected) < 1e-9);

    assert(std::fabs(threshold_linear - 0.6921) < 0.001);
    assert(threshold_linear > kThreshold * 0.6);
    assert(threshold_linear < kThreshold);

    assert(contribution(0.1, kThreshold, 0.0) == 0.0);
    assert(contribution(2.0, kThreshold, 0.0) > 0.0);
    return 0;
}
