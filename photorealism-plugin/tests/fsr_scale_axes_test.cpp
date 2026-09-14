#include "../src/fsr/scale_axes.hpp"

#include <cassert>
#include <cstdio>
#include <string>

using namespace photorealism;
using namespace photorealism::fsr;

namespace {

const char* kPresetSeventyFive =
    "uset r_mirror_scale_x \"1\"\n"
    "uset r_scale_y \"1\"\n"
    "uset r_scale_x \"0.75\"\n";

void an_asymmetric_preset_comes_back_intact() {
    std::string game_config = kPresetSeventyFive;
    const ScaleAxes before = read_game_scale(game_config);
    assert(before.x == "0.75" && before.y == "1");

    assert(write_game_scale(&game_config, ScaleAxes{"0.866000", "0.866000"}));
    assert(aa_config::config_value(game_config, "r_scale_x") == "0.866000");
    assert(aa_config::config_value(game_config, "r_scale_y") == "0.866000");

    const std::string saved = format_saved_scale(before);
    assert(write_game_scale(&game_config, parse_saved_scale(saved)));
    assert(game_config == kPresetSeventyFive);
}

void the_one_value_file_of_older_versions_still_restores_both_axes() {
    const ScaleAxes legacy = parse_saved_scale("1");
    assert(legacy.x == "1" && legacy.y == "1");
    const ScaleAxes with_newline = parse_saved_scale("0.666700\r\n");
    assert(with_newline.x == "0.666700" && with_newline.y == "0.666700");
    const ScaleAxes both = parse_saved_scale("0.75\r\n1\r\n");
    assert(both.x == "0.75" && both.y == "1");
}

void an_axis_the_game_never_wrote_is_never_invented() {
    std::string game_config = "uset r_scale_x \"1\"\n";
    const ScaleAxes before = read_game_scale(game_config);
    assert(before.y == kAbsentScale);
    assert(write_game_scale(&game_config, ScaleAxes{"0.866000", "0.866000"}));
    assert(game_config.find("r_scale_y") == std::string::npos);
    assert(write_game_scale(&game_config, before));
    assert(game_config == "uset r_scale_x \"1\"\n");
}

void a_config_already_at_the_wanted_value_is_not_rewritten() {
    std::string game_config = "uset r_scale_y \"0.866000\"\nuset r_scale_x \"0.866000\"\n";
    assert(!write_game_scale(&game_config, ScaleAxes{"0.866000", "0.866000"}));
    assert(!write_game_scale(&game_config, ScaleAxes{}));
}

void nothing_saved_means_nothing_borrowed() {
    assert(axes_empty(ScaleAxes{}));
    assert(!axes_empty(parse_saved_scale("1")));
}

}

int main() {
    an_asymmetric_preset_comes_back_intact();
    the_one_value_file_of_older_versions_still_restores_both_axes();
    an_axis_the_game_never_wrote_is_never_invented();
    a_config_already_at_the_wanted_value_is_not_rewritten();
    nothing_saved_means_nothing_borrowed();
    std::printf("fsr_scale_axes_test ok\n");
    return 0;
}
