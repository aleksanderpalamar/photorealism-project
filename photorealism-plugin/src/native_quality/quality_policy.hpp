#pragma once

#include "quality_settings.hpp"

#include <windows.h>

namespace photorealism {
namespace native_quality {

QualityPolicy read_native_quality_policy(HMODULE proxy_module);

}
}
