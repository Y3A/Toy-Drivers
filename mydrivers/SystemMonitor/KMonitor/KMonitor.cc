#include <ntifs.h>
#include <ntddk.h>
#include <fltKernel.h>

#include "FastMutex.h"
#include "KMonitor.h"
#include "Locker.h"
#include "SystemMonitorPublic.h"


#define err(msg, status) KdPrint((SYSMON_PREFIX "Error %s : (0x%08X)\n", msg, status))

InformationHead g_Head;

// Driver functions

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS        status = STATUS_SUCCESS;
    PDEVICE_OBJECT  deviceObj = nullptr;
    UNICODE_STRING  symLink = RTL_CONSTANT_STRING(SYSMON_NAME);
    UNICODE_STRING  devName = RTL_CONSTANT_STRING(SYSMON_DEVNAME);
    bool            symLinkCreated = false;
    bool            processCallbackSet = false;
    bool            threadCallbackSet = false;

    // Create as exclusive device so only one client can read data
    status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObj);
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

    status = PsSetLoadImageNotifyRoutine(OnLoadImageNotify);
    if (!NT_SUCCESS(status)) {
        err("PsSetLoadImageNotifyRoutine", status);
        goto out;
    }

    g_Head.Init(SYSMON_MAX);

    DriverObject->DriverUnload = SysMonUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = \
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = SysMonCreateClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = SysMonRead;

out:
    if (!NT_SUCCESS(status)) {
        if (symLinkCreated)
            IoDeleteSymbolicLink(&symLink);
        if (deviceObj)
            IoDeleteDevice(deviceObj);
        if (processCallbackSet)
            PsSetCreateProcessNotifyRoutineEx(OnCreateProcessNotify, TRUE);
        if (threadCallbackSet)
            PsRemoveCreateThreadNotifyRoutine(OnCreateThreadNotify);
    }

    return status;
}

static void OnCreateThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create)
{
    if (Create) {
        // thread create
        auto linkedInfo = (LinkedInformation<ThreadCreateInfo> *)ExAllocatePool2(POOL_FLAG_PAGED, \
            sizeof(LinkedInformation<ThreadCreateInfo>), SYSMON_TAG);
        if (linkedInfo == nullptr) {
            err("ExAllocatePool2 failed allocation", 0xdeadbeef);
            goto out;
        }

        ThreadCreateInfo &info = linkedInfo->information;
        KeQuerySystemTimePrecise(&info.time);
        info.pid = HandleToUlong(ProcessId);
        info.size = sizeof(ThreadCreateInfo);
        info.type = InfoType::ThreadCreate;
        info.tid = HandleToUlong(ThreadId);

        g_Head.AddItem(&linkedInfo->link);

        goto out;
    }

    // thread exit
    auto linkedInfo = (LinkedInformation<ThreadExitInfo> *)ExAllocatePool2(POOL_FLAG_PAGED, \
        sizeof(LinkedInformation<ThreadExitInfo>), SYSMON_TAG);
    if (linkedInfo == nullptr) {
        err("ExAllocatePool2 failed allocation", 0xdeadbeef);
        goto out;
    }

    ThreadExitInfo &info = linkedInfo->information;
    KeQuerySystemTimePrecise(&info.time);
    info.pid = HandleToUlong(ProcessId);
    info.size = sizeof(ThreadExitInfo);
    info.type = InfoType::ThreadExit;
    info.tid = HandleToUlong(ThreadId);

    PETHREAD thread;

    if (NT_SUCCESS(PsLookupThreadByThreadId(ThreadId, &thread))) {
        info.exitCode = PsGetThreadExitStatus(thread);
        ObDereferenceObject(thread);
    }

    g_Head.AddItem(&linkedInfo->link);

out:
    return;
}

