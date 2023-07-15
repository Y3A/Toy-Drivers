#include <fltKernel.h>
#include <ntddk.h>

#include "DeleteProctectorPublic.h"
#include "ExtensionList.h"
#include "KCommon.h"
#include "KProtector.h"
#include "MiniFilter.h"

ExtensionList   *g_ExtensionList;
PFLT_FILTER     g_Filter;
PDRIVER_OBJECT  g_DriverObject;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS        status = STATUS_SUCCESS;
    PDEVICE_OBJECT  deviceObj = nullptr;
    UNICODE_STRING  deviceName = RTL_CONSTANT_STRING(DEVICE_REALNAME);
    UNICODE_STRING  deviceSymLink = RTL_CONSTANT_STRING(DEVICE_NAME);
    bool            symLinkCreated = false;

    log("In DriverEntry");

    g_DriverObject = DriverObject;

    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObj);
    if (!NT_SUCCESS(status)) {
        err("IoCreateDevice fail.", status);
        goto out;
    }

    status = IoCreateSymbolicLink(&deviceSymLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        err("IoCreateSymbolicLink fail.", status);
        goto out;
    }
    symLinkCreated = true;

    status = InitializeExtensionList(&g_ExtensionList);
    if (!NT_SUCCESS(status)) {
        err("InitializeExtensionList fail.", status);
        goto out;
    }

    status = MiniFilterInitRegistry(RegistryPath);
    if (!NT_SUCCESS(status)) {
        err("MiniFilterInitRegistry fail.", status);
        goto out;
    }

    status = MiniFilterRegisterFilter(DriverObject, &g_Filter, DeleteProtectorCbUnload);
    if (!NT_SUCCESS(status)) {
        err("MiniFilterRegisterFilter fail.", status);
        goto out;
    }

    status = FltStartFiltering(g_Filter);
    if (!NT_SUCCESS(status)) {
        err("FltStartFiltering fail.", status);
        goto out;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = \
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;

    log("All Good.");

out:
    if (!NT_SUCCESS(status)) {
        if (deviceObj)
            IoDeleteDevice(deviceObj);
        if (symLinkCreated)
            IoDeleteSymbolicLink(&deviceSymLink);
        if (g_ExtensionList)
            DestroyExtensionList(&g_ExtensionList);
        if (g_Filter)
            FltUnregisterFilter(g_Filter);
    }

    return status;
}

NTSTATUS DeleteProtectorCbUnload(FLT_FILTER_UNLOAD_FLAGS Flags)
{
    log("In DeleteProtectorCbUnload");
    UNREFERENCED_PARAMETER(Flags);

    UNICODE_STRING  deviceSymLink = RTL_CONSTANT_STRING(DEVICE_NAME);

    FltUnregisterFilter(g_Filter);

    DestroyExtensionList(&g_ExtensionList);

    IoDeleteSymbolicLink(&deviceSymLink);
    IoDeleteDevice(g_DriverObject->DeviceObject);

    log("Delete All Good.");

    return STATUS_SUCCESS;
}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    return CompleteRequest(Irp);
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR information)
{
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS            status = STATUS_INVALID_DEVICE_REQUEST;
    auto                irpSp = IoGetCurrentIrpStackLocation(Irp);
    auto                &dic = irpSp->Parameters.DeviceIoControl;
    WCHAR               *userBuffer = nullptr;
    ULONG               inputLen = dic.InputBufferLength;
    ULONG               operatedLen = 0;
    UNICODE_STRING      extension = { 0 };

    log("In DeviceIoControl");

    switch (dic.IoControlCode) {
        case IOCTL_KPROTECT_ADD_EXT:
            log("In Add Ext");
            userBuffer = (WCHAR *)Irp->AssociatedIrp.SystemBuffer;
            KdPrint(("%S\n", userBuffer));
            if (userBuffer == nullptr || inputLen < sizeof(WCHAR) * 2 || \
                    userBuffer[inputLen / sizeof(WCHAR) - 1] != 0) {
                status = STATUS_INVALID_PARAMETER;
                err("IOCTL_KPROTECT_ADD_EXT", status);
                break;
            }
            RtlInitUnicodeString(&extension, userBuffer);
            status = AddExtension(g_ExtensionList, &extension);
            if (!NT_SUCCESS(status))
                err("AddExtension fail", status);
            operatedLen = inputLen;
            break;

        case IOCTL_KPROTECT_REMOVE_EXT:
            userBuffer = (WCHAR *)Irp->AssociatedIrp.SystemBuffer;
            if (userBuffer == nullptr || inputLen < sizeof(WCHAR) * 2 || \
                userBuffer[inputLen / sizeof(WCHAR) - 1] != 0) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            RtlInitUnicodeString(&extension, userBuffer);
            DeleteExtension(g_ExtensionList, &extension);
            operatedLen = inputLen;
            status = STATUS_SUCCESS;
            break;

        case IOCTL_KPROTECT_REMOVE_ALL:
            DeleteAllExtensions(g_ExtensionList);
            status = STATUS_SUCCESS;
            break;
    }

    return CompleteRequest(Irp, status, operatedLen);
}