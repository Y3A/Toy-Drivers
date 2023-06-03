#pragma once

#include "ExecutiveResource.h"
#include "Locker.h"
#include "LookasideList.h"
#include "ProtectorPublic.h"

#define DRIVER_TAG      'tcpK'
#define DRIVER_DEVNAME  L"\\Device\\KProtector"
#define DRIVER_PREFIX   "KProtector: "

#define PROCESS_TERMINATE 1

struct ProcessItem
{
    LIST_ENTRY          link;
    ULONG               pid;
};

struct ProcessesHead
{
    NTSTATUS Init(void);
    void     Delete(void);
    bool     AddProcess(ULONG pid);
    bool     ProcessExists(ULONG pid);
    void     FindAndRemoveProcess(ULONG pid);
    void     RemoveAll(void);

private:
    LIST_ENTRY                     *InternalFindProcessCallerLocked(ULONG pid);
    LIST_ENTRY                     m_head;
    ExecutiveResource              m_lock;
    LookasideList<ProcessItem>     m_pool;
};

#define err(msg, status) KdPrint((DRIVER_PREFIX "Error %s : (0x%08X)\n", msg, status))
#define log(msg) KdPrint((DRIVER_PREFIX "%s\n", msg))

#define DEREF_PTR(exp) *(exp)

void DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static void OnCreateProcessNotify(PEPROCESS Process, HANDLE Pid, PPS_CREATE_NOTIFY_INFO CreateInfo);
static OB_PREOP_CALLBACK_STATUS OnAcquireHandleNotify(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation);
static NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0);