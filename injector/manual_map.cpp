#include "manual_map.h"
#include <fstream>
#include <vector>
#include <cstdio>

const char* MapResultToString(MapResult r) {
    switch (r) {
    case MapResult::Success:        return "Success";
    case MapResult::FileNotFound:   return "DLL file not found";
    case MapResult::InvalidPE:      return "Invalid PE format";
    case MapResult::AllocFailed:    return "Remote allocation failed";
    case MapResult::WriteFailed:    return "WriteProcessMemory failed";
    case MapResult::ImportFailed:   return "Import resolution failed";
    case MapResult::ShellcodeFailed:return "Shellcode setup failed";
    case MapResult::ThreadFailed:   return "Remote thread creation failed";
    }
    return "Unknown error";
}

// ---------------------------------------------------------------------------
// Shellcode params passed to remote thread
// ---------------------------------------------------------------------------

struct LoaderData {
    BYTE* imageBase;
    IMAGE_NT_HEADERS* ntHeaders;
    BYTE* relocBase;
    IMAGE_IMPORT_DESCRIPTOR* importDir;
    decltype(&LoadLibraryA) fnLoadLibraryA;
    decltype(&GetProcAddress) fnGetProcAddress;
};

// ---------------------------------------------------------------------------
// Shellcode — position-independent, executed in remote process
// This function MUST NOT reference any globals or imports directly.
// Disable optimizations to prevent inlining/reordering that breaks copying.
// ---------------------------------------------------------------------------

#pragma optimize("", off)
#pragma runtime_checks("", off)

static DWORD WINAPI Shellcode(LoaderData* data) {
    // Process relocations
    if (data->relocBase) {
        auto delta = reinterpret_cast<uintptr_t>(data->imageBase) -
                     data->ntHeaders->OptionalHeader.ImageBase;

        if (delta != 0) {
            auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(data->relocBase);
            while (reloc->VirtualAddress) {
                DWORD count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                auto* entries = reinterpret_cast<WORD*>(reloc + 1);

                for (DWORD i = 0; i < count; i++) {
                    WORD type = entries[i] >> 12;
                    WORD offset = entries[i] & 0x0FFF;

                    if (type == IMAGE_REL_BASED_DIR64) {
                        auto* patch = reinterpret_cast<uintptr_t*>(
                            data->imageBase + reloc->VirtualAddress + offset);
                        *patch += delta;
                    } else if (type == IMAGE_REL_BASED_HIGHLOW) {
                        auto* patch = reinterpret_cast<DWORD*>(
                            data->imageBase + reloc->VirtualAddress + offset);
                        *patch += static_cast<DWORD>(delta);
                    }
                }

                reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                    reinterpret_cast<BYTE*>(reloc) + reloc->SizeOfBlock);
            }
        }
    }

    // Process imports
    if (data->importDir) {
        auto* importDesc = data->importDir;
        while (importDesc->Name) {
            auto* moduleName = reinterpret_cast<char*>(data->imageBase + importDesc->Name);
            HMODULE hMod = data->fnLoadLibraryA(moduleName);

            if (hMod) {
                auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                    data->imageBase + importDesc->FirstThunk);
                auto* origThunk = importDesc->OriginalFirstThunk
                    ? reinterpret_cast<IMAGE_THUNK_DATA*>(data->imageBase + importDesc->OriginalFirstThunk)
                    : thunk;

                while (origThunk->u1.AddressOfData) {
                    if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                        thunk->u1.Function = reinterpret_cast<uintptr_t>(
                            data->fnGetProcAddress(hMod,
                                reinterpret_cast<LPCSTR>(IMAGE_ORDINAL(origThunk->u1.Ordinal))));
                    } else {
                        auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                            data->imageBase + origThunk->u1.AddressOfData);
                        thunk->u1.Function = reinterpret_cast<uintptr_t>(
                            data->fnGetProcAddress(hMod, importByName->Name));
                    }
                    thunk++;
                    origThunk++;
                }
            }

            importDesc++;
        }
    }

    // Process TLS callbacks
    auto& tlsDir = data->ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (tlsDir.Size) {
        auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(data->imageBase + tlsDir.VirtualAddress);
        auto* callbacks = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(tls->AddressOfCallBacks);
        if (callbacks) {
            while (*callbacks) {
                (*callbacks)(data->imageBase, DLL_PROCESS_ATTACH, nullptr);
                callbacks++;
            }
        }
    }

    // Call DllMain
    auto fnEntry = reinterpret_cast<BOOL(WINAPI*)(HMODULE, DWORD, LPVOID)>(
        data->imageBase + data->ntHeaders->OptionalHeader.AddressOfEntryPoint);
    fnEntry(reinterpret_cast<HMODULE>(data->imageBase), DLL_PROCESS_ATTACH, nullptr);

    return 0;
}

// Dummy function to calculate shellcode size
static void ShellcodeEnd() {}

#pragma runtime_checks("", restore)
#pragma optimize("", on)

