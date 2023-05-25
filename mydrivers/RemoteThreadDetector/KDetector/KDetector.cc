#include <ntddk.h>

#include "KDetector.h"
#include "LookasideList.h"

static ThreadInfoHead                                       g_ThreadInfo;
static LookasideList<LinkedInformation<RemoteThreadInfo>>   g_ThreadInfoPool;
static ProcessesHead                                        g_Processes;
static LookasideList<LinkedInformation<ProcessInfo>>        g_ProcessesPool;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS        status = STATUS_SUCCESS;
    PDEVICE_OBJECT  deviceObj = nullptr;
    UNICODE_STRING  devName = RTL_CONSTANT_STRING(DRIVER_DEVNAME);
    UNICODE_STRING  symLink = RTL_CONSTANT_STRING(DRIVER_NAME);
    bool            symLinkCreated = false;
    bool            processCallbackSet = false;
    bool            threadCallbackSet = false;
    bool            processesPoolInit = false;
    bool            threadInfoPoolInit = false;

    status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObj);
    if (!NT_SUCCESS(status)) {
        err("IoCreateDevice", status);
        goto out;
    }
    deviceObj->Flags |= DO_DIRECT_IO;

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        err("IoCreateSymbolicLink", status);
        goto out;
    }
    symLinkCreated = true;

    status = PsSetCreateProcessNotifyRoutineEx(OnCreateProcessNotify, FALSE);
    if (!NT_SUCCESS(status)) {
        err("PsSetCreateProcessNotifyRoutineEx", status);
        goto out;
    }
    processCallbackSet = true;

    status = PsSetCreateThreadNotifyRoutine(OnCreateThreadNotify);
    if (!NT_SUCCESS(status)) {
        err("PsSetCreateThreadNotifyRoutine", status);
        goto out;
    }
    threadCallbackSet = true;

    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = \
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = DriverRead;

    status = g_ProcessesPool.Init(PagedPool, DRIVER_TAG);
    if (!NT_SUCCESS(status)) {
        err("g_ProcessesPool.Init()", status);
        goto out;
    }
    processesPoolInit = true;

    status = g_ThreadInfoPool.Init(PagedPool, DRIVER_TAG);
    if (!NT_SUCCESS(status)) {
        err("g_ThreadInfoPool.Init()", status);
        goto out;
    }
    threadInfoPoolInit = true;

    g_ThreadInfo.Init();
    g_Processes.Init();

out:
    if (!NT_SUCCESS(status)) {
        if (deviceObj)
            IoDeleteDevice(deviceObj);
        if (symLinkCreated)
            IoDeleteSymbolicLink(&symLink);
        if (processCallbackSet)
            PsSetCreateProcessNotifyRoutineEx(OnCreateProcessNotify, TRUE);
        if (threadCallbackSet)
            PsRemoveCreateThreadNotifyRoutine(OnCreateThreadNotify);
        if (processesPoolInit)
            g_ProcessesPool.Delete();
        if (threadInfoPoolInit)
            g_ThreadInfoPool.Delete();
    }
    return status;
}

static NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info)
{
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static void OnCreateProcessNotify(PEPROCESS Process, HANDLE Pid, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    UNREFERENCED_PARAMETER(Process);

    if (CreateInfo == nullptr) {
        // Process exit
        // Edge case where process has no thread but exits, remove from list
        if (auto entry = g_Processes.FindProcess(Pid))
            g_Processes.RemoveProcess(entry);

        return;
    }

    // Process create
    // Add to new processes list
    auto entry = g_ProcessesPool.Alloc();
    entry->information.pid = Pid;
    g_Processes.AddProcess(&entry->link);

    return;
}

static void OnCreateThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create)
{
    if (!Create)
        return;

    // We only care about thread creation
    // Note here ProcessId and ThreadId belongs to target thread

    bool remote = PsGetCurrentProcessId() != ProcessId
        && PsInitialSystemProcess != PsGetCurrentProcess()
        && PsGetProcessId(PsInitialSystemProcess) != ProcessId;
    if (!remote)
        return;

    // Check if thread is "true remote" : not first thread in a process
    auto entry = g_Processes.FindProcess(ProcessId);
    if (entry) {
        // fake remote
        KdPrint(("Fake remote pid %d\n", HandleToUlong(ProcessId)));
        g_Processes.RemoveProcess(entry);
        return;
    }

    // True remote, log it
    auto info = g_ThreadInfoPool.Alloc();
    info->information.target_pid = HandleToUlong(ProcessId);
    info->information.target_tid = HandleToUlong(ThreadId);
    info->information.creator_pid = HandleToUlong(PsGetCurrentProcessId());
    info->information.creator_tid = HandleToUlong(PsGetCurrentThreadId());
    KeQuerySystemTimePrecise(&info->information.time);
    info->information.size = sizeof(RemoteThreadInfo);

    g_ThreadInfo.AddInfo(&info->link);
    KdPrint(("Remote thread added!\n"));

    return;
}

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING  symLink = RTL_CONSTANT_STRING(DRIVER_NAME);

    PsSetCreateProcessNotifyRoutineEx(OnCreateProcessNotify, TRUE);
    PsRemoveCreateThreadNotifyRoutine(OnCreateThreadNotify);

    g_ThreadInfo.Delete();
    g_Processes.Delete();
    g_ThreadInfoPool.Delete();
    g_ProcessesPool.Delete();

    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    return CompleteRequest(Irp);
}

