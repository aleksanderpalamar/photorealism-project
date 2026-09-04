#pragma once

#include <windows.h>

namespace photorealism {

void set_module(HMODULE module);
const wchar_t* module_directory();
const wchar_t* plugin_root();
const wchar_t* config_path();
const wchar_t* shader_path();
const wchar_t* depth_preview_shader_path();
const wchar_t* ssao_shader_path();
const wchar_t* temporal_shader_path();
const wchar_t* bloom_shader_path();
void log_message(const char* format, ...);

}  // namespace photorealism