// ---------------------------------------------------------------------------
// ManualMap implementation
// ---------------------------------------------------------------------------

static std::vector<BYTE> ReadFileToMemory(const wchar_t* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    auto size = file.tellg();
    file.seekg(0);
    std::vector<BYTE> buffer(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

MapResult ManualMap(HANDLE hProcess, const wchar_t* dllPath) {
    // Read DLL file
    auto rawDll = ReadFileToMemory(dllPath);
    if (rawDll.empty())
        return MapResult::FileNotFound;

    // Validate PE
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(rawDll.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return MapResult::InvalidPE;

    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(rawDll.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
        return MapResult::InvalidPE;

    if (ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
        return MapResult::InvalidPE;

    // Allocate memory in target process
    DWORD imageSize = ntHeaders->OptionalHeader.SizeOfImage;
    BYTE* remoteBase = static_cast<BYTE*>(
        VirtualAllocEx(hProcess, nullptr, imageSize,
                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!remoteBase)
        return MapResult::AllocFailed;

    printf("    [*] Remote base: 0x%p\n", remoteBase);
    printf("    [*] Image size: 0x%X\n", imageSize);

    // Prepare local image buffer (mapped layout)
    std::vector<BYTE> localImage(imageSize, 0);

    // Copy headers
    memcpy(localImage.data(), rawDll.data(), ntHeaders->OptionalHeader.SizeOfHeaders);

    // Map sections
    auto* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        if (sectionHeader[i].SizeOfRawData == 0) continue;
        memcpy(
            localImage.data() + sectionHeader[i].VirtualAddress,
            rawDll.data() + sectionHeader[i].PointerToRawData,
            sectionHeader[i].SizeOfRawData);
    }

    // Fix NT headers pointer in local image
    auto* localNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        localImage.data() + dosHeader->e_lfanew);

    // Prepare loader data
    LoaderData loaderData{};
    loaderData.imageBase = remoteBase;
    loaderData.ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        remoteBase + dosHeader->e_lfanew);

    // Relocation directory
    auto& relocDir = localNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir.Size) {
        loaderData.relocBase = remoteBase + relocDir.VirtualAddress;
    }

    // Import directory
    auto& importDirEntry = localNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDirEntry.Size) {
        loaderData.importDir = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            remoteBase + importDirEntry.VirtualAddress);
    }

    // Resolve kernel32 functions (same address across processes)
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    loaderData.fnLoadLibraryA = reinterpret_cast<decltype(&LoadLibraryA)>(
        GetProcAddress(hKernel32, "LoadLibraryA"));
    loaderData.fnGetProcAddress = reinterpret_cast<decltype(&GetProcAddress)>(
        GetProcAddress(hKernel32, "GetProcAddress"));

    // Write mapped image to remote process
    if (!WriteProcessMemory(hProcess, remoteBase, localImage.data(), imageSize, nullptr)) {
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        return MapResult::WriteFailed;
    }

    // Allocate and write loader data
    auto* remoteLoaderData = static_cast<LoaderData*>(
        VirtualAllocEx(hProcess, nullptr, sizeof(LoaderData),
                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!remoteLoaderData) {
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        return MapResult::AllocFailed;
    }

    if (!WriteProcessMemory(hProcess, remoteLoaderData, &loaderData, sizeof(loaderData), nullptr)) {
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteLoaderData, 0, MEM_RELEASE);
        return MapResult::WriteFailed;
    }

    // Calculate shellcode size and write it to remote
    auto shellcodeSize = reinterpret_cast<uintptr_t>(&ShellcodeEnd) -
                         reinterpret_cast<uintptr_t>(&Shellcode);
    if (shellcodeSize == 0 || shellcodeSize > 0x10000)
        shellcodeSize = 0x1000; // fallback reasonable size

    auto* remoteShellcode = static_cast<BYTE*>(
        VirtualAllocEx(hProcess, nullptr, shellcodeSize,
                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!remoteShellcode) {
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteLoaderData, 0, MEM_RELEASE);
        return MapResult::ShellcodeFailed;
    }

    if (!WriteProcessMemory(hProcess, remoteShellcode, &Shellcode, shellcodeSize, nullptr)) {
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteLoaderData, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);
        return MapResult::ShellcodeFailed;
    }

    printf("    [*] Shellcode at: 0x%p (%zu bytes)\n", remoteShellcode, shellcodeSize);

    // Execute shellcode
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteShellcode),
        remoteLoaderData, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteLoaderData, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);
        return MapResult::ThreadFailed;
    }

    // Wait for shellcode to finish
    WaitForSingleObject(hThread, 10000);

    // Cleanup shellcode + loader data (image stays mapped)
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    VirtualFreeEx(hProcess, remoteLoaderData, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);

    // Erase PE headers in remote to reduce detection
    BYTE zeros[0x1000]{};
    WriteProcessMemory(hProcess, remoteBase, zeros, sizeof(zeros), nullptr);

    printf("    [*] Thread exit code: %lu\n", exitCode);
    return MapResult::Success;
}
