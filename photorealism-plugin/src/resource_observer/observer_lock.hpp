#pragma once

#include "observer_state.hpp"

#include <windows.h>

namespace photorealism {
namespace observer {

class WriteLock {
  public:
    WriteLock() { AcquireSRWLockExclusive(&g_observer_lock); }
    ~WriteLock() { release(); }

    WriteLock(const WriteLock&) = delete;
    WriteLock& operator=(const WriteLock&) = delete;

    void release() {
        if (held_) {
            ReleaseSRWLockExclusive(&g_observer_lock);
            held_ = false;
        }
    }

  private:
    bool held_ = true;
};

class ReadLock {
  public:
    ReadLock() { AcquireSRWLockShared(&g_observer_lock); }
    ~ReadLock() { ReleaseSRWLockShared(&g_observer_lock); }

    ReadLock(const ReadLock&) = delete;
    ReadLock& operator=(const ReadLock&) = delete;
};

}
}
