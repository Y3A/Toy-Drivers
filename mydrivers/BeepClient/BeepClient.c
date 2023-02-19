#include <Windows.h>
#include <stdio.h>
#include <winternl.h>
#include <ntddbeep.h>

#pragma comment(lib, "ntdll")

void call_beep(DWORD freq, DWORD dur);

int main(int argc, char *argv[])
{
    DWORD freq = 0, dur = 0;

    if (argc != 3) {
        printf("[*] Usage: %s <frequency> <duration miliseconds>", argv[0]);
        return 0;
    }

    freq = atoi(argv[1]);
    dur = atoi(argv[2]);

    if (!(freq && dur)) {
        printf("[*] Usage: %s <frequency> <duration miliseconds>", argv[0]);
        return 0;
    }

    call_beep(freq, dur);

    return 0;
}

void call_beep(DWORD freq, DWORD dur)
{
    HANDLE                  beep = INVALID_HANDLE_VALUE;
    OBJECT_ATTRIBUTES       attr;
    UNICODE_STRING          name;
    IO_STATUS_BLOCK         io_status;
    NTSTATUS                status;
    BEEP_SET_PARAMETERS     bparams;
    DWORD                   ret;

    RtlInitUnicodeString(&name, L"\\Device\\Beep");
    InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtOpenFile(&beep, GENERIC_WRITE, &attr, &io_status, FILE_SHARE_WRITE, 0);
    if (!NT_SUCCESS(status)) {
        printf("[-] Unable to open device handle with error 0x%08X\n", status);
        return;
    }

    bparams.Frequency = freq;
    bparams.Duration = dur;

    printf("[+] Start playing sound of %lu frequency for %lu miliseconds\n", freq, dur);

    if (!DeviceIoControl(beep, IOCTL_BEEP_SET, &bparams, sizeof(bparams), NULL, 0, &ret, NULL)) {
        printf("[-] Unable to talk to beep driver with error 0x%08X", GetLastError());
        goto out;
    }

    Sleep(dur);

out:
    if (beep != INVALID_HANDLE_VALUE)
        CloseHandle(beep);

    return;
}
