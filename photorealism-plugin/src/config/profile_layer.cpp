#include "profile_layer.hpp"

namespace photorealism {

CalibrationLayer profile_base_layer(const PhotorealismProfile& profile) {
    const PhotorealismTonemap& tonemap = active_tonemap(profile);
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = tonemap.temperature;
    layer.exposure = tonemap.exposure;
    layer.contrast = tonemap.contrast;
    layer.saturation = tonemap.saturation;
    layer.vibrance = tonemap.vibrance;
    layer.shadows = tonemap.shadows;
    layer.highlights = tonemap.highlights;
    layer.blacks = tonemap.blacks;
    layer.whites = tonemap.whites;
    layer.sharpness = profile.sharpness / kProfileSliderScale;
    layer.local_contrast = profile.sharpen_edges / kProfileSliderScale;
    return layer;
}

}
