#include <Windows.h>
#include <stdio.h>

#include "..\DeleteProtector\DeleteProctectorPublic.h"

int Error(const char *msg)
{
    printf("%s (Error: %u)\n", msg, GetLastError());
    return 1;
}

int PrintUsage()
{
    printf("Protect [add | remove | clear] [pid] ...\n");
    return 0;
}

int wmain(int argc, const WCHAR *argv[])
{
    if (argc < 2)
        return PrintUsage();

    enum class Options
    {
        Unknown,
        Add, Remove, Clear
    };
    Options option;
    if (_wcsicmp(argv[1], L"add") == 0)
        option = Options::Add;
    else if (_wcsicmp(argv[1], L"remove") == 0)
        option = Options::Remove;
    else if (_wcsicmp(argv[1], L"clear") == 0)
        option = Options::Clear;
    else {
        printf("Unknown option.\n");
        return PrintUsage();
    }

    HANDLE hFile = CreateFile(DEVICE_NAME, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return Error("Failed to open device");

    const WCHAR *ext;
    BOOL success = FALSE;
    DWORD bytes;

    switch (option) {
    case Options::Add:
        if (argc < 3) {
            PrintUsage();
            goto out;
        }
        ext = argv[2];
        success = DeviceIoControl(hFile, IOCTL_KPROTECT_ADD_EXT,
            (LPVOID)ext, (wcslen(ext) + 1) * sizeof(WCHAR),
            nullptr, 0, &bytes, nullptr);
        break;

    case Options::Remove:
        if (argc < 3) {
            PrintUsage();
            goto out;
        }
        ext =argv[2];
        success = ::DeviceIoControl(hFile, IOCTL_KPROTECT_REMOVE_EXT,
            (LPVOID)ext, (wcslen(ext) + 1) * sizeof(WCHAR),
            nullptr, 0, &bytes, nullptr);
        break;

    case Options::Clear:
        success = ::DeviceIoControl(hFile, IOCTL_KPROTECT_REMOVE_ALL,
            nullptr, 0, nullptr, 0, &bytes, nullptr);
        break;

    }

    if (!success)
        return Error("Failed in DeviceIoControl");

    printf("Operation succeeded.\n");

out:
    CloseHandle(hFile);
    return 0;
}