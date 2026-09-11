#pragma once

#include "aa_settings.hpp"

#include <windows.h>

namespace photorealism {
namespace native_aa {

AaPolicy read_native_aa_policy(HMODULE proxy_module);

}
}
