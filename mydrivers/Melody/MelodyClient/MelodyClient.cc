// Melody.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <stdio.h>
#include "..\KMelody\MelodyPublic.h"

int Error(const char *msg, DWORD error = GetLastError())
{
    printf("%s (%u)\n", msg, error);
    return 1;
}

int main()
{
    HANDLE hDevice = CreateFile(MELODY_SYMLINK, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE)
        return Error("Failed to open device handle");

    //
    // let's play some notes
    //
    Note notes[10];
    for (int i = 0; i < _countof(notes); i++) {
        notes[i].dwFrequency = 400 + i * 30;
        notes[i].dwDuration = 500;
    }
    DWORD bytes;
    if (!DeviceIoControl(hDevice, MELODY_IOCTL_PLAY, notes, sizeof(notes), nullptr, 0, &bytes, nullptr))
        return Error("Failed in DeviceIoControl");

    for (int i = 0; i < _countof(notes); i++) {
        notes[i].dwFrequency = 1200 - i * 100;
        notes[i].dwDuration = 300;
        notes[i].dwRepeat = 2;
        notes[i].dwDelay = 300;
    }
    if (!DeviceIoControl(hDevice, MELODY_IOCTL_PLAY, notes, sizeof(notes), nullptr, 0, &bytes, nullptr))
        return Error("Failed in DeviceIoControl");

    CloseHandle(hDevice);
    return 0;
}
