#pragma once
#include <stdint.h>
typedef uint64_t u64;
static inline u64 osGetTime() { static u64 tick; return ++tick; }
static inline void svcSleepThread(long long) {}
