#include "photorealism_profile.hpp"

namespace photorealism {
namespace {

constexpr PhotorealismTonemap kReferenceSets[kProfileTonemapSets] = {
    {6500.0f, -1.00f, 0.42f, 1.00f, -0.09f, 0.82f, 0.92f, -0.15f, 0.00f, 0.00f, 0.00f, 0.03f, 2.00f},
    {6500.0f, 0.00f, 0.26f, 1.00f, 0.25f, 0.97f, 0.93f, 0.00f, -0.06f, 0.06f, -0.02f, -0.13f, 0.00f},
    {6500.0f, 0.10f, 0.00f, 1.00f, 0.00f, 1.01f, 1.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f},
    {6500.0f, 0.00f, 0.00f, 1.00f, -0.06f, 1.00f, 0.99f, 0.00f, -0.01f, -0.07f, 0.00f, -0.01f, 0.00f},
    {6500.0f, 0.00f, 0.00f, 1.00f, 0.00f, 1.00f, 1.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f},
};

}

PhotorealismProfile reference_profile() {
    PhotorealismProfile profile;
    for (unsigned index = 0; index < kProfileTonemapSets; ++index) {
        profile.sets[index] = kReferenceSets[index];
    }
    profile.sharpness = 6.0f;
    profile.sharpen_edges = 4.0f;
    profile.ssao_intensity = 1.5f;
    return profile;
}

}
