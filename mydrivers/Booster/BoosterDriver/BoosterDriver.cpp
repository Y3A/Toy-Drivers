#include <ntifs.h>
#include <ntddk.h>
#include <TraceLoggingProvider.h>
#include <evntrace.h>

#include "BoosterDriver.h"
#include "BoosterCommon.h"

// {22772F30-7CB5-45EE-A9F6-6CFBE01D92A3}
TRACELOGGING_DEFINE_PROVIDER(g_Provider, "Booster", \
    (0x22772f30, 0x7cb5, 0x45ee, 0xa9, 0xf6, 0x6c, 0xfb, 0xe0, 0x1d, 0x92, 0xa3));

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS            status = STATUS_SUCCESS;
    UNICODE_STRING      DeviceName = { 0 }, SymbolicName = { 0 };
    PDEVICE_OBJECT      DeviceObject;

    TraceLoggingRegister(g_Provider);
    TraceLoggingWrite(g_Provider, "Trace Registered and Driver Loaded",
        TraceLoggingLevel(TRACE_LEVEL_INFORMATION));

    DriverObject->DriverUnload = BoosterUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = BoosterCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = BoosterCreateClose;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = BoosterWrite;

    RtlInitUnicodeString(&DeviceName, L"\\Device\\Booster");
    RtlInitUnicodeString(&SymbolicName, L"\\??\\Booster");
    
    status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to create device object (0x%08X)\n", status));
        TraceLoggingWrite(g_Provider, "IoCreateDevice Failure", TraceLoggingLevel(TRACE_LEVEL_CRITICAL),
            TraceLoggingUnicodeString(&DeviceName, "Device Name"), TraceLoggingNTStatus(status, "Return Status"));
        goto out;
    }
    TraceLoggingWrite(g_Provider, "IoCreateDevice Success", TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
        TraceLoggingUnicodeString(&DeviceName, "Device Name"), TraceLoggingNTStatus(status, "Return Status"));

    status = IoCreateSymbolicLink(&SymbolicName, &DeviceName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to create symbolic link (0x%08X)\n", status));
        TraceLoggingWrite(g_Provider, "IoCreateSymbolicLink Failure", TraceLoggingLevel(TRACE_LEVEL_CRITICAL),
            TraceLoggingUnicodeString(&SymbolicName, "Symbolic Name"), TraceLoggingNTStatus(status, "Return Status"));
        IoDeleteDevice(DeviceObject);
        goto out;
    }
    TraceLoggingWrite(g_Provider, "IoCreateSymbolicLink Success", TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
        TraceLoggingUnicodeString(&SymbolicName, "Symbolic Name"), TraceLoggingNTStatus(status, "Return Status"));

out:
    return status;
}

void BoosterUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING SymbolicName;

    SymbolicName = RTL_CONSTANT_STRING(L"\\??\\Booster");

    IoDeleteSymbolicLink(&SymbolicName);
    IoDeleteDevice(DriverObject->DeviceObject);

    TraceLoggingUnregister(g_Provider);

    return;
}

NTSTATUS BoosterCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS status = STATUS_SUCCESS;

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS BoosterWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS                status = STATUS_SUCCESS;
    PIO_STACK_LOCATION      StackLocation;
    PTHREAD_DATA            data;
    PETHREAD                TargetThread = NULL;
    KPRIORITY               OldPriority;
    DWORD                   information = 0;


    StackLocation = IoGetCurrentIrpStackLocation(Irp);

    if (StackLocation->Parameters.Write.Length < sizeof(THREAD_DATA)) {
        status = STATUS_BUFFER_TOO_SMALL;
        goto out;
    }

    data = (PTHREAD_DATA)Irp->UserBuffer;
    if (!data || data->Priority < 1 || data->Priority > 31) {
        status = STATUS_INVALID_PARAMETER;
        goto out;
    }

    information = sizeof(data);

    status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &TargetThread);
    if (!NT_SUCCESS(status) || !TargetThread) {
        status = STATUS_INVALID_PARAMETER;
        goto out;
    }

    OldPriority = KeSetPriorityThread((PKTHREAD)TargetThread, (KPRIORITY)data->Priority);
    KdPrint(("Priority change for thread %u from %d to %d succeeded!\n",
        data->ThreadId, OldPriority, data->Priority));

    TraceLoggingWrite(g_Provider, "Priority Change Succeeded",
        TraceLoggingLevel(TRACE_LEVEL_INFORMATION), TraceLoggingInt32(data->ThreadId, "Thread ID"),
        TraceLoggingInt32(data->Priority, "New Priority"), TraceLoggingInt32(OldPriority, "Old Priority"));

    ObDereferenceObject(TargetThread);

out:
    Irp->IoStatus.Information = information;
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}