#ifndef MELODY_COMMON_H
#define MELODY_COMMON_H

#define MELODY_SYMLINK      L"\\??\\KMelody"
#define MELODY_DEVICE       0x8028
#define MELODY_IOCTL_BASE   0x800

#define MELODY_IOCTL_PLAY \
    CTL_CODE(MELODY_DEVICE, MELODY_IOCTL_BASE, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef unsigned long DWORD;

struct Note
{
    DWORD dwFrequency;
    DWORD dwDuration;
    DWORD dwDelay{ 0 };
    DWORD dwRepeat{ 1 };
};

#endif