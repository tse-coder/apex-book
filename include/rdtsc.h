#pragma once
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>

inline uint64_t rdtsc() {
    return __rdtsc();
}
#elif defined(__aarch64__)
inline uint64_t rdtsc() {
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
}
#else
#include <chrono>
inline uint64_t rdtsc() {
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}
#endif