NTSTATUS DriverRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS status = STATUS_SUCCESS;
    auto     irpSp = IoGetCurrentIrpStackLocation(Irp);
    SIZE_T   readLen = irpSp->Parameters.Read.Length;
    SIZE_T   bytesRead = 0;

    if (readLen == 0 || readLen % sizeof(RemoteThreadInfo)) {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto out;
    }

    auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, LowPagePriority);
    if (!buffer) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        err("MmGetSystemAddressForMdlSafe", status);
        goto out;
    }

    while (readLen > 0) {
        auto entry = g_ThreadInfo.RemoveInfo();
        if (entry == nullptr)
            break;

        auto linkedInfo = CONTAINING_RECORD(entry, LinkedInformation<RemoteThreadInfo>, link);
        auto &threadInfo = linkedInfo->information;

        RtlCopyMemory(buffer, &threadInfo, threadInfo.size);
        
        buffer = (PVOID)((ULONG_PTR)buffer + threadInfo.size);
        readLen -= threadInfo.size;
        bytesRead += threadInfo.size;

        g_ThreadInfoPool.Free(linkedInfo);
    }

out:
    return CompleteRequest(Irp, status, bytesRead);
}

// Object functions

void ThreadInfoHead::Init(void)
{
    InitializeListHead(&m_head);
    m_lock.Init();
}

void ThreadInfoHead::AddInfo(LIST_ENTRY *info)
{
    SingleLocker<FastMutex> locker(m_lock);
    InsertTailList(&m_head, info);
}

LIST_ENTRY *ThreadInfoHead::RemoveInfo(void)
{
    SingleLocker<FastMutex> locker(m_lock);
    auto info = RemoveHeadList(&m_head);
    if (info == &m_head)
        return nullptr;

    return info;
}

void ThreadInfoHead::Delete(void)
{
    ;
}

void ProcessesHead::Init(void)
{
    InitializeListHead(&m_head);
    m_lock.Init();
}

void ProcessesHead::Delete(void)
{
    m_lock.Delete();
}

void ProcessesHead::AddProcess(LIST_ENTRY *info)
{
    SingleLocker<ExecutiveResource> locker(m_lock);
    InsertHeadList(&m_head, info);
}

LIST_ENTRY *ProcessesHead::FindProcess(HANDLE pid)
{
    LIST_ENTRY *ret = nullptr;

    /*
     * We technically have a race bug here by returning a pointer from a shared search
     *   in the case where the process list gets huge enough so search is slow, and one process quickly
     *   created a thread and exits before the thread callback is serviced.
     * It will trigger both processnotify and threadnotify to free an entry, resulting in double free.
     */
    SharedLocker<ExecutiveResource> locker(m_lock);

    for (auto cur = m_head.Flink; cur != &m_head; cur = cur->Flink) {
        auto linkedInfo = CONTAINING_RECORD(cur, LinkedInformation<ProcessInfo>, link);
        auto &processInfo = linkedInfo->information;

        if (processInfo.pid == pid) {
            ret = cur;
            break;
        }
    }

    return ret;
}

void ProcessesHead::RemoveProcess(LIST_ENTRY *entry)
{
    SingleLocker<ExecutiveResource> locker(m_lock);
    RemoveEntryList(entry);
    auto linkedInfo = CONTAINING_RECORD(entry, LinkedInformation<ProcessInfo>, link);
    g_ProcessesPool.Free(linkedInfo);
}