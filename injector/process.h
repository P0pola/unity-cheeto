#pragma once

#include <Windows.h>
#include <string>

DWORD FindProcessByName(const wchar_t* processName);
bool IsProcessAlive(DWORD pid);
