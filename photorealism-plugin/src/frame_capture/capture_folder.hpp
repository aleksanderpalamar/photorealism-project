#pragma once

#include <string>

namespace photorealism {
namespace frame_capture {

bool create_capture_folder(std::wstring* folder, std::string* name);
bool write_text_file(const std::wstring& folder, const wchar_t* file, const std::string& text);

}
}
