#include <Windows.h>
#include <cstdio>
#include "process.h"
#include "manual_map.h"

// ============================================================================
// Configuration — change these for your target
// ============================================================================

constexpr const wchar_t* TARGET_PROCESS = L"preternatural.exe";
constexpr const wchar_t* DLL_FILE       = L"UnityCheeto.dll";
constexpr DWORD POLL_INTERVAL_MS        = 100;

// ============================================================================

void PrintBanner() {
    printf("\n");
    printf("  ╔══════════════════════════════════════╗\n");
    printf("  ║       UnityCheeto Injector           ║\n");
    printf("  ║       Manual Map | x64               ║\n");
    printf("  ╚══════════════════════════════════════╝\n");
    printf("\n");
    printf("  [*] Target: %ls\n", TARGET_PROCESS);
    printf("  [*] DLL:    %ls\n", DLL_FILE);
    printf("\n");
}

std::wstring GetDllFullPath() {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring path(exePath);
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        path = path.substr(0, pos + 1);

    path += DLL_FILE;
    return path;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"UnityCheeto Injector");
    PrintBanner();

    // Verify DLL exists
    auto dllPath = GetDllFullPath();
    DWORD attr = GetFileAttributesW(dllPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        printf("  [!] DLL not found: %ls\n", dllPath.c_str());
        printf("  [!] Place %ls next to this executable.\n", DLL_FILE);
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    printf("  [*] DLL path: %ls\n", dllPath.c_str());
    printf("  [*] Waiting for %ls...\n\n", TARGET_PROCESS);

    // Poll for target process
    DWORD pid = 0;
    while (!pid) {
        pid = FindProcessByName(TARGET_PROCESS);
        if (!pid)
            Sleep(POLL_INTERVAL_MS);
    }

    printf("  [+] Found %ls (PID: %lu)\n", TARGET_PROCESS, pid);

    // Small delay to let the process initialize
    printf("  [*] Waiting 2s for process initialization...\n");
    Sleep(2000);

    // Verify process is still alive
    if (!IsProcessAlive(pid)) {
        printf("  [!] Process exited before injection.\n");
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    // Open process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("  [!] Failed to open process (error: %lu)\n", GetLastError());
        printf("  [!] Try running as Administrator.\n");
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }

    printf("  [*] Process handle acquired.\n");
    printf("  [*] Starting manual map...\n\n");

    // Inject
    MapResult result = ManualMap(hProcess, dllPath.c_str());
    CloseHandle(hProcess);

    printf("\n");
    if (result == MapResult::Success) {
        printf("  [+] Injection successful!\n");
        printf("  [+] Press INSERT in-game to open menu.\n");
      /*  Sleep(1500);*/
		exit(0);
        return 0;
    } else {
        printf("  [!] Injection failed: %s\n", MapResultToString(result));
        printf("\n  Press any key to exit...\n");
        system("pause >nul");
        return 1;
    }
}
