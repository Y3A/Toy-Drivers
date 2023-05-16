#pragma once

#include <ntddk.h>

template <typename T>
struct LookasideList
{
    NTSTATUS Init(POOL_TYPE poolType, ULONG driverTag)
    {
        return ExInitializeLookasideListEx(
            &m_list,
            nullptr,
            nullptr,
            poolType,
            0,
            sizeof(T),
            driverTag,
            0
        );
    }

    void Delete(void)
    {
        ExDeleteLookasideListEx(&m_list);
    }

    T *Alloc(void)
    {
        return (T *)ExAllocateFromLookasideListEx(&m_list);
    }

    void Free(T *p)
    {
        ExFreeToLookasideListEx(&m_list, (PVOID)p);
    }

private:
    LOOKASIDE_LIST_EX m_list;
};