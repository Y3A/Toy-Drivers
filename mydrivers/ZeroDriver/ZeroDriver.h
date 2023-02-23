#ifndef ZERO_H
#define ZERO_H

NTSTATUS CompleteIrp(_Inout_ PIRP Irp, _In_ NTSTATUS status=STATUS_SUCCESS, _In_ ULONG_PTR info=0);
NTSTATUS GetPreviousReadWriteValue(_In_ HANDLE hKey, _In_ PUNICODE_STRING name, _Inout_ PKEY_VALUE_PARTIAL_INFORMATION RegistryData, _In_ ULONG length, _Out_ PDWORD ReturnData);
void ZeroUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS ZeroCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ZeroRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
NTSTATUS ZeroDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ZeroGetStats(_Out_ ZEROSTATS *stats);
NTSTATUS ZeroClearStats(void);
void UpdateStats(_In_ DWORD AddRead, _In_ DWORD AddWrite);

struct FastMutex {
    void Init() {
        ExInitializeFastMutex(&mutex_, 0);
    }
    void Lock() {
        ExAcquireFastMutex(&mutex_);
    }
    void Unlock() {
        ExReleaseFastMutex(&mutex_);
    }
private:
    FAST_MUTEX mutex_;
};

template<typename TLock>
struct Locker {
    explicit Locker(TLock& lock) : lock_(lock) {
        lock_.Lock();
    }
    ~Locker() {
        lock_.Unlock();
    }
private:
    TLock& lock_;
};

#endif