static void OnCreateProcessNotify(PEPROCESS Process, HANDLE Pid, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    if (CreateInfo) {
        // process create
        SIZE_T commandLineSize = 0;
        USHORT allocSize = sizeof(LinkedInformation<ProcessCreateInfo>);

        if (CreateInfo->CommandLine) {
            commandLineSize = CreateInfo->CommandLine->Length;
            // USHORT overflow leading to heap overflow below, hax me plz
            allocSize += CreateInfo->CommandLine->MaximumLength;
        }

        auto linkedInfo = (LinkedInformation<ProcessCreateInfo> *)ExAllocatePool2(POOL_FLAG_PAGED, \
            allocSize, SYSMON_TAG);
        if (linkedInfo == nullptr) {
            err("ExAllocatePool2 failed allocation", 0xdeadbeef);
            goto out;
        }

        ProcessCreateInfo &info = linkedInfo->information;
        KeQuerySystemTimePrecise(&info.time);
        info.pid = HandleToUlong(Pid);
        info.size = (ULONG)allocSize;
        info.type = InfoType::ProcessCreate;
        info.ppid = HandleToUlong(CreateInfo->ParentProcessId);
        info.creatingThreadId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueThread);

        info.commandLineLen = (USHORT)(commandLineSize / sizeof(WCHAR));

        if (commandLineSize)
            RtlCopyMemory(info.commandLine, CreateInfo->CommandLine->Buffer, commandLineSize);

        g_Head.AddItem(&linkedInfo->link);

        goto out;
    }

    // process exit
    auto linkedInfo = (LinkedInformation<ProcessExitInfo> *)ExAllocatePool2(POOL_FLAG_PAGED, \
        sizeof(LinkedInformation<ProcessExitInfo>), SYSMON_TAG);
    if (linkedInfo == nullptr) {
        err("ExAllocatePool2 failed allocation", 0xdeadbeef);
        goto out;
    }

    ProcessExitInfo &info = linkedInfo->information;
    KeQuerySystemTimePrecise(&info.time);
    info.pid = HandleToUlong(Pid);
    info.size = sizeof(ProcessExitInfo);
    info.type = InfoType::ProcessExit;
    info.exitCode = PsGetProcessExitStatus(Process);

    g_Head.AddItem(&linkedInfo->link);

out:
    return;
}

static void OnLoadImageNotify(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo)
{
    LinkedInformation<ImageLoadInfo> *linkedInfo = nullptr;
    SIZE_T                            allocSize = sizeof(LinkedInformation<ImageLoadInfo>);
    bool                              nameAcquired = false;

    if (ImageInfo->ExtendedInfoPresent) {
        auto exInfo = CONTAINING_RECORD(ImageInfo, IMAGE_INFO_EX, ImageInfo);
        PFLT_FILE_NAME_INFORMATION fltInfo;
        if (NT_SUCCESS(FltGetFileNameInformationUnsafe(exInfo->FileObject, nullptr, \
            FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &fltInfo))) {
            allocSize += fltInfo->Name.MaximumLength;

            linkedInfo = (LinkedInformation<ImageLoadInfo> *)ExAllocatePool2(POOL_FLAG_PAGED, \
                allocSize, SYSMON_TAG);
            if (!linkedInfo) {
                err("ExAllocatePool2 out of memory", 0xdeadbeef);
            }

            if (linkedInfo) {
                auto &loadInfo = linkedInfo->information;
                RtlCopyMemory(loadInfo.imageName, fltInfo->Name.Buffer, fltInfo->Name.Length);
                loadInfo.imageNameLen = fltInfo->Name.Length / sizeof(WCHAR);
                nameAcquired = true;
            }

            FltReleaseFileNameInformation(fltInfo);
        }
    }

    if (!nameAcquired && FullImageName) {

        allocSize = sizeof(LinkedInformation<ImageLoadInfo>);
        allocSize += FullImageName->MaximumLength;

        linkedInfo = (LinkedInformation<ImageLoadInfo> *)ExAllocatePool2(POOL_FLAG_PAGED, \
            allocSize, SYSMON_TAG);
        if (!linkedInfo) {
            err("ExAllocatePool2 out of memory", 0xdeadbeef);
        }

        if (linkedInfo) {
            auto &loadInfo = linkedInfo->information;
            RtlCopyMemory(loadInfo.imageName, FullImageName->Buffer, FullImageName->Length);
            loadInfo.imageNameLen = FullImageName->Length / sizeof(WCHAR);
            nameAcquired = true;
        }
    }

    if (!nameAcquired) {
        allocSize = sizeof(LinkedInformation<ImageLoadInfo>);
        linkedInfo = (LinkedInformation<ImageLoadInfo> *)ExAllocatePool2(POOL_FLAG_PAGED, \
            allocSize, SYSMON_TAG);
        if (!linkedInfo) {
            err("ExAllocatePool2 out of memory", 0xdeadbeef);
            return;
        }

        auto &loadInfo = linkedInfo->information;
        loadInfo.imageNameLen = 0;
    }

    auto &info = linkedInfo->information;
    KeQuerySystemTimePrecise(&info.time);
    info.pid = HandleToUlong(ProcessId);
    info.size = (USHORT)allocSize;
    info.type = InfoType::ImageLoad;
    info.loadAddress = (ULONG_PTR)ImageInfo->ImageBase;
    info.imageSize = (ULONG)ImageInfo->ImageSize;

    g_Head.AddItem(&linkedInfo->link);

    return;
}

