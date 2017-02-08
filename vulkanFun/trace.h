#pragma once

#define NOMINMAX

#include <Windows.h>

#define TRACE_ENABLED

#if defined TRACE_ENABLED
#define TRACE(v, ...) { char __buffv__[1024]; sprintf_s(__buffv__, sizeof(__buffv__), "[TRACE] " v "\n", __VA_ARGS__); printf(__buffv__); OutputDebugString(__buffv__); }
#else
#define TRACE(v, ...)
#endif