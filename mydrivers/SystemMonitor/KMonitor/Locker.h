#pragma once

template<typename TLock>
struct Locker
{
    explicit Locker(TLock &lock) : m_lock(lock)
    {
        m_lock.Lock();
    }
    ~Locker()
    {
        m_lock.Unlock();
    }
private:
    TLock &m_lock;
};