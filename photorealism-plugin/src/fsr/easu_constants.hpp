#pragma once

namespace photorealism {
namespace fsr {

struct EasuConstants {
    float con0[4];
    float input_size[2];
    float output_size[2];
};

inline EasuConstants populate_easu_constants(
    float input_viewport_x,
    float input_viewport_y,
    float output_size_x,
    float output_size_y) {
    const float reciprocal_output_x = 1.0f / output_size_x;
    const float reciprocal_output_y = 1.0f / output_size_y;
    const float half_step_x = 0.5f * input_viewport_x * reciprocal_output_x;
    const float half_step_y = 0.5f * input_viewport_y * reciprocal_output_y;
    EasuConstants constants = {};
    constants.con0[0] = input_viewport_x * reciprocal_output_x;
    constants.con0[1] = input_viewport_y * reciprocal_output_y;
    constants.con0[2] = half_step_x - 0.5f;
    constants.con0[3] = half_step_y - 0.5f;
    constants.input_size[0] = input_viewport_x;
    constants.input_size[1] = input_viewport_y;
    constants.output_size[0] = output_size_x;
    constants.output_size[1] = output_size_y;
    return constants;
}

}
}
