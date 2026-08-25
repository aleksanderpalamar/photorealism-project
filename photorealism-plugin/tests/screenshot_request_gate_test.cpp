#include "../src/screenshot_request_gate.hpp"

#include <cassert>

using photorealism::steam_screenshot::kDuplicateRequestWindowMs;
using photorealism::steam_screenshot::request_is_duplicate;

int main() {
    assert(!request_is_duplicate(1000, 0, false));
    assert(request_is_duplicate(1001, 1000, true));
    assert(request_is_duplicate(
        1000 + kDuplicateRequestWindowMs - 1, 1000, false));
    assert(!request_is_duplicate(
        1000 + kDuplicateRequestWindowMs, 1000, false));
    assert(!request_is_duplicate(10, 1000, false));
    return 0;
}
