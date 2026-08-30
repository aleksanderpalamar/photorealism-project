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
    // Leitor da politica de AA nativa no photorealism-plugin.cfg.
    using photorealism::aa_config::plugin_config_value;
    const std::string plugin_config =
        "# comentario\n"
        "[plugin]\n"
        "enabled=true\n"
        "\n"
        "[native_aa.0.12.2]\n"
        "manage=true\n"
        "r_aa = 6\n"
        "  r_taa_luma_sharpen=1.5\r\n"
        "# r_taa_tuning=99\n"
        "\n"
        "[native_aa.0.12.2]\n"
        "r_aa=999\n";
    assert(plugin_config_value(plugin_config, "native_aa.0.12.2", "manage") ==
           "true");
    assert(plugin_config_value(plugin_config, "native_aa.0.12.2", "r_aa") ==
           "6");
    assert(plugin_config_value(
               plugin_config, "native_aa.0.12.2", "r_taa_luma_sharpen") ==
           "1.5");
    // Chave comentada nao vale, e a secao seguinte nao contamina a anterior.
    assert(plugin_config_value(
               plugin_config, "native_aa.0.12.2", "r_taa_tuning") == "ausente");
    assert(plugin_config_value(plugin_config, "native_aa.0.12.2", "ausente") ==
           "ausente");
    assert(plugin_config_value(plugin_config, "secao.inexistente", "r_aa") ==
           "ausente");
    // Prefixo nao pode casar: r_aa_quality nao e r_aa.
    const std::string prefix_config =
        "[native_aa.0.12.2]\n"
        "r_aa_quality=7\n";
    assert(plugin_config_value(prefix_config, "native_aa.0.12.2", "r_aa") ==
           "ausente");
    assert(plugin_config_value("", "native_aa.0.12.2", "r_aa") == "ausente");

    return 0;
}
