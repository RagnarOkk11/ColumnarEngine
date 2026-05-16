#pragma once

#ifdef ENABLE_MULTITHREADING

#include <atomic>
#include <mutex>

template <typename T>
using Atomic = std::atomic<T>;

using Mutex = std::mutex;

#else

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

template <typename T>
using Atomic = DummyAtomic<T>;

using Mutex = DummyMutex;

#endif