NTSTATUS SysMonRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS            status = STATUS_SUCCESS;
    auto                irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG               readLength = irpSp->Parameters.Read.Length;
    ULONG               bytes = 0;
    PVOID               buffer = nullptr;

    buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, LowPagePriority);
    if (!buffer) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        err("MmGetSystemAddressForMdlSafe", status);
        goto out;
    }

    while (true) {
        auto entry = g_Head.RemoveItem();
        if (entry == nullptr)
            goto out;

        auto &header = ((LinkedInformation<InfoHeader> *)entry)->information;

        if (readLength < header.size) {
            // buffer too small, throw back into list
            g_Head.ReturnItem(entry);
            goto out;
        }

        RtlCopyMemory(buffer, &header, header.size);
        readLength -= header.size;
        buffer = (PVOID)((ULONG_PTR)buffer + (ULONG_PTR)(header.size));
        bytes += header.size;

        ExFreePool(entry);
    }

out:
    return CompleteRequest(Irp, status, bytes);
}

NTSTATUS SysMonCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    return CompleteRequest(Irp);
}

void SysMonUnload(PDRIVER_OBJECT DriverObject)
{
    LIST_ENTRY *entry;
    UNICODE_STRING  symLink = RTL_CONSTANT_STRING(SYSMON_NAME);

    PsSetCreateProcessNotifyRoutineEx(OnCreateProcessNotify, TRUE);
    PsRemoveCreateThreadNotifyRoutine(OnCreateThreadNotify);
    PsRemoveLoadImageNotifyRoutine(OnLoadImageNotify);

    while ((entry = g_Head.RemoveItem()) != nullptr)
        ExFreePool(entry);

    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(DriverObject->DeviceObject);

    return;
}

static NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info)
{
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// InformationHead member functions

void InformationHead::Init(ULONG maxCount)
{
    InitializeListHead(&m_link);
    m_lock.Init();
    m_count = 0;
    m_maxCount = maxCount;
}

void InformationHead::AddItem(LIST_ENTRY *information)
{
    Locker<FastMutex> locker(m_lock);
    if (m_count >= m_maxCount) {
        auto head = RemoveHeadList(&m_link);
        ExFreePool(head);
        m_count--;
    }

    InsertTailList(&m_link, information);
    m_count++;

    return;
}

void InformationHead::ReturnItem(LIST_ENTRY *information)
{
    Locker<FastMutex> locker(m_lock);

    InsertHeadList(&m_link, information);
    m_count++;

    return;
}

LIST_ENTRY *InformationHead::RemoveItem(void)
{
    Locker<FastMutex> locker(m_lock);
    LIST_ENTRY *item = RemoveHeadList(&m_link);

    if (item == &m_link)
        return nullptr;

    m_count--;
    return item;
}
