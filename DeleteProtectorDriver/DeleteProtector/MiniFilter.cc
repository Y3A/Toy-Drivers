#include <fltKernel.h>
#include <ntddk.h>

#include "ExtensionList.h"
#include "KCommon.h"
#include "MiniFilter.h"

NTSTATUS MiniFilterInitRegistry(PUNICODE_STRING RegistryPath)
{
    NTSTATUS            status = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES   driverRegistryBase = RTL_CONSTANT_OBJECT_ATTRIBUTES(RegistryPath, OBJ_KERNEL_HANDLE);
    OBJECT_ATTRIBUTES   instancesAttr = { 0 };
    OBJECT_ATTRIBUTES   defaultInstanceAttr = { 0 };
    UNICODE_STRING      instancesKeyName = RTL_CONSTANT_STRING(L"Instances");
    UNICODE_STRING      instancesValueName = RTL_CONSTANT_STRING(L"DefaultInstance");
    UNICODE_STRING      defaultInstName = RTL_CONSTANT_STRING(MF_DEFAULT_INST_NAME);
    UNICODE_STRING      altitudeValueName = RTL_CONSTANT_STRING(L"Altitude");
    WCHAR               altitudeValueBuf[] = MF_ALTITUDE;
    UNICODE_STRING      flagsValueName = RTL_CONSTANT_STRING(L"Flags");
    ULONG               flagsValueBuf = MF_FLAGS;
    HANDLE              hBaseKey = nullptr, hInstancesKey = nullptr, hDefaultInstanceKey = nullptr;

    log("In MiniFilterInitRegistry");
    KdPrint((DRIVER_PREFIX "%wZ\n", RegistryPath));

    status = ZwOpenKey(&hBaseKey, KEY_WRITE, &driverRegistryBase);
    if (!NT_SUCCESS(status)) {
        err("ZwOpenKey basekey fail", status);
        hBaseKey = nullptr;
        goto out;
    }

    InitializeObjectAttributes(&instancesAttr, &instancesKeyName, OBJ_KERNEL_HANDLE, hBaseKey, nullptr);
    status = ZwCreateKey(&hInstancesKey, KEY_WRITE, &instancesAttr, 0, nullptr, 0, nullptr);
    if (!NT_SUCCESS(status)) {
        err("ZwCreateKey Instances fail", status);
        hInstancesKey = nullptr;
        goto out;
    }

    status = ZwSetValueKey(hInstancesKey, &instancesValueName, 0, REG_SZ, defaultInstName.Buffer, defaultInstName.MaximumLength);
    if (!NT_SUCCESS(status)) {
        err("ZwSetValueKey DefaultInstance fail", status);
        goto out;
    }

    InitializeObjectAttributes(&defaultInstanceAttr, &defaultInstName, OBJ_KERNEL_HANDLE, hInstancesKey, nullptr);
    status = ZwCreateKey(&hDefaultInstanceKey, KEY_WRITE, &defaultInstanceAttr, 0, nullptr, 0, nullptr);
    if (!NT_SUCCESS(status)) {
        err("ZwCreateKey Default Instance fail", status);
        hDefaultInstanceKey = nullptr;
        goto out;
    }

    status = ZwSetValueKey(hDefaultInstanceKey, &altitudeValueName, 0, REG_SZ, &altitudeValueBuf, sizeof(altitudeValueBuf));
    if (!NT_SUCCESS(status)) {
        err("ZwSetValueKey Altitude fail", status);
        goto out;
    }

    status = ZwSetValueKey(hDefaultInstanceKey, &flagsValueName, 0, REG_DWORD, &flagsValueBuf, sizeof(flagsValueBuf));
    if (!NT_SUCCESS(status)) {
        err("ZwSetValueKey Flags fail", status);
        goto out;
    }

    log("MiniFilterInitRegistry Done");

out:
    if (hDefaultInstanceKey)
        ZwClose(hDefaultInstanceKey);
    if (hInstancesKey)
        ZwClose(hInstancesKey);
    if (hBaseKey)
        ZwClose(hBaseKey);

    return status;
}

