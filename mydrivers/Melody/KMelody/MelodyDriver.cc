#include <ntifs.h>
#include <ntddk.h>

#include "MelodyPublic.h"
#include "Memory.h"
#include "PlaybackState.h"

#define err(msg, status) KdPrint((MELODY_PREFIX "Error %s : (0x%08X)\n", msg, status))

void     MelodyUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS MelodyCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS MelodyDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info);

PlaybackState *g_State;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS            status = STATUS_SUCCESS;
    char                *errmsg = nullptr;
    PDEVICE_OBJECT      DeviceObject = nullptr;
    UNICODE_STRING      symLink = RTL_CONSTANT_STRING(L"\\??\\KMelody");
    UNICODE_STRING      name = RTL_CONSTANT_STRING(L"\\Device\\KMelody");

    g_State = new (POOL_FLAG_PAGED) PlaybackState;
    if (g_State == nullptr) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        errmsg = "ExAllocatePool2";
        goto out;
    }

    status = IoCreateDevice(DriverObject, 0, &name, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
    if (!NT_SUCCESS(status)) {
        errmsg = "IoCreateDevice";
        goto out;
    }

    status = IoCreateSymbolicLink(&symLink, &name);
    if (!NT_SUCCESS(status)) {
        errmsg = "IoCreateSymbolicLink";
        goto out;
    }

    DriverObject->DriverUnload = MelodyUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] \
        = MelodyCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MelodyDeviceControl;

out:
    if (!NT_SUCCESS(status)) {
        err(errmsg, status);
    }

    return status;
}

void MelodyUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\KMelody");

    delete g_State;
    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(DriverObject->DeviceObject);

    return;
}

NTSTATUS MelodyCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS    status = STATUS_SUCCESS;
    ULONG_PTR   info = 0;

    if (IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CREATE)
        status = g_State->PlaybackStart(DeviceObject, Irp);

    return CompleteRequest(Irp, status, info);
}

NTSTATUS MelodyDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS    status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR   info = 0;
    Note        *data = nullptr;

    auto irpSp = IoGetCurrentIrpStackLocation(Irp);
    auto &ioCtl = irpSp->Parameters.DeviceIoControl;

    switch (ioCtl.IoControlCode)
    {
        case MELODY_IOCTL_PLAY:
            if (ioCtl.InputBufferLength == 0 || ioCtl.InputBufferLength % sizeof(Note)) {
                status = STATUS_INVALID_BUFFER_SIZE;
                break;
            }

            if ((data = (Note *)Irp->AssociatedIrp.SystemBuffer) == nullptr) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = g_State->AddNotes(data, ioCtl.InputBufferLength / sizeof(Note));
            if (NT_SUCCESS(status))
                info = ioCtl.InputBufferLength;

            break;
    }

    return CompleteRequest(Irp, status, info);
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info)
{
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}