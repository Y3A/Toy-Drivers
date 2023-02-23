#ifndef ZERO_COMMON
#define ZERO_COMMON

#define CTL_CODE( DeviceType, Function, Method, Access ) ( \
  ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

#define DEVICE_ZERO 0x9191
 
#define IOCTL_ZERO_GET_STATS \
  CTL_CODE(DEVICE_ZERO, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ZERO_CLEAR_STATS \
  CTL_CODE(DEVICE_ZERO, 0x801, METHOD_NEITHER, FILE_ANY_ACCESS)

typedef struct
{
    DWORD TotalRead;
    DWORD TotalWrite;
} ZEROSTATS, *PZEROSTATS;

#endif