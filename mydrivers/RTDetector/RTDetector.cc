#include <Windows.h>
#include <stdio.h>

#include "RTDetector.h"
#include "..\KDetector\DetectorPublic.h"

int main(void)
{
    HANDLE           driver = nullptr;
    RemoteThreadInfo rt[10];
    DWORD            bytes;

    driver = CreateFileW(
        DRIVER_NAME,
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    if (driver == INVALID_HANDLE_VALUE) {
        puts("Create file fail");
        return 0;
    }

    while (true) {
        RtlZeroMemory(&rt, sizeof(rt));
        if (!ReadFile(driver, &rt, sizeof(rt), &bytes, nullptr)) {
            puts("Failed to read data");
            break;
        }

        DisplayData(rt, bytes / sizeof(rt[0]));
        Sleep(1000);
    }

    return 0;
}

void DisplayData(RemoteThreadInfo *rt, DWORD count)
{
    for (int i = 0; i < count; i++) {
        DisplayTime(rt[i].time);
        printf("Process %d (thread %d) created remote thread in process %d (thread %d)\n", \
            rt[i].creator_pid, rt[i].creator_tid, rt[i].target_pid, rt[i].target_tid);
    }
}

void DisplayTime(const LARGE_INTEGER &time)
{
    FILETIME local;
    FileTimeToLocalFileTime((FILETIME *)&time, &local);
    SYSTEMTIME st;
    FileTimeToSystemTime(&local, &st);
    printf("%02d:%02d:%02d.%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}