#pragma once

#include <Windows.h>

#define TRACE(v, ...) { char buff[1024]; sprintf_s(buff, sizeof(buff), "[TRACE] " v "\n", __VA_ARGS__); printf(buff); OutputDebugString(buff); }