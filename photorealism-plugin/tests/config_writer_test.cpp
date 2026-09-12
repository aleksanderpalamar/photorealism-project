#include "../src/config/writer.cpp"

#include <cassert>
#include <cstdio>

using photorealism::config_writer::set_value;

namespace {

const char* kSample =
    "# cabecalho de calibracao\n"
    "[module.ssao.0.7.0]\n"
    "enabled=true\n"
    "# radius e medido, nao chutado\n"
    "radius=0.8\n"
    "intensity=0.28\n"
    "\n"
    "[module.ssao_interior.0.9.0]\n"
    "radius=0.45\n"
    "intensity=0.20\n";

void replaces_only_the_matching_section() {
    std::string text = kSample;
    assert(set_value(&text, "module.ssao.0.7.0", "radius", "1.25"));
    assert(text.find("radius=1.25") != std::string::npos);
    assert(text.find("radius=0.45") != std::string::npos);
    assert(text.find("radius=0.8") == std::string::npos);
}

void keeps_every_comment_and_blank_line() {
    std::string text = kSample;
    assert(set_value(&text, "module.ssao.0.7.0", "intensity", "0.5"));
    assert(text.find("# cabecalho de calibracao\n") == 0);
    assert(text.find("# radius e medido, nao chutado") != std::string::npos);
    assert(text.find("\n\n[module.ssao_interior") != std::string::npos);
}

void changes_nothing_but_the_value_span() {
    std::string before = kSample;
    std::string after = kSample;
    assert(set_value(&after, "module.ssao.0.7.0", "intensity", "0.28"));
    assert(before == after);
}

void inserts_a_missing_key_into_an_existing_section() {
    std::string text = kSample;
    assert(set_value(&text, "module.ssao.0.7.0", "bias", "0.04"));
    const std::size_t header = text.find("[module.ssao.0.7.0]\n");
    const std::size_t inserted = text.find("bias=0.04");
    const std::size_t next_section = text.find("[module.ssao_interior");
    assert(header != std::string::npos);
    assert(inserted > header && inserted < next_section);
}

void appends_a_missing_section() {
    std::string text = kSample;
    assert(set_value(&text, "module.user.0.20.0", "exposure_delta", "0.1"));
    const std::size_t section = text.find("[module.user.0.20.0]");
    assert(section != std::string::npos);
    assert(section > text.find("[module.ssao_interior.0.9.0]"));
    assert(text.find("exposure_delta=0.1") > section);
}

void survives_carriage_returns() {
    std::string text =
        "[module.bloom.0.17.0]\r\nenabled=true\r\nintensity=0.12\r\n";
    assert(set_value(&text, "module.bloom.0.17.0", "intensity", "0.30"));
    assert(text.find("intensity=0.30\r\n") != std::string::npos);
    assert(text.find("enabled=true\r\n") != std::string::npos);
}

void ignores_a_commented_key() {
    std::string text =
        "[module.bloom.0.17.0]\n#intensity=0.99\nintensity=0.12\n";
    assert(set_value(&text, "module.bloom.0.17.0", "intensity", "0.30"));
    assert(text.find("#intensity=0.99") != std::string::npos);
    assert(text.find("\nintensity=0.30") != std::string::npos);
}

void does_not_match_a_key_that_only_shares_a_prefix() {
    std::string text = "[native_aa.0.12.2]\nr_aa_quality=2\nr_aa=6\n";
    assert(set_value(&text, "native_aa.0.12.2", "r_aa", "0"));
    assert(text.find("r_aa_quality=2") != std::string::npos);
    assert(text.find("\nr_aa=0") != std::string::npos);
}

void writes_into_the_section_even_when_it_is_the_last_one() {
    std::string text = "[a]\nx=1\n[b]\ny=2\n";
    assert(set_value(&text, "b", "y", "9"));
    assert(text == "[a]\nx=1\n[b]\ny=9\n");
}

}

int main() {
    replaces_only_the_matching_section();
    keeps_every_comment_and_blank_line();
    changes_nothing_but_the_value_span();
    inserts_a_missing_key_into_an_existing_section();
    appends_a_missing_section();
    survives_carriage_returns();
    ignores_a_commented_key();
    does_not_match_a_key_that_only_shares_a_prefix();
    writes_into_the_section_even_when_it_is_the_last_one();
    std::printf("config_writer_test ok\n");
    return 0;
}
