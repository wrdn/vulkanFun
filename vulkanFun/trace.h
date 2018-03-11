#pragma once

#include <stdio.h>
#define NOMINMAX

#define TRACE_ENABLED

#if defined TRACE_ENABLED

#define TRACE(v, ...) \
		{ \
			char __buffv__[1024]; \
			sprintf(__buffv__, "[TRACE] " v "\n", __VA_ARGS__); \
			printf(__buffv__); \
		}
#else

#define TRACE(v, ...)
#endif
