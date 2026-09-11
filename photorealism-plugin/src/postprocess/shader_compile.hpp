#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>

namespace photorealism {

using CompileFromFileFunction = decltype(&D3DCompileFromFile);

CompileFromFileFunction resolve_shader_compiler();

ID3DBlob* compile_shader_blob(
    CompileFromFileFunction compile_from_file,
    const wchar_t* path,
    const char* entry_point,
    const char* target,
    const char* stage);

}
