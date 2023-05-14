#pragma once


#define SYSMON_NAME L"\\??\\KernMon"
#define SYSMON_MAX  10000

enum class InfoType : short
{
    None,
    ProcessCreate,
    ProcessExit,
    ThreadCreate,
    ThreadExit,
    ImageLoad,
};

struct InfoHeader
{
    InfoType        type;
    USHORT          size;
    LARGE_INTEGER   time;
};

// Individual information types

struct ProcessExitInfo : InfoHeader
{
    ULONG       pid;
    ULONG       exitCode;
};

struct ProcessCreateInfo : InfoHeader
{
    ULONG       pid;
    ULONG       ppid;
    ULONG       creatingThreadId;
    USHORT      commandLineLen;
    WCHAR       commandLine[1];
};

struct ThreadCreateInfo : InfoHeader
{
    ULONG       tid;
    ULONG       pid;
};

struct ThreadExitInfo : InfoHeader
{
    ULONG       tid;
    ULONG       pid;
    ULONG       exitCode;
};

struct ImageLoadInfo : InfoHeader
{
    ULONG       pid;
    ULONG       imageSize;
    ULONG_PTR   loadAddress;
    USHORT      imageNameLen;
    WCHAR       imageName[1];
};