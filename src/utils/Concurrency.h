#pragma once

#ifdef ENABLE_MULTITHREADING

#include <atomic>
#include <condition_variable>
#include <mutex>

template <typename T>
using Atomic = std::atomic<T>;
using Mutex = std::mutex;
using ConditionVariable = std::condition_variable;

template <typename T>
using LockGuard = std::lock_guard<T>;
template <typename T>
using UniqueLock = std::unique_lock<T>;

#else

#include "utils/Assert.h"

template <typename T>
class DummyAtomic {
public:
    DummyAtomic(T val = 0) : val_(val) {
    }

    T fetch_add(T arg, std::memory_order /*order*/ = std::memory_order_seq_cst) {  // NOLINT
        T old = val_;
        val_ += arg;
        return old;
    }

    T load(std::memory_order /*order*/ = std::memory_order_seq_cst) const {  // NOLINT
        return val_;
    }
    void store(T val, std::memory_order /*order*/ = std::memory_order_seq_cst) {  // NOLINT
        val_ = val;
    }
    bool compare_exchange_strong(T& expected, T desired,  // NOLINT
                                 std::memory_order /*success*/ = std::memory_order_seq_cst,
                                 std::memory_order /*failure*/ = std::memory_order_seq_cst) {
        if (val_ == expected) {
            val_ = desired;
            return true;
        } else {
            expected = val_;
            return false;
        }
    }
    bool compare_exchange_weak(T& expected, T desired,  // NOLINT
                                 std::memory_order /*success*/ = std::memory_order_seq_cst,
                                 std::memory_order /*failure*/ = std::memory_order_seq_cst) {
        if (val_ == expected) {
            val_ = desired;
            return true;
        } else {
            expected = val_;
            return false;
        }
    }

private:
    T val_;
};

class DummyMutex {
public:
    void lock() {  // NOLINT
    }
    void unlock() {  // NOLINT
    }
    bool try_lock() {  // NOLINT
        return true;
    }
};

class DummyConditionVariable {
public:
    template <typename Lock, typename Predicate>
    void wait(Lock& /*lock*/, Predicate pred) {
        ASSERT(pred());
    }
    void notify_one() {}
    void notify_all() {}
};

template <typename T>
using Atomic = DummyAtomic<T>;

using Mutex = DummyMutex;

using ConditionVariable = DummyConditionVariable;

template <typename MutexT>
class LockGuard {
public:
    explicit LockGuard(MutexT& /*m*/) {
    }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

template <typename MutexT>
class UniqueLock {
public:
    explicit UniqueLock(MutexT& /*m*/) {
    }
    UniqueLock(const UniqueLock&) = delete;
    UniqueLock& operator=(const UniqueLock&) = delete;
};

#endif
