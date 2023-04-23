#ifndef FAST_MUTEX_H
#define FAST_MUTEX_H

#include <ntifs.h>
#include <ntddk.h>

struct FastMutex
{
    explicit FastMutex()
    {
        ExInitializeFastMutex(&m_mutex);
    }

    void Lock()
    {
        ExAcquireFastMutex(&m_mutex);
    }

    void Unlock()
    {
        ExReleaseFastMutex(&m_mutex);
    }

private:
    FAST_MUTEX m_mutex;
};

#endif