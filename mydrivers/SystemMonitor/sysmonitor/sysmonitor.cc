#include <Windows.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <unordered_map>

#include "../KMonitor/SystemMonitorPublic.h"

#define err(msg) printf("Error %s : (0x%08X)\n", msg, GetLastError())

#define SZ_PER_READ 0x1000

void KeTimeToSystemTime(PLARGE_INTEGER ketime, SYSTEMTIME *st);
void display_data(char *buf, DWORD size);
std::wstring GetDosNameFromNTName(PCWSTR path);

int main(void)
{
    HANDLE      driver;
    char        *buf = NULL;
    DWORD       read;

    driver = CreateFileW(
        SYSMON_NAME,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (driver == INVALID_HANDLE_VALUE) {
        err("CreateFileW");
        goto out;
    }

    buf = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, SZ_PER_READ+1);
    if (!buf) {
        err("HeapAlloc");
        goto out;
    }

    while (1) {
        read = 0;
        ReadFile(driver, buf, SZ_PER_READ, &read, NULL);

        if (read > 0)
            display_data(buf, read);

        Sleep(500);
    }

out:
    if (driver != INVALID_HANDLE_VALUE)
        CloseHandle(driver);

    if (buf)
        HeapFree(GetProcessHeap(), 0, buf);

    return 0;
}

void display_data(char *buf, DWORD size)
{
    SYSTEMTIME  st;
    DWORD       sz = size;

    for (auto cur = (InfoHeader *)buf; sz > 0;  sz -= (ULONG_PTR)cur->size, cur = (InfoHeader *)((ULONG_PTR)cur + (ULONG_PTR)(cur->size))) {
        switch (cur->type)
        {
            case InfoType::ProcessCreate:
            {
                auto    create_info = (ProcessCreateInfo *)cur;
                SHORT   cmdlinelen = create_info->commandLineLen;
                auto    cmdline = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cmdlinelen + 1);

                if (!cmdline) {
                    err("HeapAlloc");
                    break;
                }

                while (cmdlinelen-- > 0)
                    cmdline[cmdlinelen] = (CHAR)(create_info->commandLine[cmdlinelen]);

                RtlZeroMemory(&st, sizeof(SYSTEMTIME));
                KeTimeToSystemTime(&create_info->time, &st);

                printf("[%02d:%02d:%02d.%03d] Process %u Created. Command Line: %s\n", \
                    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
                    create_info->pid, cmdline);

                HeapFree(GetProcessHeap(), 0, cmdline);
                break;
            }
            case InfoType::ProcessExit:
            {
                auto    exit_info = (ProcessExitInfo *)cur;

                RtlZeroMemory(&st, sizeof(SYSTEMTIME));
                KeTimeToSystemTime(&exit_info->time, &st);

                printf("[%02d:%02d:%02d.%03d] Process %u Exited. Exit Code: 0x%08X\n", \
                    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
                    exit_info->pid, exit_info->exitCode);
                break;
            }
            case InfoType::ThreadCreate:
            {
                auto create_info = (ThreadCreateInfo *)cur;

                RtlZeroMemory(&st, sizeof(SYSTEMTIME));
                KeTimeToSystemTime(&create_info->time, &st);

                printf("[%02d:%02d:%02d.%03d] Thread %u Created In Process %u\n", \
                    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
                    create_info->tid, create_info->pid);
                break;
            }
            case InfoType::ThreadExit:
            {
                auto exit_info = (ThreadExitInfo *)cur;

                RtlZeroMemory(&st, sizeof(SYSTEMTIME));
                KeTimeToSystemTime(&exit_info->time, &st);

                printf("[%02d:%02d:%02d.%03d] Thread %u Exited In Process %u (Code: 0x%08X)\n", \
                    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
                    exit_info->tid, exit_info->pid, exit_info->exitCode);
                break;
            }
            case InfoType::ImageLoad:
            {
                auto load_info = (ImageLoadInfo *)cur;

                RtlZeroMemory(&st, sizeof(SYSTEMTIME));
                KeTimeToSystemTime(&load_info->time, &st);

                printf("[%02d:%02d:%02d.%03d] Image %ws Loaded Into Process %u At Address 0x%08llX\n", \
                    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
                    GetDosNameFromNTName(load_info->imageName).c_str(), load_info->pid, load_info->loadAddress);
                break;
            }
        }
    }
    return;
}

void KeTimeToSystemTime(PLARGE_INTEGER ketime, SYSTEMTIME *st)
{
    FILETIME local;

    FileTimeToLocalFileTime((FILETIME *)ketime, &local);
    FileTimeToSystemTime(&local, st);

    return;
}

std::wstring GetDosNameFromNTName(PCWSTR path)
{
    if (path[0] != L'\\')
        return path;

    static std::unordered_map<std::wstring, std::wstring> map;

    if (map.empty()) {
        auto drives = GetLogicalDrives();
        int c = 0;
        WCHAR root[] = L"X:";
        WCHAR target[128];
        while (drives) {
            if (drives & 1) {
                root[0] = 'A' + c;
                if (QueryDosDevice(root, target, _countof(target))) {
                    map.insert({ target, root });
                }
            }
            drives >>= 1;
            c++;
        }
    }
    auto pos = wcschr(path + 1, L'\\');
    if (pos == nullptr)
        return path;

    pos = wcschr(pos + 1, L'\\');
    if (pos == nullptr)
        return path;

    std::wstring ntname(path, pos - path);
    auto it = map.find(ntname);

    if (it != map.end())
        return it->second + std::wstring(pos);

    return path;
}