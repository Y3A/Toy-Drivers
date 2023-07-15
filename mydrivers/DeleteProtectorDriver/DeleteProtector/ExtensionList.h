#pragma once

#include "ExecutiveResource.h"

#define MAX_STRLEN (NTSTRSAFE_MAX_CCH * sizeof(WCHAR))

struct ExtensionList
{
    LIST_ENTRY          head;
    ExecutiveResource   lock;
};

DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT) struct ExtensionEntry
{
    UNICODE_STRING  extension;
    LIST_ENTRY      entry;
};

NTSTATUS InitializeExtensionList(ExtensionList **OutList);
NTSTATUS AddExtension(ExtensionList *List, PUNICODE_STRING Extension);
void DeleteExtension(ExtensionList *List, PUNICODE_STRING Extension);
void DeleteAllExtensions(ExtensionList *List);
bool ExistsExtension(ExtensionList *List, PUNICODE_STRING Extension);
static void DeleteAllExtensionsCallerLock(ExtensionList *List);
void DestroyExtensionList(ExtensionList **List);