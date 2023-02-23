#include <ntddk.h>

#include "ZeroDriver.h"
#include "ZeroCommon.h"

#define DRV_DBGPREFIX "Zero: "
#define DRV_DEVNAME  L"\\Device\\Zero"
#define DRV_SYMNAME  L"\\??\\Zero"
#define DRV_TAG      'oreZ'

FastMutex      g_Mutex;
DWORD          g_TotalRead, g_TotalWrite;
UNICODE_STRING g_RegistryPath;

extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    UNICODE_STRING                  DeviceName = {0}, SymbolicName = {0};
    PDEVICE_OBJECT                  DeviceObject;
    NTSTATUS                        status = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES               RegKeyAttributes = {0};
    HANDLE                          hKey = nullptr;
    DWORD                           PreviousTotalRead = 0; PreviousTotalWrite = 0;
    UNICODE_STRING                  RegReadName = RTL_CONSTANT_STRING(L"TotalRead");
    UNICODE_STRING                  RegWriteName = RTL_CONSTANT_STRING(L"TotalWrite");
    PKEY_VALUE_PARTIAL_INFORMATION  ReadWriteStats = nullptr;
    ULONG                           length = 0;

    // Initialize routines
    DriverObject->DriverUnload = ZeroUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = ZeroCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = ZeroCreateClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = ZeroRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ZeroDeviceControl;
    
    // Initialize driver name, make a copy of registry path
    RtlInitUnicodeString(&DeviceName, DRV_DEVNAME);
    RtlInitUnicodeString(&SymbolicName, DRV_SYMNAME);
    
    g_RegistryPath.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, RegistryPath->Length, DRV_TAG);
    if (g_RegistryPath.Buffer == nullptr) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        KdPrint((DBG_PREFIX "Failed to allocate memory (0x%08X)\n", status));
        goto out;
    }

    g_RegistryPath.MaximumLength = RegistryPath->Length;
    status = RtlCopyUnicodeString(&g_RegistryPath, (PCUNICODE_STRING)RegistryPath);
    if (!NT_SUCCESS(status)) {
        KdPrint((DBG_PREFIX "Failed to copy RegistryPath (0x%08X)\n", status));
        goto out;
    }

    // Initialize registry information buffer
    ReadWriteStats = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(
        PagedPool,
        sizeof(DWORD) + sizeof(KEY_VALUE_PARTIAL_INFORMATION),
        DRV_TAG
    );
    if (ReadWriteStats == nullptr) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        KdPrint((DBG_PREFIX "Failed to allocate memory (0x%08X)\n", status));
        goto out;
    }

    // Create device and symlink, enable direct IO
    status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN 0, FALSE, &DeviceObject);
    if (!NT_SUCCESS(status)) {
        KdPrint((DBG_PREFIX "Failed to create device object (0x%08X)\n", status));
        goto out;
    }

    DeviceObject->Flags |= DO_DIRECT_IO;

    status = IoCreateSymbolicLink(&SymbolicName, &DeviceName);
    if (!NT_SUCCESS(status)) {
        KdPrint((DBG_PREFIX "Failed to create symbolic link (0x%08X)\n", status));
        goto out;
    }

    // Initialize mutex
    g_Mutex.Init();

    // Read in previous TotalRead and TotalWrite from registry
    RegKeyAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(g_RegistryPath, OBJ_KERNEL_HANDLE);
    status = ZwOpenKey(&hKey, KEY_ALL_ACCESS, RegKeyAttributes);
    if (!NT_SUCCESS(status)) {
        KdPrint((DBG_PREFIX "Failed to open registry key (0x%08X)\n", status));
        goto out;
    }

    status = GetPreviousReadWriteValue(
        hkey,
        &RegReadName,
        ReadWriteStats,
        sizeof(DWORD) + sizeof(KEY_VALUE_PARTIAL_INFORMATION),
        &PreviousTotalRead
    );
    if (!NT_SUCCESS(status))
        goto out;
    
    status = GetPreviousReadWriteValue(
        hkey,
        &RegWriteName,
        ReadWriteStats,
        sizeof(DWORD) + sizeof(KEY_VALUE_PARTIAL_INFORMATION),
        &PreviousTotalWrite
    );
    if (!NT_SUCCESS(status))
        goto out;

    // Update previous stats
    UpdateStats(PreviousTotalRead, PreviousTotalWrite);

out:
    if (!NT_SUCCESS(status)) {
        if (DeviceObject)
            IoDeleteDevice(DeviceObject);

         if (g_RegistryPath.Buffer)
            ExFreePool(g_RegistryPath.Buffer);
    }

    if (hKey)
        ZwClose(hKey);

    if (ReadWriteStats)
        ExFreePool(ReadWriteStats);

    return status;
}

