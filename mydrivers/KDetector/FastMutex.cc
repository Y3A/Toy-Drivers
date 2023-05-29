#include "FastMutex.h"

void FastMutex::Init(void)
{
    ExInitializeFastMutex(&m_mutex);
}

void FastMutex::Lock(void)
{
    ExAcquireFastMutex(&m_mutex);
}

void FastMutex::Unlock(void)
{
    ExReleaseFastMutex(&m_mutex);
}