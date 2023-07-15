#include <ntddk.h>

#include "ExecutiveResource.h"

void ExecutiveResource::Init(void)
{
    ExInitializeResourceLite(&m_eresource);
}

void ExecutiveResource::Delete(void)
{
    ExDeleteResourceLite(&m_eresource);
}

void ExecutiveResource::Lock(void)
{
    m_in_critical_region = KeAreApcsDisabled();

    if (m_in_critical_region)
        ExAcquireResourceExclusiveLite(&m_eresource, TRUE);
    else
        ExEnterCriticalRegionAndAcquireResourceExclusive(&m_eresource);
}

void ExecutiveResource::Unlock(void)
{

    if (m_in_critical_region)
        ExReleaseResourceLite(&m_eresource);
    else
        ExReleaseResourceAndLeaveCriticalRegion(&m_eresource);
}

void ExecutiveResource::LockShared(void)
{
    m_in_critical_region = KeAreApcsDisabled();

    if (m_in_critical_region)
        ExAcquireResourceSharedLite(&m_eresource, TRUE);
    else
        ExEnterCriticalRegionAndAcquireResourceShared(&m_eresource);
}

void ExecutiveResource::UnlockShared(void)
{
    Unlock();
}