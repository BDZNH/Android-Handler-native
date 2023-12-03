#ifndef A_BASE_H
#define A_BASE_H

#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <unordered_map>

#include "Errors.h"

#ifndef DISALLOW_EVIL_CONSTRUCTORS
#define DISALLOW_EVIL_CONSTRUCTORS(name) \
    name(const name &); \
    name &operator=(const name &) /* NOLINT */
#endif // !DISALLOW_EVIL_CONSTRUCTORS

using MutexAutoLock = std::unique_lock<std::mutex>;
using KeyedI32Vector = std::unordered_map<int32_t, int32_t>;

using Condition = std::condition_variable;
#ifdef _WIN32
using thread_id_t = unsigned long;
#else
using thread_id_t = long long;
#endif // _WIN32

using Mutex = std::mutex;

#endif // !A_BASE_H



