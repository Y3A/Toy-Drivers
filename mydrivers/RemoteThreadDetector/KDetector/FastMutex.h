#pragma once

#include <ntddk.h>

struct FastMutex
{
    void Init(void);
    void Lock(void);
    void Unlock(void);

private:
    FAST_MUTEX m_mutex;
};