NTSTATUS MiniFilterRegisterFilter(PDRIVER_OBJECT DriverObject, PFLT_FILTER *OutFilter, PFLT_FILTER_UNLOAD_CALLBACK DeleteProtectorCbUnload)
{
    log("In MiniFilterRegisterFilter");

    NTSTATUS status = STATUS_SUCCESS;

    const FLT_OPERATION_REGISTRATION callbacks[] = {
        { IRP_MJ_CREATE, 0, DeleteProtectorCbPreCreate, nullptr },
        { IRP_MJ_SET_INFORMATION, 0, DeleteProtectorCbPreSetInformation, nullptr },
        { IRP_MJ_OPERATION_END }
    };

    const FLT_REGISTRATION reg = {
        sizeof(FLT_REGISTRATION),
        FLT_REGISTRATION_VERSION,
        0,
        nullptr,
        callbacks,
        DeleteProtectorCbUnload,
        DeleteProtectorCbInstanceSetup,
        DeleteProtectorCbInstanceQueryTeardown
    };

    log("MiniFilterRegisterFilter Done");
    status = FltRegisterFilter(DriverObject, &reg, OutFilter);
    return status;
}

NTSTATUS DeleteProtectorCbInstanceSetup(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags, DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType)
{
    log("In CbInstanceSetup");
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);

    return VolumeFilesystemType == FLT_FSTYPE_NTFS ? STATUS_SUCCESS : STATUS_FLT_DO_NOT_ATTACH;
}

NTSTATUS DeleteProtectorCbInstanceQueryTeardown(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
{
    log("In CbInstanceQueryTeardown");
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    return STATUS_SUCCESS;
}

FLT_PREOP_CALLBACK_STATUS DeleteProtectorCbPreCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(CompletionContext);

    if (Data->RequestorMode == KernelMode)
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    const auto          &params = Data->Iopb->Parameters.Create;
    PUNICODE_STRING     fileName = nullptr;
    UNICODE_STRING      extension = { 0 };

    if (params.Options & FILE_DELETE_ON_CLOSE) {
        log("Opened for delete");
        fileName = &FltObjects->FileObject->FileName;
        KdPrint((DRIVER_PREFIX "Delete on close: %wZ\n", fileName));
        
        if (!NT_SUCCESS(FltParseFileName(fileName, &extension, nullptr, nullptr)))
            return FLT_PREOP_SUCCESS_NO_CALLBACK;

        KdPrint((DRIVER_PREFIX "Found extension: %wZ\n", &extension));
        if (ExistsExtension(g_ExtensionList, &extension)) {
            Data->IoStatus.Status = STATUS_ACCESS_DENIED;
            log("Delete blocked");
            return FLT_PREOP_COMPLETE;
        }
    }
    
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS DeleteProtectorCbPreSetInformation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (Data->RequestorMode == KernelMode)
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    const auto                  &operation = Data->Iopb->Parameters.SetFileInformation;
    PFLT_FILE_NAME_INFORMATION  fi = nullptr;

    if (operation.FileInformationClass != FileDispositionInformation && \
        operation.FileInformationClass != FileDispositionInformationEx)
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    auto info = (PFILE_DISPOSITION_INFORMATION)operation.InfoBuffer;
    if (!(info->DeleteFile & 1))
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

   if (!NT_SUCCESS(FltGetFileNameInformation(Data, FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_NORMALIZED, &fi)))
       return FLT_PREOP_SUCCESS_NO_CALLBACK;

   KdPrint((DRIVER_PREFIX "Set to delete: %wZ\n", fi->Name));
   if (!NT_SUCCESS(FltParseFileNameInformation(fi))) {
       FltReleaseFileNameInformation(fi);
       return FLT_PREOP_SUCCESS_NO_CALLBACK;
   }

   KdPrint((DRIVER_PREFIX "Found extension: %wZ\n", fi->Extension));
   if (ExistsExtension(g_ExtensionList, &fi->Extension)) {
       Data->IoStatus.Status = STATUS_ACCESS_DENIED;
       log("Delete blocked");
       FltReleaseFileNameInformation(fi);
       return FLT_PREOP_COMPLETE;
   }

   FltReleaseFileNameInformation(fi);
   return FLT_PREOP_SUCCESS_NO_CALLBACK;
}