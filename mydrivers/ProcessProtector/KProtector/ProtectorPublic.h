#pragma once

#define DRIVER_NAME     L"\\??\\PProtector"
#define DRIVER_IOCTL_BASE 0x8206

#define IOCTL_KPROTECT_ADD_PID \
    CTL_CODE(DRIVER_IOCTL_BASE, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_KPROTECT_REMOVE_PID \
    CTL_CODE(DRIVER_IOCTL_BASE, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_KPROTECT_REMOVE_ALL \
    CTL_CODE(DRIVER_IOCTL_BASE, 0x802, METHOD_NEITHER, FILE_WRITE_ACCESS)