#include "capture_manifest.hpp"

#include <cstdio>

namespace photorealism {
namespace frame_capture {
namespace {

void append_target(std::string* text, const TargetInfo& target) {
    char buffer[256] = {};
    std::snprintf(
        buffer, sizeof(buffer),
        "\"slot\": %u, \"id\": %u, \"depth\": %s, \"largura\": %u, \"altura\": %u, "
        "\"formato\": %u, \"formato_view\": %u, \"mip\": %u, \"fatia\": %u, "
        "\"amostras\": %u",
        target.slot, target.identity, target.depth ? "true" : "false", target.width,
        target.height, target.texture_format, target.view_format, target.key.mip,
        target.key.slice, target.samples);
    text->append(buffer);
}

void append_bind(std::string* text, const BindRecord& bind, bool last) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "    {\"bind\": %u, \"alvos\": [", bind.index);
    text->append(buffer);
    for (std::size_t index = 0; index < bind.targets.size(); ++index) {
        text->append("{");
        append_target(text, bind.targets[index]);
        text->append(index + 1 < bind.targets.size() ? "}, " : "}");
    }
    text->append(last ? "]}\n" : "]},\n");
}

void append_snapshot(std::string* text, const SnapshotRecord& snapshot, bool last) {
    char buffer[128] = {};
    std::snprintf(
        buffer, sizeof(buffer), "    {\"primeiro_bind\": %u, \"ultimo_bind\": %u, ",
        snapshot.first_bind, snapshot.last_bind);
    text->append(buffer);
    append_target(text, snapshot.target);
    text->append(", \"arquivo\": \"");
    text->append(snapshot.file);
    text->append("\", \"falha\": ");
    text->append(snapshot.failure != nullptr ? "\"" : "null");
    text->append(snapshot.failure != nullptr ? snapshot.failure : "");
    text->append(snapshot.failure != nullptr ? "\"" : "");
    text->append(last ? "}\n" : "},\n");
}

void append_constant(std::string* text, const ConstantRecord& constant, bool last) {
    char buffer[160] = {};
    std::snprintf(
        buffer, sizeof(buffer),
        "    {\"bind\": %u, \"estagio\": \"%s\", \"slot\": %u, \"bytes\": %u, \"arquivo\": \"",
        constant.bind, constant.stage, constant.slot, constant.bytes);
    text->append(buffer);
    text->append(constant.file);
    text->append("\", \"falha\": ");
    text->append(constant.failure != nullptr ? "\"" : "null");
    text->append(constant.failure != nullptr ? constant.failure : "");
    text->append(constant.failure != nullptr ? "\"" : "");
    text->append(last ? "}\n" : "},\n");
}

}

std::string manifest_json(
    const std::vector<BindRecord>& binds,
    const std::vector<SnapshotRecord>& snapshots,
    const std::vector<ConstantRecord>& constants,
    bool truncated) {
    std::string text = "{\n  \"versao\": \"0.24.1\",\n  \"truncado\": ";
    text.append(truncated ? "true" : "false");
    text.append(",\n  \"binds\": [\n");
    for (std::size_t index = 0; index < binds.size(); ++index) {
        append_bind(&text, binds[index], index + 1 == binds.size());
    }
    text.append("  ],\n  \"capturas\": [\n");
    for (std::size_t index = 0; index < snapshots.size(); ++index) {
        append_snapshot(&text, snapshots[index], index + 1 == snapshots.size());
    }
    text.append("  ],\n  \"constantes\": [\n");
    for (std::size_t index = 0; index < constants.size(); ++index) {
        append_constant(&text, constants[index], index + 1 == constants.size());
    }
    text.append("  ]\n}\n");
    return text;
}

}
}
