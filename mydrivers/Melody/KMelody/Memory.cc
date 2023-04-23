#include <ntddk.h>

#include "Memory.h"

void *operator new(size_t count, POOL_FLAGS flags, ULONG tag)
{
    return ExAllocatePool2(flags, count, tag);
}

void *operator new(size_t count, POOL_FLAGS flags, EX_POOL_PRIORITY priority, ULONG tag)
{
    POOL_EXTENDED_PARAMETER pp;

    pp.Priority = priority;
    pp.Type = PoolExtendedParameterPriority;
    pp.Optional = FALSE;

    return ExAllocatePool3(flags, count, tag, &pp, 1);
}

void operator delete(void *p, size_t)
{
    ExFreePool(p);
    return;
}