#ifndef MEMORY_H
#define MEMORY_H

#include <ntddk.h>

#include "MelodyDriver.h"

void *operator new(size_t count, POOL_FLAGS flags, ULONG tag = MELODY_TAG);
void *operator new(size_t count, POOL_FLAGS flags, EX_POOL_PRIORITY priority, ULONG tag = MELODY_TAG);

void operator delete(void *p, size_t);

#endif