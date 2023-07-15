#pragma once

struct ExecutiveResource
{
    void Init(void);
    void Delete(void);
    void Lock(void);
    void Unlock(void);
    void LockShared(void);
    void UnlockShared(void);

private:
    ERESOURCE m_eresource;
    bool      m_in_critical_region;
};