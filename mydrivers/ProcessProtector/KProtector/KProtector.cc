#include "KProtector.h"

ProcessesHead g_Processes;
PVOID         g_RegistrationHandle;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS        status = STATUS_SUCCESS;
    PDEVICE_OBJECT  devObj = nullptr;
    UNICODE_STRING  devName = RTL_CONSTANT_STRING(DRIVER_DEVNAME);
    UNICODE_STRING  symLink = RTL_CONSTANT_STRING(DRIVER_NAME);
    bool            symLinkCreated = false;
    bool            processCallbackSet = false;
    bool            processesHeadInitialized = false;
    bool            operationCallbackSet = false;
    OB_OPERATION_REGISTRATION operation = {
        PsProcessType,
        OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
        OnAcquireHandleNotify, nullptr
    };

    OB_CALLBACK_REGISTRATION reg = {
        OB_FLT_REGISTRATION_VERSION,
        1,
        RTL_CONSTANT_STRING(L"1337.3771802"),
        nullptr,
        &operation
    };

    status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);
    if (!NT_SUCCESS(status)) {
        err("IoCreateDevice", status);
        goto out;
    }

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

    status = g_Processes.Init();
    if (!NT_SUCCESS(status)) {
        err("g_Processes.Init()", status);
        goto out;
    }
    processesHeadInitialized = true;

    status = ObRegisterCallbacks(&reg, &g_RegistrationHandle);
    if (!NT_SUCCESS(status)) {
        err("ObRegisterCallbacks", status);
        goto out;
    }
    operationCallbackSet = true;

    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = \
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;

out:
    if (!NT_SUCCESS(status)) {
        if (devObj)
            IoDeleteDevice(devObj);
        if (symLinkCreated)
            IoDeleteSymbolicLink(&symLink);
        if (processCallbackSet)
            PsSetCreateProcessNotifyRoutineEx(OnCreateProcessNotify, TRUE);
        if (processesHeadInitialized)
            g_Processes.Delete();
        if (operationCallbackSet)
            ObUnRegisterCallbacks(g_RegistrationHandle);
    }

    return status;
}

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING  symLink = RTL_CONSTANT_STRING(DRIVER_NAME);

    PsSetCreateProcessNotifyRoutineEx(OnCreateProcessNotify, TRUE);
    ObUnRegisterCallbacks(g_RegistrationHandle);
    g_Processes.Delete();
    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(DriverObject->DeviceObject);

    return;
}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    return CompleteRequest(Irp);
}

NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS  status = STATUS_SUCCESS;
    auto      irpSp = IoGetCurrentIrpStackLocation(Irp);
    auto      &dic = irpSp->Parameters.DeviceIoControl;
    ULONG     info = sizeof(ULONG), pid = 0;
    ULONG     inputLen = dic.InputBufferLength;

    switch (dic.IoControlCode)
    {
        case IOCTL_KPROTECT_REMOVE_ALL:
            info = 0;
            g_Processes.RemoveAll();
            break;

        case IOCTL_KPROTECT_ADD_PID:
            if (inputLen != sizeof(ULONG) || Irp->AssociatedIrp.SystemBuffer == nullptr) {
                status = STATUS_INVALID_BUFFER_SIZE;
                info = 0;
                break;
            }
            pid = DEREF_PTR((ULONG *)Irp->AssociatedIrp.SystemBuffer);
            if (!g_Processes.AddProcess(pid)) {
                status = STATUS_ALREADY_REGISTERED;
                info = 0;
            }
            break;

        case IOCTL_KPROTECT_REMOVE_PID:
            if (inputLen != sizeof(ULONG) || Irp->AssociatedIrp.SystemBuffer == nullptr) {
                status = STATUS_INVALID_BUFFER_SIZE;
                info = 0;
                break;
            }
            pid = DEREF_PTR((ULONG *)Irp->AssociatedIrp.SystemBuffer);
            g_Processes.FindAndRemoveProcess(pid);
            break;
    }

    return CompleteRequest(Irp, status, info);
}

static void OnCreateProcessNotify(PEPROCESS Process, HANDLE Pid, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    UNREFERENCED_PARAMETER(Process);

    if (CreateInfo)
        return;

    // Process Termination
    ULONG pid = HandleToUlong(Pid);

    if (g_Processes.ProcessExists(pid))
        g_Processes.FindAndRemoveProcess(pid);

    return;
}

static OB_PREOP_CALLBACK_STATUS OnAcquireHandleNotify(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation)
{

    UNREFERENCED_PARAMETER(RegistrationContext);

    PEPROCESS   proc;
    ULONG       pid;

    if (OperationInformation->KernelHandle)
        return OB_PREOP_SUCCESS;

    proc = (PEPROCESS)OperationInformation->Object;
    pid = HandleToUlong(PsGetProcessId(proc));

    if (g_Processes.ProcessExists(pid))
        OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;

    return OB_PREOP_SUCCESS;
}

static NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info)
{
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

// Auxiliary Struct Methods

NTSTATUS ProcessesHead::Init(void)
{
    InitializeListHead(&m_head);
    m_lock.Init();
    return m_pool.Init(PagedPool, DRIVER_TAG);
}

void ProcessesHead::Delete(void)
{
    m_pool.Delete();
    m_lock.Delete();
}

LIST_ENTRY *ProcessesHead::InternalFindProcessCallerLocked(ULONG pid)
{
    LIST_ENTRY *ret = nullptr;

    for (auto cur = m_head.Flink; cur != &m_head; cur = cur->Flink) {
        auto linkedInfo = CONTAINING_RECORD(cur, ProcessItem, link);

        if (linkedInfo->pid == pid) {
            ret = cur;
            break;
        }
    }

    return ret;
}

bool ProcessesHead::ProcessExists(ULONG pid)
{
    SharedLocker<ExecutiveResource> Locker(m_lock);
    return static_cast<bool>(InternalFindProcessCallerLocked(pid));
}

bool ProcessesHead::AddProcess(ULONG pid)
{
    SingleLocker<ExecutiveResource> Locker(m_lock);

    if (InternalFindProcessCallerLocked(pid))
        return false;

    auto item = m_pool.Alloc();
    item->pid = pid;
    InsertHeadList(&m_head, &item->link);

    return true;
}

void ProcessesHead::FindAndRemoveProcess(ULONG pid)
{
    SingleLocker<ExecutiveResource> Locker(m_lock);

    LIST_ENTRY *itemLink = InternalFindProcessCallerLocked(pid);
    if (!itemLink)
        return;

    auto linkedInfo = CONTAINING_RECORD(itemLink, ProcessItem, link);

    RemoveEntryList(itemLink);
    m_pool.Free(linkedInfo);

    return;
}

void ProcessesHead::RemoveAll(void)
{
    SingleLocker<ExecutiveResource> Locker(m_lock);

    for (auto cur = m_head.Flink, next = cur->Flink; cur != &m_head; cur = next, next = next->Flink) {
        auto linkedInfo = CONTAINING_RECORD(cur, ProcessItem, link);
        RemoveEntryList(cur);
        m_pool.Free(linkedInfo);
    }

    return;
}