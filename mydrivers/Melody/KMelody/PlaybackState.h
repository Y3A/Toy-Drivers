#ifndef PLAYBACK_STATE_H
#define PLAYBACK_STATE_H

#include <ntifs.h>
#include <ntddk.h>

#include "FastMutex.h"
#include "MelodyPublic.h"

#define MAX_NOTES 1000

typedef struct _SYSTEM_PROCESS_INFORMATION
{
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize; //VISTA
    ULONG HardFaultCount; //WIN7
    ULONG NumberOfThreadsHighWatermark; //WIN7
    ULONGLONG CycleTime; //WIN7
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR PageDirectoryBase;

    //
    // This part corresponds to VM_COUNTERS_EX.
    // NOTE: *NOT* THE SAME AS VM_COUNTERS!
    //
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;

    //
    // This part corresponds to IO_COUNTERS
    //
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
    //    SYSTEM_THREAD_INFORMATION TH[1];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef enum _SYSTEM_INFORMATION_CLASS
{
    SystemBasicInformation = 0,
    SystemProcessorInformation = 1,             // obsolete...delete
    SystemPerformanceInformation = 2,
    SystemTimeOfDayInformation = 3,
    SystemPathInformation = 4,
    SystemProcessInformation = 5,
} SYSTEM_INFORMATION_CLASS, *PSYSTEM_INFORMATION_CLASS;

extern "C"
NTSTATUS ZwQuerySystemInformation (
    _In_      SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _Inout_   PVOID                    SystemInformation,
    _In_      ULONG                    SystemInformationLength,
    _Out_opt_ PULONG                   ReturnLength
);

struct LinkedNote: Note
{
    LIST_ENTRY link;
};

struct PlaybackState
{
    PlaybackState();
    ~PlaybackState();

    NTSTATUS    PlaybackStart(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    void        PlaybackTerminate(void);

    NTSTATUS    AddNotes(Note *notes, DWORD count);

    static void PlayMelody(PVOID ctx);
    void        PlayMelody(void);

private:
    LIST_ENTRY           m_notesHead;
    FastMutex            m_lock;
    LOOKASIDE_LIST_EX    m_notesPool;
    KSEMAPHORE           m_counter;
    KEVENT               m_stopEvent;
    HANDLE               m_thread{ nullptr };
};

#endif