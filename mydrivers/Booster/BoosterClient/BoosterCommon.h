#ifndef BOOSTER_COMM_H
#define BOOSTER_COMM_H

typedef unsigned long DWORD;

typedef struct _THREAD_DATA
{
    DWORD ThreadId;
    DWORD Priority;
} THREAD_DATA, *PTHREAD_DATA;

#endif