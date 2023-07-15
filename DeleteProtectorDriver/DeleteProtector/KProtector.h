#pragma once

#define DEVICE_REALNAME L"\\Device\\DeleteProtector"

typedef ULONG FLT_FILTER_UNLOAD_FLAGS;

NTSTATUS DeleteProtectorCbUnload(FLT_FILTER_UNLOAD_FLAGS Flags);
NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0);