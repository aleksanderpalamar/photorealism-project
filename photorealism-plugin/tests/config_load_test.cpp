#include <windows.h>

#include "../src/config.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

// Teste de carga do cfg -- 0.19.1.
//
// Ate aqui `config.cpp` nao tinha teste nenhum: `validate.sh` so fazia grep de
// valores literais dentro do arquivo. Grep confirma que um numero esta escrito;
// nao confirma que ele chega a Settings, nem que o parse le a chave, nem que o
// clamp faz o que diz.
//
// Foi essa lacuna que deixou passar o defeito que este teste agora impede: a
// 0.18.2 mudou `exposure` no cfg de -0,09 para -0,0488697 e esqueceu o default
// interno, entao o fallback passou a renderizar 0,041 EV mais escuro que o
// arquivo -- e nada acusou, porque o grep de cada lado passava.
//
// O teste inclui `config.cpp` direto, com um substituto de <windows.h>. E o
// unico jeito de exercitar o binario de verdade sem cortar o arquivo em dois so
// para agradar ao teste.

namespace photorealism {
namespace {
wchar_t g_config_path[4096] = {};
}  // namespace

const wchar_t* config_path() { return g_config_path; }
void log_message(const char*, ...) {}
void set_module(HMODULE) {}
const wchar_t* module_directory() { return L"."; }
const wchar_t* plugin_root() { return L"."; }
const wchar_t* shader_path() { return L"."; }
const wchar_t* depth_preview_shader_path() { return L"."; }
const wchar_t* ssao_shader_path() { return L"."; }
const wchar_t* temporal_shader_path() { return L"."; }
const wchar_t* bloom_shader_path() { return L"."; }
}  // namespace photorealism

#include "../src/config.cpp"

using namespace photorealism;

namespace {

const char* kTempPath = "/tmp/photorealism-config-load-test.cfg";

Settings load_from_text(const char* text) {
    FILE* file = std::fopen(kTempPath, "wb");
    assert(file != nullptr);
    std::fwrite(text, 1, std::strlen(text), file);
    std::fclose(file);
    ::mbstowcs(photorealism::g_config_path, kTempPath, 4095);
    Settings settings = {};
    const bool found = load_settings(&settings);
    assert(found);
    std::remove(kTempPath);
    return settings;
}

Settings load_missing_file() {
    ::mbstowcs(photorealism::g_config_path, "/tmp/nao-existe-photorealism.cfg", 4095);
    Settings settings = {};
    const bool found = load_settings(&settings);
    assert(!found);
    return settings;
}

bool near(float value, float target, float tolerance) {
    return std::fabs(value - target) <= tolerance;
}

// Le o cfg que de fato acompanha o pacote.
std::string shipped_config_path() {
    const char* root = std::getenv("PHOTOREALISM_PROJECT_DIR");
    const std::string base = root != nullptr ? root : ".";
    return base + "/config/photorealism-plugin.cfg";
}

}  // namespace

