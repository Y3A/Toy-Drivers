#include <ntddk.h>
#include <ntstrsafe.h>

#include "ExtensionList.h"
#include "KCommon.h"
#include "KProtector.h"
#include "Locker.h" 

NTSTATUS InitializeExtensionList(ExtensionList **OutList)
{
    log("In InitializeExtensionList");
    ExtensionList *outList = nullptr;

    if (!OutList)
        return STATUS_INVALID_PARAMETER;

    outList = (ExtensionList *)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(ExtensionList), DRIVER_TAG);
    if (outList == nullptr)
        return STATUS_INSUFFICIENT_RESOURCES;

    outList->lock.Init();
    InitializeListHead(&outList->head);

    DEREF_PTR(OutList) = outList;
    return STATUS_SUCCESS;
}

void DestroyExtensionList(ExtensionList **List)
{
    log("In DestroyExtensionList");
    if (!List || !DEREF_PTR(List))
        return;

    SingleLocker<ExecutiveResource> lock(DEREF_PTR(List)->lock);
    DeleteAllExtensionsCallerLock(*List);
    DEREF_PTR(List)->lock.Delete();
    ExFreePool(DEREF_PTR(List));

    DEREF_PTR(List) = nullptr;
    return;
}

NTSTATUS AddExtension(ExtensionList *List, PUNICODE_STRING Extension)
{
    NTSTATUS        status = STATUS_SUCCESS;
    LPWSTR          ext = nullptr;
    ExtensionEntry  *newEntry = nullptr;

    log("In AddExtension");
    KdPrint((DRIVER_PREFIX "Extension: %wZ\n", Extension));

    if (List == nullptr)
        return STATUS_INVALID_PARAMETER;

    ext = (LPWSTR)ExAllocatePool2(POOL_FLAG_PAGED, Extension->MaximumLength, DRIVER_TAG);
    if (ext == nullptr) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        err("ExAllocatePool2 for extension fail", status);
        goto out;
    }

    newEntry = (ExtensionEntry *)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(ExtensionEntry), DRIVER_TAG);
    if (newEntry == nullptr) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        err("ExAllocatePool2 for newentry fail", status);
        goto out;
    }

    RtlCopyMemory(ext, Extension->Buffer, Extension->Length);
    RtlInitUnicodeString(&newEntry->extension, ext);

    {
        SingleLocker<ExecutiveResource> lock(List->lock);
        InsertTailList(&List->head, &newEntry->entry);
    }

    log("Done Add Extension.");

out:
    if (!NT_SUCCESS(status)) {
        if (ext)
            ExFreePool(ext);
        if (newEntry)
            ExFreePool(newEntry);
    }

    return status;
}

void DeleteExtension(ExtensionList *List, PUNICODE_STRING Extension)
{
    if (List == nullptr)
        return;

    log("In DeleteExtension");
    KdPrint((DRIVER_PREFIX "Extension: %wZ\n", Extension));

    SingleLocker<ExecutiveResource> lock(List->lock);

    PLIST_ENTRY           cur = List->head.Flink;
    ExtensionEntry        *curEntry = nullptr;

    while (cur != &List->head) {
        curEntry = CONTAINING_RECORD(cur, ExtensionEntry, entry);
        if (RtlCompareUnicodeString(Extension, &curEntry->extension, TRUE) != 0) {
            cur = cur->Flink;
            continue;
        }

        // Found
        log("Found extension to delete");
        RemoveEntryList(cur);
        ExFreePool(curEntry->extension.Buffer);
        ExFreePool(curEntry);
        break;
    }

    log("Done DeleteExtension");
    return;
}

bool ExistsExtension(ExtensionList *List, PUNICODE_STRING Extension)
{
    log("In ExistsExtension");
    KdPrint((DRIVER_PREFIX "Extension: %wZ\n", Extension));

    SharedLocker<ExecutiveResource> lock(List->lock);

    PLIST_ENTRY           cur = List->head.Flink;
    ExtensionEntry        *curEntry = nullptr;

    while (cur != &List->head) {
        curEntry = CONTAINING_RECORD(cur, ExtensionEntry, entry);
        if (RtlCompareUnicodeString(Extension, &curEntry->extension, TRUE) != 0) {
            cur = cur->Flink;
            continue;
        }

        // Found
        log("Found extension");
        return true;
    }

    log("Done ExistsExtension");
    return false;
}

void DeleteAllExtensions(ExtensionList *List)
{
    SingleLocker<ExecutiveResource> lock(List->lock);

    DeleteAllExtensionsCallerLock(List);

    return;
}

static void DeleteAllExtensionsCallerLock(ExtensionList *List)
{
    if (List == nullptr)
        return;

    log("In DeleteAllExtensionsCallerLock");

    PLIST_ENTRY    cur = nullptr;
    ExtensionEntry *curEntry = nullptr;

    while ((cur = RemoveHeadList(&List->head)) != &List->head) {
        curEntry = CONTAINING_RECORD(cur, ExtensionEntry, entry);
        ExFreePool(curEntry->extension.Buffer);
        ExFreePool(curEntry);
    }

    log("Done DeleteAllExtensionsCallerLock");
    return;
}