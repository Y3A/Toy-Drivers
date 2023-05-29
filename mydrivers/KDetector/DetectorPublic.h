#pragma once

#define DRIVER_NAME     L"\\??\\RTDetector"

struct RemoteThreadInfo
{
    LARGE_INTEGER   time;
    SIZE_T          size;
    ULONG           creator_pid;
    ULONG           creator_tid;
    ULONG           target_pid;
    ULONG           target_tid;
};