#include "../src/native_aa_config_text.hpp"

#include <cassert>
#include <string>

int main() {
    std::string config =
        "uset r_aa_quality \"do-not-touch\"\n"
        "uset r_aa \"6\"\n"
        " uset r_taa_tuning \"1\"\r\n"
        "uset r_taa_luma_sharpen \"1.5\"\n"
        "uset r_taa_modulated_drr_strength \"0.75\"\n"
        "uset unrelated \"preserve\"\n";
    assert(photorealism::aa_config::config_value(config, "r_aa") == "6");
    assert(photorealism::aa_config::set_config_value(
        &config, "r_aa", "0"));
    assert(photorealism::aa_config::set_config_value(
        &config, "r_taa_tuning", "0"));
    assert(photorealism::aa_config::set_config_value(
        &config, "r_taa_luma_sharpen", "0.0"));
    assert(photorealism::aa_config::set_config_value(
        &config, "r_taa_modulated_drr_strength", "0.0"));
    assert(photorealism::aa_config::config_value(config, "r_aa") == "0");
    assert(
        photorealism::aa_config::config_value(config, "r_aa_quality") ==
        "do-not-touch");
    assert(
        photorealism::aa_config::config_value(config, "r_taa_tuning") ==
        "0");
    assert(
        photorealism::aa_config::config_value(
            config, "r_taa_luma_sharpen") == "0.0");
    assert(
        photorealism::aa_config::config_value(
            config, "r_taa_modulated_drr_strength") == "0.0");
    assert(
        photorealism::aa_config::config_value(config, "unrelated") ==
        "preserve");
    assert(!photorealism::aa_config::set_config_value(
        &config, "unknown_cvar", "0"));
    return 0;
}
