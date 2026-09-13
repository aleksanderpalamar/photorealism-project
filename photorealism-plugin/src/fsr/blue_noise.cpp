#include "blue_noise.hpp"

#include <cmath>
#include <cstdint>

namespace photorealism {
namespace fsr {
namespace {

constexpr double kSigma = 1.9;
constexpr unsigned kInitialDensityDivisor = 10;

class EnergyField {
  public:
    explicit EnergyField(unsigned size)
        : size_(size),
          kernel_(size * size),
          energy_(size * size, 0.0),
          pattern_(size * size, false) {
        for (unsigned dy = 0; dy < size; ++dy) {
            for (unsigned dx = 0; dx < size; ++dx) {
                const double x = wrapped(dx);
                const double y = wrapped(dy);
                kernel_[dy * size + dx] =
                    std::exp(-(x * x + y * y) / (2.0 * kSigma * kSigma));
            }
        }
    }

    bool at(unsigned index) const { return pattern_[index]; }

    void set(unsigned index, bool value) {
        if (pattern_[index] == value) {
            return;
        }
        pattern_[index] = value;
        const double sign = value ? 1.0 : -1.0;
        const unsigned px = index % size_;
        const unsigned py = index / size_;
        for (unsigned qy = 0; qy < size_; ++qy) {
            const unsigned dy = (qy + size_ - py) % size_;
            for (unsigned qx = 0; qx < size_; ++qx) {
                const unsigned dx = (qx + size_ - px) % size_;
                energy_[qy * size_ + qx] += sign * kernel_[dy * size_ + dx];
            }
        }
    }

    unsigned tightest_cluster() const { return extreme(true, true); }
    unsigned largest_void() const { return extreme(false, false); }

  private:
    double wrapped(unsigned delta) const {
        const unsigned folded = delta > size_ / 2 ? size_ - delta : delta;
        return static_cast<double>(folded);
    }

    unsigned extreme(bool wanted, bool highest) const {
        unsigned best = 0;
        bool found = false;
        for (unsigned index = 0; index < pattern_.size(); ++index) {
            if (pattern_[index] != wanted) {
                continue;
            }
            const bool better = !found ||
                (highest ? energy_[index] > energy_[best]
                         : energy_[index] < energy_[best]);
            best = better ? index : best;
            found = true;
        }
        return best;
    }

    unsigned size_;
    std::vector<double> kernel_;
    std::vector<double> energy_;
    std::vector<bool> pattern_;
};

EnergyField initial_pattern(unsigned size, unsigned* ones) {
    EnergyField field(size);
    const unsigned total = size * size;
    std::uint32_t state = 0x9E3779B9u;
    *ones = 0;
    while (*ones < total / kInitialDensityDivisor) {
        state = state * 1664525u + 1013904223u;
        const unsigned index = (state >> 8) % total;
        *ones += field.at(index) ? 0 : 1;
        field.set(index, true);
    }
    for (unsigned step = 0; step < total; ++step) {
        const unsigned cluster = field.tightest_cluster();
        field.set(cluster, false);
        const unsigned hole = field.largest_void();
        field.set(hole, true);
        if (hole == cluster) {
            break;
        }
    }
    return field;
}

}

std::vector<float> generate_blue_noise(unsigned size) {
    const unsigned total = size * size;
    std::vector<unsigned> rank(total, 0);
    unsigned ones = 0;
    const EnergyField seed = initial_pattern(size, &ones);

    EnergyField removal = seed;
    for (unsigned order = ones; order > 0; --order) {
        const unsigned cluster = removal.tightest_cluster();
        removal.set(cluster, false);
        rank[cluster] = order - 1;
    }

    EnergyField filling = seed;
    for (unsigned order = ones; order < total; ++order) {
        const unsigned hole = filling.largest_void();
        filling.set(hole, true);
        rank[hole] = order;
    }

    std::vector<float> values(total);
    for (unsigned index = 0; index < total; ++index) {
        values[index] = (static_cast<float>(rank[index]) + 0.5f) /
                        static_cast<float>(total);
    }
    return values;
}

}
}
