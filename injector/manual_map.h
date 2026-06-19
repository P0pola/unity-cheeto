#pragma once

#include <Windows.h>
#include <string>

enum class MapResult {
    Success,
    FileNotFound,
    InvalidPE,
    AllocFailed,
    WriteFailed,
    ImportFailed,
    ShellcodeFailed,
    ThreadFailed,
};

const char* MapResultToString(MapResult r);

MapResult ManualMap(HANDLE hProcess, const wchar_t* dllPath);