NTSTATUS GetPreviousReadWriteValue(_In_ HANDLE hKey, _In_ PUNICODE_STRING name, _Inout_ PKEY_VALUE_PARTIAL_INFORMATION RegistryData, _In_ ULONG length, _Out_ PDWORD ReturnData)
{
    NTSTATUS status;
    ULONG    OutLength;

    RtlZeroMemory(RegistryData, length);

    status = ZwQueryValueKey(
        hKey,
        name,
        KeyValuePartialInformation,
        RegistryData,
        length,
        &OutLength;
    );
    if (!NT_SUCCESS(status) && status != STATUS_OBJECT_NAME_NOT_FOUND) {
        // If name not found that's fine, means no previous total read/writes
        // Other statuses are probably fatal
        KdPrint((DBG_PREFIX "Failed to read registry value (0x%08X)\n", status));
        goto out;
    }

    *ReturnData = *(PDWORD)(RegistryData.Data);

out:
    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
        status = STATUS_SUCCESS;

    return status;
}

NTSTATUS CompleteIrp(_Inout_ PIRP Irp, _In_ NTSTATUS status=STATUS_SUCCESS, _In_ ULONG_PTR info=0)
{
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS ZeroCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    return CompleteIrp(Irp);
}

NTSTATUS ZeroRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    auto  StackLocation = IoGetCurrentStackLocation(Irp);
    DWORD length = StackLocation->Parameters.Read.Length;
    if (length == 0)
        return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE);

    NT_ASSERT(Irp->MdlAddress);

    auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, LowPagePriority);
    if (!buffer)
        return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES);

    RtlZeroMemory(buffer, length);

    UpdateStats(length, 0);

    return CompleteIrp(Irp, STATUS_SUCCESS, length);
}

NTSTATUS ZeroWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    auto  StackLocation = IoGetCurrentStackLocation(Irp);
    DWORD length = StackLocation->Parameters.Write.Length;

    UpdateStats(0, length);

    return CompleteIrp(Irp, STATUS_SUCCESS, length);
}

void ZeroUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING SymbolicName = RTL_CONSTANT_STRING(DRV_SYMNAME);
    HANDLE         hKey;
    ZEROSTATS      stats;

    // Save total values to registry
    RegKeyAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(g_RegistryPath, OBJ_KERNEL_HANDLE);
    status = ZwOpenKey(&hKey, KEY_ALL_ACCESS, RegKeyAttributes);
    if (!NT_SUCCESS(status)) {
        KdPrint((DBG_PREFIX "Failed to open registry key (0x%08X)\n", status));
        goto out;
    }

    ZeroGetStats(&stats);

    // Save read
    status = ZwSetValueKey(
        &hKey,
        &SymbolicName,
        0,
        REG_DWORD,
        &stats.TotalRead,
        sizeof(DWORD)
    );
    if (!NT_SUCCESS(status))
        KdPrint((DBG_PREFIX "Failed to save total read value (0x%08X)\n", status));

    // Save write
    status = ZwSetValueKey(
        &hKey,
        &SymbolicName,
        0,
        REG_DWORD,
        &stats.TotalWrite,
        sizeof(DWORD)
    );
    if (!NT_SUCCESS(status))
        KdPrint((DBG_PREFIX "Failed to save total write value (0x%08X)\n", status));

out:
    if (hKey)
        ZwClose(hKey);
    IoDeleteSymbolicLink(&SymbolicName);
    IoDeleteDevice(DriverObject->DeviceObject);
    return;
}

NTSTATUS ZeroDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    auto       StackLocation = IoGetCurrentStackLocation(Irp);
    auto&      DeviceIoControl = StackLocation->Parameters.DeviceIoControl;

    ULONG64    length = 0;
    NTSTATUS   status = STATUS_INVALID_DEVICE_REQUEST;

    switch (DeviceIoControl.IoControlCode) {
        case IOCTL_ZERO_GET_STATS:
        {
            if (DeviceIoControl.OutputBufferLength < sizeof(ZEROSTATS)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            auto stats = (ZEROSTATS *)Irp->AssociatedIrp.SystemBuffer;
            if (stats == nullptr) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = ZeroGetStats(stats);
            if (NT_SUCCESS(status))
                length = sizeof(ZEROSTATS);
            break;
        }
        case IOCTL_ZERO_CLEAR_STATS:
        {
            status = ZeroClearStats();
            break;
        }
    }

    return CompleteIrp(Irp, status, length);
}

NTSTATUS ZeroGetStats(_Out_ ZEROSTATS *stats)
{
    Locker<FastMutex> locker(g_Mutex);
    stats->TotalRead = g_TotalRead;
    stats->TotalWrite = g_TotalWrite;

    return STATUS_SUCCESS;
}

NTSTATUS ZeroClearStats(void)
{
    Locker<FastMutex> locker(g_Mutex);
    g_TotalRead = 0;
    g_TotalWrite = 0;

    return STATUS_SUCCESS;
}

void UpdateStats(_In_ DWORD AddRead, _In_ DWORD AddWrite)
{
    Locker<FastMutex> locker(g_Mutex);
    g_TotalRead += AddRead;
    g_TotalWrite += AddWrite;

    return;
}