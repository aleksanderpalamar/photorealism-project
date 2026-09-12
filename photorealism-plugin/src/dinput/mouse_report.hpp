#pragma once

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>

namespace photorealism {
namespace dinput {

bool axes_are_relative(IDirectInputDevice8W* device);
void report_state(const void* data, DWORD size);
void report_data(const DIDEVICEOBJECTDATA* entries, DWORD count);
void clear_state(void* data, DWORD size);

}
}
