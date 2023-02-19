#include <stdio.h>
#include <Windows.h>

#include "BoosterClient.h"
#include "BoosterCommon.h"

int main(int argc, char *argv[])
{
    int             tid = 0, priority = 0;
    HANDLE          device;

    if (argc < 3) {
        printf("[*] Usage: %s <threadid> <priority>\n", argv[0]);
        return 0;
    }

    tid = atoi(argv[1]);
    priority = atoi(argv[2]);

    device = CreateFileW(L"\\\\.\\Booster", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (device == INVALID_HANDLE_VALUE) {
        perr("[-] Failed to open device");
        goto out;
    }
    

    if (!change_priority(tid, priority, device)) {
        perr("[-] Failed to change priority");
        goto out;
    }

    printf("[+] Priority of thread %d successfully changed to %d\n", tid, priority);

out:
    if (device != INVALID_HANDLE_VALUE)
        CloseHandle(device);

    return 0;
}

BOOL change_priority(int tid, int priority, HANDLE device)
{
    THREAD_DATA data;
    DWORD       written;

    data.ThreadId = tid;
    data.Priority = priority;

    return WriteFile(device, &data, sizeof(data), &written, nullptr);
}