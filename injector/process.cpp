#include "process.h"
#include <TlHelp32.h>

DWORD FindProcessByName(const wchar_t* processName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                CloseHandle(snap);
                return entry.th32ProcessID;
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return 0;
}

bool IsProcessAlive(DWORD pid) {
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return false;

    DWORD exitCode = 0;
    GetExitCodeProcess(proc, &exitCode);
    CloseHandle(proc);
    return exitCode == STILL_ACTIVE;
}
