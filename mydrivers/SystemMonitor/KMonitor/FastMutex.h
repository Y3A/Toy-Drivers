#pragma once

#include <ntddk.h>

struct FastMutex
{
    void Init(void)
    {
        ExInitializeFastMutex(&m_mutex);
    }

    void Lock(void)
    {
        ExAcquireFastMutex(&m_mutex);
    }

    void Unlock(void)
    {
        ExReleaseFastMutex(&m_mutex);
    }

private:
    FAST_MUTEX m_mutex;
};