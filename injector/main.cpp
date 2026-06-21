#include <Windows.h>
#include <commdlg.h>
#include <cstdio>
#include <string>
#include <filesystem>
#include "process.h"
#include "manual_map.h"

#pragma comment(lib, "comdlg32.lib")

static constexpr const wchar_t* INI_FILE = L"injector.ini";
static constexpr const wchar_t* INI_SECTION = L"Config";

// ============================================================================
// INI helpers
// ============================================================================

std::wstring GetIniPath() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        path = path.substr(0, pos + 1);
    path += INI_FILE;
    return path;
}

std::wstring ReadIniString(const wchar_t* key, const wchar_t* def = L"") {
    auto ini = GetIniPath();
    wchar_t buf[MAX_PATH]{};
    GetPrivateProfileStringW(INI_SECTION, key, def, buf, MAX_PATH, ini.c_str());
    return buf;
}

int ReadIniInt(const wchar_t* key, int def = 0) {
    auto ini = GetIniPath();
    return GetPrivateProfileIntW(INI_SECTION, key, def, ini.c_str());
}

void WriteIniString(const wchar_t* key, const wchar_t* value) {
    auto ini = GetIniPath();
    WritePrivateProfileStringW(INI_SECTION, key, value, ini.c_str());
}

// ============================================================================
// File picker
// ============================================================================

std::wstring BrowseForExe() {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"Executable (*.exe)\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = L"Select target game executable";

    if (GetOpenFileNameW(&ofn))
        return file;
    return {};
}

// ============================================================================
// Utility
// ============================================================================

std::wstring GetDllFullPath() {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath);
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        path = path.substr(0, pos + 1);
    path += L"UnityCheeto.dll";
    return path;
}

std::wstring FilenameFromPath(const std::wstring& path) {
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        return path.substr(pos + 1);
    return path;
}

void PrintBanner(const wchar_t* target, const wchar_t* exe, bool listenMode) {
    printf("\n");
    printf("  +======================================+\n");
    printf("  |       UnityCheeto Injector           |\n");
    printf("  |       Manual Map | x64               |\n");
    printf("  +======================================+\n");
    printf("\n");
    printf("  [*] Mode:   %s\n", listenMode ? "Listen (attach)" : "Launch (auto-start)");
    printf("  [*] Target: %ls\n", target);
    printf("  [*] Exe:    %ls\n", exe);
    printf("\n");
}

// ============================================================================
// Injection logic (shared)
// ============================================================================

int DoInject(HANDLE hProcess, DWORD pid) {
    auto dllPath = GetDllFullPath();
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("  [!] DLL not found: %ls\n", dllPath.c_str());
        printf("  [!] Place UnityCheeto.dll next to this executable.\n");
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    printf("  [*] Starting manual map (PID: %lu)...\n\n", pid);

    MapResult result = ManualMap(hProcess, dllPath.c_str());
    CloseHandle(hProcess);

    printf("\n");
    if (result == MapResult::Success) {
        printf("  [+] Injection successful!\n");
        printf("  [+] Press INSERT in-game to open menu.\n");
        exit(0);
    } else {
        printf("  [!] Injection failed: %s\n", MapResultToString(result));
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }
}

// ============================================================================
// Mode: Launch (original behavior — start game then inject)
// ============================================================================

int ModeLaunch(const std::wstring& exePath) {
    std::wstring processName = FilenameFromPath(exePath);
    std::wstring exeDir = exePath.substr(0, exePath.find_last_of(L"\\/"));

    auto dllPath = GetDllFullPath();
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("  [!] DLL not found: %ls\n", dllPath.c_str());
        printf("  [!] Place UnityCheeto.dll next to this executable.\n");
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    printf("  [*] Launching %ls...\n", processName.c_str());

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (!CreateProcessW(exePath.c_str(), nullptr, nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, exeDir.c_str(), &si, &pi)) {
        printf("  [!] Failed to launch (error: %lu)\n", GetLastError());
        printf("  [!] Try running as Administrator.\n");
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    printf("  [+] Process created (PID: %lu) — suspended\n", pi.dwProcessId);

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    printf("  [*] Waiting for process initialization...\n");
    Sleep(3000);

    if (!IsProcessAlive(pi.dwProcessId)) {
        printf("  [!] Process exited before injection.\n");
        CloseHandle(pi.hProcess);
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    return DoInject(pi.hProcess, pi.dwProcessId);
}

// ============================================================================
// Mode: Listen (wait for process to appear, then inject)
// ============================================================================

int ModeListen(const std::wstring& exePath) {
    std::wstring processName = FilenameFromPath(exePath);

    auto dllPath = GetDllFullPath();
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("  [!] DLL not found: %ls\n", dllPath.c_str());
        printf("  [!] Place UnityCheeto.dll next to this executable.\n");
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    printf("  [*] Waiting for %ls to start...\n", processName.c_str());
    printf("  [*] (Press Ctrl+C to cancel)\n\n");

    DWORD pid = 0;
    while (true) {
        pid = FindProcessByName(processName.c_str());
        if (pid != 0) break;
        Sleep(100);
    }

    printf("  [+] Process detected (PID: %lu)\n", pid);

    // Wait a bit for the process to finish its own initialization
    printf("  [*] Waiting for process initialization...\n");
    Sleep(100);

    if (!IsProcessAlive(pid)) {
        printf("  [!] Process exited before injection.\n");
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("  [!] Failed to open process (error: %lu)\n", GetLastError());
        printf("  [!] Try running as Administrator.\n");
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    return DoInject(hProcess, pid);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"UnityCheeto Injector");

    // Read mode: 0 = Launch (default), 1 = Listen
    int mode = ReadIniInt(L"Mode", 0);
    bool listenMode = (mode == 1);

    // Load saved exe path from ini
    std::wstring exePath = ReadIniString(L"ExePath");

    // If no saved path or file doesn't exist, prompt user
    if (exePath.empty() || GetFileAttributesW(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("\n  [*] No saved target. Please select the game exe...\n");
        exePath = BrowseForExe();
        if (exePath.empty()) {
            printf("  [!] No file selected. Exiting.\n");
            return 1;
        }
        WriteIniString(L"ExePath", exePath.c_str());
        printf("  [+] Saved to injector.ini\n");
    }

    std::wstring processName = FilenameFromPath(exePath);
    PrintBanner(processName.c_str(), exePath.c_str(), listenMode);

    if (listenMode) {
        return ModeListen(exePath);
    } else {
        return ModeLaunch(exePath);
    }
}
