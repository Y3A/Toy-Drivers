#include "Locker.h"
#include "MelodyDriver.h"
#include "PlaybackState.h"

#include <ntddbeep.h>

PlaybackState::PlaybackState()
{
    InitializeListHead(&m_notesHead);
    KeInitializeSemaphore(&m_counter, 0, MAX_NOTES);
    KeInitializeEvent(&m_stopEvent, SynchronizationEvent, FALSE);
    ExInitializeLookasideListEx(&m_notesPool, nullptr, nullptr, PagedPool, 0, sizeof(LinkedNote), MELODY_TAG, 0);
}

PlaybackState::~PlaybackState()
{
    PlaybackTerminate();
    ExDeleteLookasideListEx(&m_notesPool);
}

NTSTATUS PlaybackState::PlaybackStart(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    DWORD               bufLen;
    NTSTATUS            status;
    HANDLE              process = 0, pid = 0;
    UNICODE_STRING      csrssName = RTL_CONSTANT_STRING(L"csrss.exe");
    ULONG               curSession = 0;
    CLIENT_ID           cid;
    OBJECT_ATTRIBUTES   procAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(nullptr, OBJ_KERNEL_HANDLE);

    Locker<FastMutex> locker(m_lock);
    if (m_thread)
        return STATUS_SUCCESS;

    status = IoGetRequestorSessionId(Irp, &curSession);
    if (!NT_SUCCESS(status))
        return status;

    ZwQuerySystemInformation(SystemProcessInformation, nullptr, 0, &bufLen);
    auto buf = ExAllocatePool2(POOL_FLAG_PAGED, bufLen, MELODY_TAG);
    if (!buf)
        return STATUS_INSUFFICIENT_RESOURCES;

    status = ZwQuerySystemInformation(SystemProcessInformation, buf, bufLen, nullptr);
    if (!NT_SUCCESS(status)) {
        ExFreePool(buf);
        return status;
    }

    for (auto info = (SYSTEM_PROCESS_INFORMATION *)buf; info->NextEntryOffset; info = (PSYSTEM_PROCESS_INFORMATION)((DWORD64)info + (DWORD64)info->NextEntryOffset))
        if (RtlCompareUnicodeString(&csrssName, &(info->ImageName), TRUE) == 0 && info->SessionId == curSession) {
            pid = info->UniqueProcessId;
            break;
        }

    cid.UniqueProcess = pid;
    cid.UniqueThread = nullptr;

    status = ZwOpenProcess(&process, PROCESS_ALL_ACCESS, &procAttributes, &cid);
    if (!NT_SUCCESS(status)) {
        ExFreePool(buf);
        return status;
    }

    ExFreePool(buf);

    return IoCreateSystemThread(
        DeviceObject,
        &m_thread,
        THREAD_ALL_ACCESS,
        nullptr,
        process,
        nullptr,
        PlaybackState::PlayMelody,
        this
    );
}

NTSTATUS PlaybackState::AddNotes(Note *notes, DWORD count)
{
    for (DWORD i = 0; i < count; i++) {
        auto linkedNote = (LinkedNote *)ExAllocateFromLookasideListEx(&m_notesPool);
        if (linkedNote == nullptr)
            return STATUS_INSUFFICIENT_RESOURCES;
        
        RtlCopyMemory(linkedNote, &notes[i], sizeof(Note));

        {
            Locker<FastMutex> locker(m_lock);
            InsertTailList(&m_notesHead, &linkedNote->link);
        }
    }

    KeReleaseSemaphore(&m_counter, IO_NO_INCREMENT, count, FALSE);

    return STATUS_SUCCESS;
}

void PlaybackState::PlaybackTerminate(void)
{
    if (&m_thread) {
        KeSetEvent(&m_stopEvent, IO_NO_INCREMENT, FALSE);
        PVOID    thread;
        NTSTATUS status = ObReferenceObjectByHandle(m_thread, SYNCHRONIZE, *PsThreadType, KernelMode, &thread, nullptr);
        if (NT_SUCCESS(status)) {
            KeWaitForSingleObject(thread, Executive, KernelMode, FALSE, nullptr);
            ObDereferenceObject(thread);
        }
        ZwClose(m_thread);
        m_thread = nullptr;
    }
}

void PlaybackState::PlayMelody(PVOID ctx)
{
    return ((PlaybackState *)(ctx))->PlayMelody();
}

void PlaybackState::PlayMelody(void)
{
    PDEVICE_OBJECT          beepDevice;
    UNICODE_STRING          beepName = RTL_CONSTANT_STRING(DD_BEEP_DEVICE_NAME_U);
    PFILE_OBJECT            beepFile;
    NTSTATUS                status;
    PVOID                   objects[] = { &m_counter, &m_stopEvent };
    PLIST_ENTRY             noteLink;
    Note                    *note;
    LARGE_INTEGER           interval = { 0 };
    BEEP_SET_PARAMETERS     params = { 0 };
    KEVENT                  doneEvent = { 0 };
    IO_STATUS_BLOCK         isb = { 0 };

    KeInitializeEvent(&doneEvent, NotificationEvent, FALSE);

    status = IoGetDeviceObjectPointer(&beepName, GENERIC_WRITE, &beepFile, &beepDevice);
    if (!NT_SUCCESS(status))
        return;

    while (1) {
        status = KeWaitForMultipleObjects(2, objects, WaitAny, Executive, KernelMode, FALSE, nullptr, nullptr);
        if (status == STATUS_WAIT_1) break;
        
        {
            Locker<FastMutex> locker(m_lock);
            noteLink = RemoveHeadList(&m_notesHead);
            NT_ASSERT((noteLink != &m_notesHead));
        }
        
        note = CONTAINING_RECORD(noteLink, LinkedNote, link);
        interval.QuadPart = -10000LL * note->dwDuration;

        if (note->dwFrequency == 0) {
            KeDelayExecutionThread(KernelMode, FALSE, &interval);
            continue;
        }

        params.Duration = note->dwDuration;
        params.Frequency = note->dwFrequency;

        for (DWORD i = 0; i < note->dwRepeat; i++) {
            auto irp = IoBuildDeviceIoControlRequest(
                IOCTL_BEEP_SET,
                beepDevice,
                &params,
                sizeof(params),
                nullptr,
                0,
                FALSE,
                &doneEvent,
                &isb
            );
            if (!irp)
                break;

            status = IoCallDriver(beepDevice, irp);
            if (!NT_SUCCESS(status))
                break;

            if (status == STATUS_PENDING)
                KeWaitForSingleObject(&doneEvent, Executive, KernelMode, FALSE, nullptr);

            KeDelayExecutionThread(KernelMode, FALSE, &interval);
            interval.QuadPart = -10000LL * note->dwDelay;
            KeDelayExecutionThread(KernelMode, FALSE, &interval);
        }

        ExFreeToLookasideListEx(&m_notesPool, note);
    }

    ObDereferenceObject(beepFile);
    return;
}