int main() {
    // --- 1. O defeito que este teste existe para impedir.
    //
    // Os defaults internos e o cfg entregue tem que produzir o MESMO perfil de
    // cor. Se divergirem, um jogador que perca o cfg roda com uma imagem
    // diferente da que foi calibrada -- e foi o que aconteceu entre a 0.18.2 e
    // a 0.19.0, com 0,041 EV de diferenca que ninguem viu.
    {
        const Settings internal = load_missing_file();
        const std::string path = shipped_config_path();
        ::mbstowcs(photorealism::g_config_path, path.c_str(), 4095);
        Settings shipped = {};
        if (load_settings(&shipped)) {
            assert(near(internal.temperature, shipped.temperature, 0.01f));
            assert(near(internal.exposure, shipped.exposure, 1e-5f));
            assert(near(internal.contrast, shipped.contrast, 1e-5f));
            assert(near(internal.saturation, shipped.saturation, 1e-5f));
            assert(near(internal.vibrance, shipped.vibrance, 1e-5f));
            assert(near(internal.shadows, shipped.shadows, 1e-5f));
            assert(near(internal.highlights, shipped.highlights, 1e-5f));
            assert(near(internal.blacks, shipped.blacks, 1e-5f));
            assert(near(internal.whites, shipped.whites, 1e-5f));
            assert(near(internal.local_contrast, shipped.local_contrast, 1e-5f));
            assert(near(internal.sharpness, shipped.sharpness, 1e-5f));
            assert(near(internal.vignette, shipped.vignette, 1e-5f));
            assert(near(internal.tint, shipped.tint, 1e-5f));
            assert(near(internal.highlight_rolloff, shipped.highlight_rolloff, 1e-5f));
            assert(near(internal.black_lift_r, shipped.black_lift_r, 1e-7f));
            assert(near(internal.black_lift_g, shipped.black_lift_g, 1e-7f));
            assert(near(internal.black_lift_b, shipped.black_lift_b, 1e-7f));
            // 0.19.0: as ancoras tambem.
            assert(near(internal.condition_sun_temperature,
                        shipped.condition_sun_temperature, 0.01f));
            assert(near(internal.condition_rain_temperature,
                        shipped.condition_rain_temperature, 0.01f));
            assert(near(internal.condition_night_temperature,
                        shipped.condition_night_temperature, 0.01f));
            assert(near(internal.condition_sun_tint, shipped.condition_sun_tint, 1e-5f));
            assert(near(internal.condition_rain_tint, shipped.condition_rain_tint, 1e-5f));
            assert(near(internal.condition_night_tint, shipped.condition_night_tint, 1e-5f));
        }
    }

    // --- 2. As camadas se somam, e o sufixo _delta e reconhecido.
    {
        const Settings s = load_from_text(
            "[base.0.1.2]\n"
            "enabled=true\n"
            "temperature=6000\n"
            "tint=0.20\n"
            "[module.visual.0.2.0]\n"
            "enabled=true\n"
            "temperature_delta=200\n"
            "tint_delta=0.10\n"
            "[module.rain_overcast.0.3.0]\n"
            "enabled=true\n"
            "temperature_delta=100\n"
            "tint_delta=0.05\n");
        assert(near(s.temperature, 6300.0f, 0.01f));
        assert(near(s.tint, 0.35f, 1e-5f));
    }

    // --- 3. Camada desligada nao entra na soma.
    {
        const Settings s = load_from_text(
            "[base.0.1.2]\n"
            "enabled=true\n"
            "temperature=6000\n"
            "[module.visual.0.2.0]\n"
            "enabled=false\n"
            "temperature_delta=500\n"
            "[module.rain_overcast.0.3.0]\n"
            "enabled=false\n"
            "temperature_delta=500\n");
        assert(near(s.temperature, 6000.0f, 0.01f));
    }

    // --- 4. A forma escalar `black_lift` da 0.14.0 continua carregando, e
    // escreve os tres canais. Um cfg antigo tem que rodar.
    {
        const Settings s = load_from_text(
            "[base.0.1.2]\n"
            "enabled=true\n"
            "black_lift=0.0027\n"
            "[module.visual.0.2.0]\n"
            "enabled=false\n"
            "[module.rain_overcast.0.3.0]\n"
            "enabled=false\n");
        assert(near(s.black_lift_r, 0.0027f, 1e-7f));
        assert(near(s.black_lift_g, 0.0027f, 1e-7f));
        assert(near(s.black_lift_b, 0.0027f, 1e-7f));
    }

    // --- 5. Secao desconhecida e ignorada sem contaminar a seguinte.
    //
    // E assim que um cfg de versao futura carrega numa versao antiga: as chaves
    // que ela nao conhece somem, e as que ela conhece continuam valendo.
    {
        const Settings s = load_from_text(
            "[module.inventado.9.9.9]\n"
            "sun_temperature=3000\n"
            "radius=99\n"
            "[module.ssao.0.7.0]\n"
            "radius=1.25\n");
        assert(near(s.ssao_radius, 1.25f, 1e-5f));
        assert(near(s.condition_sun_temperature, 5900.0f, 0.01f));
    }

    // --- 6. Chave desconhecida dentro de secao conhecida tambem e ignorada.
    {
        const Settings s = load_from_text(
            "[module.bloom.0.17.0]\n"
            "chave_que_nao_existe=123\n"
            "threshold=0.9\n");
        assert(near(s.bloom_threshold, 0.9f, 1e-5f));
    }

    // --- 7. Os limites seguram valores absurdos.
    {
        const Settings s = load_from_text(
            "[module.bloom.0.17.0]\n"
            "threshold=5.0\n"
            "[module.condition_adaptation.0.19.0]\n"
            "rain_temperature=99999\n"
            "sun_tint=-40\n");
        // O teto do limiar e 0.98: em 1.0 nada passa e o modulo ficaria ligado
        // sem produzir nada, o que e pior que desligado porque o log diz ativo.
        assert(near(s.bloom_threshold, 0.98f, 1e-5f));
        assert(near(s.condition_rain_temperature, 9000.0f, 0.01f));
        assert(near(s.condition_sun_tint, -1.0f, 1e-5f));
    }

    // --- 8. Banda invertida e corrigida.
    //
    // Com low >= high o smoothstep de quem consome degenera em degrau, e a
    // adaptacao por condicao volta a ter classe dura -- que e exatamente o que
    // a 0.19.0 existe para nao ter.
    {
        const Settings s = load_from_text(
            "[module.condition_adaptation.0.19.0]\n"
            "daylight_median_low=50\n"
            "daylight_median_high=10\n"
            "overcast_saturation_low=0.30\n"
            "overcast_saturation_high=0.05\n");
        assert(s.condition_daylight_median_high > s.condition_daylight_median_low);
        assert(s.condition_overcast_saturation_high >
               s.condition_overcast_saturation_low);
    }

    // --- 9. Comentarios, espacos e linhas sem `=` nao atrapalham.
    {
        const Settings s = load_from_text(
            "# comentario\n"
            "; outro comentario\n"
            "\n"
            "   [ module.ssao.0.7.0 ]   \n"
            "   radius   =   0.55   \n"
            "linha sem igual\n"
            "intensity=0.33\n");
        assert(near(s.ssao_radius, 0.55f, 1e-5f));
        assert(near(s.ssao_intensity, 0.33f, 1e-5f));
    }

    // --- 10. `enabled` de modulo nao vaza para o plugin inteiro, e vice-versa.
    {
        const Settings s = load_from_text(
            "[plugin]\n"
            "enabled=true\n"
            "[module.ssao.0.7.0]\n"
            "enabled=false\n"
            "[module.bloom.0.17.0]\n"
            "enabled=false\n");
        assert(s.enabled);
        assert(!s.ssao_enabled);
        assert(!s.bloom_enabled);
        assert(s.temporal_enabled);
        assert(s.condition_adaptation_enabled);
    }
    {
        const Settings s = load_from_text(
            "[plugin]\n"
            "enabled=false\n"
            "[module.ssao.0.7.0]\n"
            "enabled=true\n");
        assert(!s.enabled);
        assert(s.ssao_enabled);
    }

    // --- 11. Booleano aceita as tres formas historicas.
    {
        assert(load_from_text("[module.bloom.0.17.0]\nenabled=yes\n").bloom_enabled);
        assert(load_from_text("[module.bloom.0.17.0]\nenabled=TRUE\n").bloom_enabled);
        assert(load_from_text("[module.bloom.0.17.0]\nenabled=1\n").bloom_enabled);
        assert(!load_from_text("[module.bloom.0.17.0]\nenabled=0\n").bloom_enabled);
        assert(!load_from_text("[module.bloom.0.17.0]\nenabled=nao\n").bloom_enabled);
    }

    // --- 12. Cfg vazio devolve exatamente os defaults internos.
    {
        const Settings empty = load_from_text("");
        const Settings internal = load_missing_file();
        assert(near(empty.temperature, internal.temperature, 0.01f));
        assert(near(empty.exposure, internal.exposure, 1e-6f));
        assert(near(empty.tint, internal.tint, 1e-6f));
        assert(near(empty.condition_rain_temperature,
                    internal.condition_rain_temperature, 0.01f));
    }

    return 0;
}
