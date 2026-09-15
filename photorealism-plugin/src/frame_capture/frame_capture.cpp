#include "frame_capture.hpp"

#include "../runtime.hpp"
#include "bind_runs.hpp"
#include "capture_folder.hpp"
#include "capture_manifest.hpp"
#include "capture_schedule.hpp"
#include "staging_snapshots.hpp"
#include "target_description.hpp"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

namespace photorealism {
namespace {

using namespace frame_capture;

constexpr const char kIdleLabel[] = "Capturar quadro para analise";

SRWLOCK g_lock = SRWLOCK_INIT;
std::atomic<bool> g_recording{false};
CaptureSchedule g_schedule;
BindRuns g_runs;
StagingSnapshots g_snapshots;
std::vector<BindRecord> g_binds;
ID3D11DeviceContext* g_context = nullptr;
char g_status[160] = "Capturar quadro para analise";

void set_status(const char* text) {
    std::snprintf(g_status, sizeof(g_status), "%s", text);
}

void snapshot_released(const std::vector<ReleasedTarget>& released) {
    for (const ReleasedTarget& target : released) {
        g_snapshots.take(g_context, target);
        static_cast<ID3D11Texture2D*>(target.target.key.texture)->Release();
    }
}

void start_recording() {
    g_snapshots.reset();
    g_binds.clear();
    g_runs.reset();
    g_recording.store(true, std::memory_order_release);
}

void clear_recording() {
    g_recording.store(false, std::memory_order_release);
    g_snapshots.reset();
    g_binds.clear();
    g_runs.reset();
    if (g_context != nullptr) {
        g_context->Release();
        g_context = nullptr;
    }
}

void report(unsigned written, const std::string& name, unsigned long long started) {
    const unsigned total = static_cast<unsigned>(g_snapshots.records().size());
    log_message(
        "Captura de quadro 0.24.0 salva: %u de %u alvos em %s, %u binds%s, %llu ms. "
        "Gerar o relatorio com tools/gbuffer_report.py nessa pasta.",
        written, total, name.c_str(), g_runs.binds(),
        g_snapshots.truncated() ? " (truncada no limite de memoria)" : "",
        GetTickCount64() - started);
    char status[160] = {};
    std::snprintf(status, sizeof(status), "Captura salva: %u alvos (capturar de novo)", written);
    set_status(written > 0 ? status : "Captura falhou: veja o log");
}

void flush() {
    const unsigned long long started = GetTickCount64();
    std::vector<ReleasedTarget> released;
    g_runs.finish(&released);
    snapshot_released(released);
    std::wstring folder;
    std::string name;
    const bool folder_ready = g_context != nullptr && create_capture_folder(&folder, &name);
    const unsigned written = folder_ready ? g_snapshots.write_all(g_context, folder.c_str()) : 0u;
    const bool manifest_written =
        folder_ready && write_text_file(
                            folder, L"manifesto.json",
                            manifest_json(g_binds, g_snapshots.records(), g_snapshots.truncated()));
    g_schedule.finish(written > 0 && manifest_written);
    report(manifest_written ? written : 0u, name, started);
    clear_recording();
}

}

void request_frame_capture() {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted = g_schedule.request();
    if (accepted) {
        set_status("Capturando o proximo quadro...");
    }
    ReleaseSRWLockExclusive(&g_lock);
    log_message(
        accepted ? "Captura de quadro 0.24.0 pedida pelo menu; grava o proximo quadro."
                 : "Captura de quadro 0.24.0 ja em andamento; pedido ignorado.");
}

const char* frame_capture_status() {
    return g_status[0] != '\0' ? g_status : kIdleLabel;
}

void observe_capture_binds(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target) {
    if (!g_recording.load(std::memory_order_acquire) || context == nullptr) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted =
        g_schedule.recording() && (g_context == nullptr || g_context == context);
    if (accepted && g_context == nullptr) {
        context->AddRef();
        g_context = context;
    }
    std::vector<TargetInfo> targets =
        accepted ? describe_binding(render_target_count, render_targets, depth_target)
                 : std::vector<TargetInfo>();
    std::vector<ReleasedTarget> released;
    std::vector<TargetInfo> opened;
    const unsigned index = accepted ? g_runs.bind(&targets, &released, &opened) : 0u;
    for (const TargetInfo& target : opened) {
        static_cast<ID3D11Texture2D*>(target.key.texture)->AddRef();
    }
    if (accepted) {
        g_binds.push_back({index, targets});
    }
    snapshot_released(released);
    ReleaseSRWLockExclusive(&g_lock);
}

void end_capture_frame() {
    AcquireSRWLockExclusive(&g_lock);
    const FrameAction action = g_schedule.end_frame();
    if (action == FrameAction::StartRecording) {
        start_recording();
    }
    if (action == FrameAction::Flush) {
        g_recording.store(false, std::memory_order_release);
        flush();
    }
    ReleaseSRWLockExclusive(&g_lock);
}

}
