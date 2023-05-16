#pragma once

#include "DetectorPublic.h"
#include "ExecutiveResource.h"
#include "FastMutex.h"
#include "Locker.h"

#define DRIVER_TAG      'tcdK'
#define DRIVER_DEVNAME  L"\\Device\\RTDetector"
#define DRIVER_PREFIX    "RTDetector: "

struct ProcessInfo
{
    HANDLE pid;
};

template <typename T>
struct LinkedInformation
{
    LIST_ENTRY          link;
    T                   information;
};

struct ThreadInfoHead
{
    void Init(void);
    void Delete(void);
    void AddInfo(LIST_ENTRY *info);
    LIST_ENTRY *RemoveInfo(void);

private:
    LIST_ENTRY  m_head;
    FastMutex   m_lock;
};

struct ProcessesHead
{
    void Init(void);
    void Delete(void);
    void AddProcess(LIST_ENTRY *info);
    LIST_ENTRY * FindProcess(HANDLE pid);
    void RemoveProcess(LIST_ENTRY *entry);

private:
    LIST_ENTRY        m_head;
    ExecutiveResource m_lock;
};

#define err(msg, status) KdPrint((DRIVER_PREFIX "Error %s : (0x%08X)\n", msg, status))

void DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DriverRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static void OnCreateProcessNotify(PEPROCESS Process, HANDLE Pid, PPS_CREATE_NOTIFY_INFO CreateInfo);
static void OnCreateThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
static NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0);