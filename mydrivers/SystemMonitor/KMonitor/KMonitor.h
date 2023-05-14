#pragma once

#include <ntddk.h>

#include "FastMutex.h"

#define SYSMON_DEVNAME  L"\\Device\\KernMon"
#define SYSMON_TAG      'nmsS'
#define SYSMON_PREFIX   "SysmonDriver: "

template <typename T>
struct LinkedInformation
{
    LIST_ENTRY  link;
    T           information;
};

// Head node for all information structs
// Notice the use of type polymorphism with embedded LIST_ENTRY*

struct InformationHead
{
    void Init(ULONG maxItems);
    void AddItem(LIST_ENTRY *information);
    void ReturnItem(LIST_ENTRY *information);
    LIST_ENTRY *RemoveItem(void);

private:
    LIST_ENTRY  m_link;
    ULONG       m_count;
    ULONG       m_maxCount;
    FastMutex   m_lock;
};

static void OnCreateProcessNotify(PEPROCESS Process, HANDLE Pid, PPS_CREATE_NOTIFY_INFO CreateInfo);
static void OnCreateThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
static void OnLoadImageNotify(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);
NTSTATUS SysMonCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SysMonRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
void SysMonUnload(PDRIVER_OBJECT DriverObject);
static NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0);