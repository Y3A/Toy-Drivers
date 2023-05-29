#pragma once

template <typename T>
struct SingleLocker
{
    explicit SingleLocker(T &lock) : m_lock(lock)
    {
        m_lock.Lock();
    }

    ~SingleLocker()
    {
        m_lock.Unlock();
    }

private:
    T &m_lock;
};

template <typename T>
struct SharedLocker
{
    explicit SharedLocker(T &lock) : m_lock(lock)
    {
        m_lock.LockShared();
    }

    ~SharedLocker()
    {
        m_lock.UnlockShared();
    }

private:
    T &m_lock;
};
