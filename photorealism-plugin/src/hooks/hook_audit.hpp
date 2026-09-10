#pragma once

namespace photorealism {

void log_present_entry(
    const char* phase,
    const char* method,
    void** entry,
    const void* replacement,
    const void* downstream);

}
