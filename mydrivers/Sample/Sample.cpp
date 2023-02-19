#include <ntddk.h>

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    RTL_OSVERSIONINFOW VersionInfo = { 0 };
    NTSTATUS           status = STATUS_SUCCESS;

    KdPrint(("Sample Driver Loaded\n"));

    DriverObject->DriverUnload = SampleUnload;
    VersionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);

    if (!NT_SUCCESS(status = RtlGetVersion(&VersionInfo))) {
        KdPrint(("Error calling RtlGetVersion with status of 0x%08X\n", status));
        goto out;
    }

    KdPrint(("The Windows OS Major number is %lu, Minor number is %lu, build number is %lu\n", VersionInfo.dwMajorVersion,
        VersionInfo.dwMinorVersion, VersionInfo.dwBuildNumber));

out:
    return status;
}

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    KdPrint(("Sample Driver Unloaded\n"));

    return;
}