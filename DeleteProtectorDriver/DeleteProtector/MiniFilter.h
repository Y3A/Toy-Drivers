#pragma once

#define MF_ALTITUDE L"341802"
#define MF_FLAGS 0
#define MF_DEFAULT_INST_NAME L"DeleteProtector Instance"

NTSTATUS MiniFilterInitRegistry(PUNICODE_STRING RegistryPath);
NTSTATUS MiniFilterRegisterFilter(PDRIVER_OBJECT DriverObject, PFLT_FILTER *OutFilter, PFLT_FILTER_UNLOAD_CALLBACK DeleteProtectorCbUnload);
NTSTATUS DeleteProtectorCbInstanceSetup(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags, DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType);
NTSTATUS DeleteProtectorCbInstanceQueryTeardown(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags);
FLT_PREOP_CALLBACK_STATUS DeleteProtectorCbPreCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext);
FLT_PREOP_CALLBACK_STATUS DeleteProtectorCbPreSetInformation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext);

extern ExtensionList *g_ExtensionList;