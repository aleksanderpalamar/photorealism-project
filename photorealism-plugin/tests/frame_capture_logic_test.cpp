#include "../src/frame_capture/bind_runs.hpp"
#include "../src/frame_capture/capture_manifest.hpp"
#include "../src/frame_capture/capture_schedule.hpp"
#include "../src/frame_capture/dds_header.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace photorealism::frame_capture;

namespace {

int g_textures[8] = {};

TargetInfo target(int texture, unsigned slot, unsigned format = 10) {
    TargetInfo info;
    info.key.texture = &g_textures[texture];
    info.slot = slot;
    info.width = 1920;
    info.height = 1080;
    info.texture_format = format;
    info.view_format = format;
    return info;
}

std::uint32_t read_u32(const std::array<std::uint8_t, kDdsHeaderBytes>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

void the_schedule_records_exactly_one_frame() {
    CaptureSchedule schedule;
    assert(schedule.end_frame() == FrameAction::None);
    assert(schedule.request());
    assert(!schedule.request());
    assert(!schedule.recording());
    assert(schedule.end_frame() == FrameAction::StartRecording);
    assert(schedule.recording());
    assert(!schedule.request());
    assert(schedule.end_frame() == FrameAction::Flush);
    schedule.finish(true);
    assert(schedule.state() == CaptureState::Saved);
    assert(schedule.end_frame() == FrameAction::None);
    assert(schedule.request());
}

void a_target_is_released_when_the_next_pass_stops_binding_it() {
    BindRuns runs;
    std::vector<ReleasedTarget> released;
    std::vector<TargetInfo> opened;
    std::vector<TargetInfo> gbuffer = {target(1, 0), target(2, 1), target(3, 2), target(4, 3)};
    runs.bind(&gbuffer, &released, &opened);
    assert(released.empty() && opened.size() == 4);
    opened.clear();
    runs.bind(&gbuffer, &released, &opened);
    assert(released.empty() && opened.empty());

    std::vector<TargetInfo> lighting = {target(5, 0), target(2, 1)};
    runs.bind(&lighting, &released, &opened);
    assert(released.size() == 3);
    assert(released[0].first_bind == 1 && released[0].last_bind == 2);
    assert(opened.size() == 1);
    assert(lighting[1].identity == 2 && lighting[0].identity == 5);

    released.clear();
    runs.finish(&released);
    assert(released.size() == 2);
    const bool kept_run = released[0].target.identity == 2 || released[1].target.identity == 2;
    assert(kept_run);
    for (const ReleasedTarget& run : released) {
        const bool gbuffer_slot = run.target.identity == 2;
        assert(!gbuffer_slot || (run.first_bind == 1 && run.last_bind == 3));
    }
}

void a_rebound_texture_opens_a_new_run_with_the_same_identity() {
    BindRuns runs;
    std::vector<ReleasedTarget> released;
    std::vector<TargetInfo> opened;
    std::vector<TargetInfo> first = {target(6, 0)};
    std::vector<TargetInfo> second = {target(7, 0)};
    std::vector<TargetInfo> again = {target(6, 0)};
    runs.bind(&first, &released, &opened);
    runs.bind(&second, &released, &opened);
    runs.bind(&again, &released, &opened);
    assert(released.size() == 2);
    assert(again[0].identity == first[0].identity);
    assert(opened.size() == 3);
}

void the_dds_header_carries_the_real_format() {
    const auto header = dds_header(1920, 1080, 10);
    assert(read_u32(header, 0) == 0x20534444);
    assert(read_u32(header, 4) == 124);
    assert(read_u32(header, 12) == 1080 && read_u32(header, 16) == 1920);
    assert(read_u32(header, 84) == 0x30315844);
    assert(read_u32(header, 128) == 10 && read_u32(header, 132) == 3);
    assert(bytes_per_pixel(10) == 8);
    assert(bytes_per_pixel(26) == 4);
    assert(bytes_per_pixel(49) == 2);
    assert(bytes_per_pixel(19) == 8);
    assert(bytes_per_pixel(87) == 4);
    assert(bytes_per_pixel(71) == 0);
}

void the_manifest_lists_binds_and_files() {
    BindRecord bind;
    bind.index = 24;
    bind.targets = {target(1, 0), target(2, 1)};
    SnapshotRecord snapshot;
    snapshot.target = target(1, 0);
    snapshot.first_bind = 24;
    snapshot.last_bind = 25;
    snapshot.file = "000_b024-025_id01_s0.dds";
    SnapshotRecord failed = snapshot;
    failed.failure = "formato sem tamanho conhecido";
    ConstantRecord constant;
    constant.bind = 144;
    constant.stage = "ps";
    constant.slot = 2;
    constant.bytes = 256;
    constant.file = "cb_b144_ps2.bin";
    const std::string json = manifest_json({bind}, {snapshot, failed}, {constant}, false);
    assert(json.find("\"constantes\": [") != std::string::npos);
    assert(json.find("\"estagio\": \"ps\", \"slot\": 2, \"bytes\": 256, \"arquivo\": \"cb_b144_ps2.bin\"") != std::string::npos);
    assert(json.find("\"bind\": 24") != std::string::npos);
    assert(json.find("\"arquivo\": \"000_b024-025_id01_s0.dds\"") != std::string::npos);
    assert(json.find("\"falha\": null") != std::string::npos);
    assert(json.find("\"falha\": \"formato sem tamanho conhecido\"") != std::string::npos);
    assert(json.find("\"truncado\": false") != std::string::npos);
}

}

int main() {
    the_schedule_records_exactly_one_frame();
    a_target_is_released_when_the_next_pass_stops_binding_it();
    a_rebound_texture_opens_a_new_run_with_the_same_identity();
    the_dds_header_carries_the_real_format();
    the_manifest_lists_binds_and_files();
    std::printf("frame_capture_logic_test ok\n");
    return 0;
}
