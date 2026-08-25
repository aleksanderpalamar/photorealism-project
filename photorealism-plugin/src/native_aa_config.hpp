#pragma once

#include <windows.h>

// Runs only from the dinput bootstrap worker, never under loader lock.
// Returns true when this process is ETS2/ATS and its config was inspected.
bool configure_native_aa_for_photorealism(HMODULE proxy_module);
