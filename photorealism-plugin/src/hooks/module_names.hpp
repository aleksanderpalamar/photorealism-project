#pragma once

#include <cstddef>

namespace photorealism {

const char* module_name_for_address(
    const void* address, char* output, std::size_t size);

}
