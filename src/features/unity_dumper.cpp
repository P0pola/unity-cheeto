#include "pch.h"
#include "features/unity_dumper.h"
#include <filesystem>
#include <fstream>
#include <Windows.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void UnityDumper::ensureOutputDir(const std::string& path) {
    std::filesystem::create_directories(path);
}

bool UnityDumper::saveFile(const std::string& dir, const std::string& name, const void* data, size_t size) {
    ensureOutputDir(dir);
    auto path = std::filesystem::path(dir) / name;

    if (std::filesystem::exists(path)) {
        auto existingSize = std::filesystem::file_size(path);
        if (existingSize == size) {
            LOG_INFO("[UnityDumper] Skipped duplicate: {}", path.string());
            return false;
        }
        int idx = 1;
        while (true) {
            auto stem = path.stem().string();
            auto ext = path.extension().string();
            auto alt = std::filesystem::path(dir) / (stem + "_" + std::to_string(idx) + ext);
            if (!std::filesystem::exists(alt)) { path = alt; break; }
            idx++;
        }
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        LOG_ERROR("[UnityDumper] Failed to write: {}", path.string());
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data), size);
    LOG_INFO("[UnityDumper] Saved {} ({} bytes)", path.string(), size);
    return true;
}

// ---------------------------------------------------------------------------
// PE validation helpers
// ---------------------------------------------------------------------------

static bool isValidPE(const uint8_t* base, size_t regionSize) {
    if (regionSize < 0x40) return false;
    if (base[0] != 'M' || base[1] != 'Z') return false;

    auto e_lfanew = *reinterpret_cast<const int32_t*>(base + 0x3C);
    if (e_lfanew < 0 || static_cast<size_t>(e_lfanew) + 4 > regionSize) return false;

    auto* pe_sig = reinterpret_cast<const uint32_t*>(base + e_lfanew);
    return *pe_sig == 0x00004550; // "PE\0\0"
}

static bool isDotNetDll(const uint8_t* base, size_t regionSize) {
    if (!isValidPE(base, regionSize)) return false;

    auto e_lfanew = *reinterpret_cast<const int32_t*>(base + 0x3C);
    size_t peOffset = static_cast<size_t>(e_lfanew);

    size_t optOffset = peOffset + 4 + 20;
    if (optOffset + 2 > regionSize) return false;

    auto magic = *reinterpret_cast<const uint16_t*>(base + optOffset);
    size_t clrDirOffset;
    if (magic == 0x10B) {
        clrDirOffset = optOffset + 208;
    } else if (magic == 0x20B) {
        clrDirOffset = optOffset + 224;
    } else {
        return false;
    }

    if (clrDirOffset + 8 > regionSize) return false;

    auto clrRva = *reinterpret_cast<const uint32_t*>(base + clrDirOffset);
    auto clrSize = *reinterpret_cast<const uint32_t*>(base + clrDirOffset + 4);

    return clrRva != 0 && clrSize != 0;
}

static size_t getPESize(const uint8_t* base, size_t regionSize) {
    auto e_lfanew = *reinterpret_cast<const int32_t*>(base + 0x3C);
    size_t peOffset = static_cast<size_t>(e_lfanew);
    size_t optOffset = peOffset + 4 + 20;

    auto magic = *reinterpret_cast<const uint16_t*>(base + optOffset);
    size_t sizeOfHeaders;
    size_t sectionTableOffset;
    uint16_t numSections = *reinterpret_cast<const uint16_t*>(base + peOffset + 4 + 2);

    if (magic == 0x10B) {
        sizeOfHeaders = *reinterpret_cast<const uint32_t*>(base + optOffset + 60);
        sectionTableOffset = optOffset + 96 + 8 * (*reinterpret_cast<const uint32_t*>(base + optOffset + 92));
    } else {
        sizeOfHeaders = *reinterpret_cast<const uint32_t*>(base + optOffset + 60);
        sectionTableOffset = optOffset + 112 + 8 * (*reinterpret_cast<const uint32_t*>(base + optOffset + 108));
    }

    size_t maxEnd = sizeOfHeaders;
    for (uint16_t i = 0; i < numSections; i++) {
        size_t secEntry = sectionTableOffset + i * 40;
        if (secEntry + 40 > regionSize) break;

        auto rawOffset = *reinterpret_cast<const uint32_t*>(base + secEntry + 20);
        auto rawSize = *reinterpret_cast<const uint32_t*>(base + secEntry + 16);
        size_t secEnd = rawOffset + rawSize;
        if (secEnd > maxEnd) maxEnd = secEnd;
    }

    return (maxEnd <= regionSize) ? maxEnd : regionSize;
}

static std::string getAssemblyName(const uint8_t* base, size_t regionSize) {
    auto e_lfanew = *reinterpret_cast<const int32_t*>(base + 0x3C);
    size_t peOffset = static_cast<size_t>(e_lfanew);
    size_t optOffset = peOffset + 4 + 20;

    auto magic = *reinterpret_cast<const uint16_t*>(base + optOffset);
    size_t clrDirOffset = (magic == 0x10B) ? optOffset + 208 : optOffset + 224;

    auto clrRva = *reinterpret_cast<const uint32_t*>(base + clrDirOffset);

    uint16_t numSections = *reinterpret_cast<const uint16_t*>(base + peOffset + 4 + 2);
    size_t optSize = *reinterpret_cast<const uint16_t*>(base + peOffset + 4 + 16);
    size_t sectionTableOffset = peOffset + 4 + 20 + optSize;

    auto rvaToOffset = [&](uint32_t rva) -> size_t {
        for (uint16_t i = 0; i < numSections; i++) {
            size_t sec = sectionTableOffset + i * 40;
            if (sec + 40 > regionSize) return 0;
            auto va = *reinterpret_cast<const uint32_t*>(base + sec + 12);
            auto vs = *reinterpret_cast<const uint32_t*>(base + sec + 8);
            auto rawOff = *reinterpret_cast<const uint32_t*>(base + sec + 20);
            if (rva >= va && rva < va + vs)
                return rawOff + (rva - va);
        }
        return 0;
    };

    size_t clrFileOff = rvaToOffset(clrRva);
    if (clrFileOff == 0 || clrFileOff + 16 > regionSize) return "";

    auto metadataRva = *reinterpret_cast<const uint32_t*>(base + clrFileOff + 8);
    size_t metaOff = rvaToOffset(metadataRva);
    if (metaOff == 0 || metaOff + 20 > regionSize) return "";

    if (base[metaOff] != 'B' || base[metaOff + 1] != 'S' ||
        base[metaOff + 2] != 'J' || base[metaOff + 3] != 'B')
        return "";

    size_t cursor = metaOff + 12;
    if (cursor + 4 > regionSize) return "";
    auto versionLen = *reinterpret_cast<const uint32_t*>(base + cursor);
    cursor += 4 + ((versionLen + 3) & ~3u);

    if (cursor + 4 > regionSize) return "";
    cursor += 2; // flags
    auto numStreams = *reinterpret_cast<const uint16_t*>(base + cursor);
    cursor += 2;

    size_t stringsOff = 0, stringsSize = 0;
    size_t tablesOff = 0;

    for (int s = 0; s < numStreams && cursor + 8 <= regionSize; s++) {
        auto sOff = *reinterpret_cast<const uint32_t*>(base + cursor);
        auto sSize = *reinterpret_cast<const uint32_t*>(base + cursor + 4);
        cursor += 8;

        std::string sName;
        while (cursor < regionSize && base[cursor] != 0) {
            sName += static_cast<char>(base[cursor]);
            cursor++;
        }
        cursor++;
        cursor = (cursor + 3) & ~3ULL;

        if (sName == "#Strings") {
            stringsOff = metaOff + sOff;
            stringsSize = sSize;
        } else if (sName == "#~" || sName == "#-") {
            tablesOff = metaOff + sOff;
        }
    }

    if (stringsOff == 0 || tablesOff == 0) return "";

    if (tablesOff + 24 > regionSize) return "";
    auto heapSizes = base[tablesOff + 6];
    bool strIdx4 = (heapSizes & 0x01) != 0;

    auto validMask = *reinterpret_cast<const uint64_t*>(base + tablesOff + 8);
    size_t tableDataStart = tablesOff + 24;

    int tableCount = 0;
    for (int i = 0; i < 64; i++)
        if (validMask & (1ULL << i)) tableCount++;

    tableDataStart += tableCount * 4;

    if (!(validMask & 1ULL)) return "";

    size_t moduleNameIdx;
    if (strIdx4)
        moduleNameIdx = *reinterpret_cast<const uint32_t*>(base + tableDataStart + 2);
    else
        moduleNameIdx = *reinterpret_cast<const uint16_t*>(base + tableDataStart + 2);

    if (stringsOff + moduleNameIdx >= regionSize) return "";

    std::string name;
    size_t p = stringsOff + moduleNameIdx;
    while (p < regionSize && p < stringsOff + stringsSize && base[p] != 0) {
        name += static_cast<char>(base[p]);
        p++;
    }

    return name;
}

// ---------------------------------------------------------------------------
// global-metadata.dat dumper
// ---------------------------------------------------------------------------

// IL2CPP global-metadata.dat magic: 0xFAB11BAF
static constexpr uint32_t METADATA_MAGIC = 0xFAB11BAF;

// Compute metadata file size by scanning all offset/size pairs in the header.
// The header is a sequence of int32 pairs after (sanity, version). We don't need
// to define the full struct — just iterate all pairs up to the known max count
// for the detected version, and find the furthest (offset + size).
static size_t calcMetadataSize(const uint8_t* base, size_t maxSize) {
    if (maxSize < 8) return 0;

    size_t headerStart = 8; // skip sanity + version
    size_t maxPairs = (maxSize - headerStart) / 8;

    // Cap at a reasonable maximum (no known version exceeds 35 pairs)
    if (maxPairs > 35) maxPairs = 35;

    size_t computedEnd = 0;
    for (size_t i = 0; i < maxPairs; i++) {
        size_t fieldOff = headerStart + i * 8;
        auto offset = *reinterpret_cast<const int32_t*>(base + fieldOff);
        auto size = *reinterpret_cast<const int32_t*>(base + fieldOff + 4);

        if (offset < 0 || size < 0) continue;
        // Sanity: offset should be beyond the header itself
        if (offset < static_cast<int32_t>(headerStart)) continue;

        size_t end = static_cast<size_t>(offset) + static_cast<size_t>(size);
        if (end > maxSize) break; // hit garbage, stop
        if (end > computedEnd) computedEnd = end;
    }

    if (computedEnd == 0 || computedEnd > maxSize)
        return 0;

    return computedEnd;
}

void UnityDumper::dumpGlobalMetadata() {
    std::string dir = static_cast<std::string>(outputDir);

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    auto* addr = reinterpret_cast<uint8_t*>(si.lpMinimumApplicationAddress);
    auto* maxAddr = reinterpret_cast<uint8_t*>(si.lpMaximumApplicationAddress);

    MEMORY_BASIC_INFORMATION mbi;

    LOG_INFO("[UnityDumper] Scanning for global-metadata.dat in memory...");

    while (addr < maxAddr) {
        if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) break;

        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
            !(mbi.Protect & PAGE_GUARD) &&
            mbi.RegionSize >= 280) {

            auto* base = reinterpret_cast<uint8_t*>(mbi.BaseAddress);
            size_t regionSize = mbi.RegionSize;

            for (size_t offset = 0; offset + 280 < regionSize; offset += 4) {
                auto* candidate = reinterpret_cast<const uint32_t*>(base + offset);
                if (*candidate == METADATA_MAGIC) {
                    auto* metaBase = base + offset;
                    auto version = *reinterpret_cast<const int32_t*>(metaBase + 4);

                    if (version < 16 || version > 31) continue;

                    size_t remaining = regionSize - offset;
                    size_t metaSize = calcMetadataSize(metaBase, remaining);
                    if (metaSize < 1024) continue;

                    if (saveFile(dir, "global-metadata.dat", metaBase, metaSize)) {
                        metadataDumped_ = true;
                        LOG_INFO("[UnityDumper] global-metadata.dat dumped (version={}, size={})",
                                 version, metaSize);
                        return;
                    }
                }
            }
        }

        addr = reinterpret_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }

    LOG_ERROR("[UnityDumper] global-metadata.dat not found in memory");
}

// ---------------------------------------------------------------------------
// Memory scan for .NET DLLs
// ---------------------------------------------------------------------------

void UnityDumper::scanAndDumpDlls() {
    std::string dir = static_cast<std::string>(outputDir) + "/dlls";
    ensureOutputDir(dir);

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    auto* addr = reinterpret_cast<uint8_t*>(si.lpMinimumApplicationAddress);
    auto* maxAddr = reinterpret_cast<uint8_t*>(si.lpMaximumApplicationAddress);

    MEMORY_BASIC_INFORMATION mbi;
    int found = 0;

    LOG_INFO("[UnityDumper] Scanning process memory for .NET assemblies...");

    while (addr < maxAddr) {
        if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) break;

        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
            !(mbi.Protect & PAGE_GUARD) &&
            mbi.Type != MEM_IMAGE) {

            auto* base = reinterpret_cast<uint8_t*>(mbi.BaseAddress);
            size_t regionSize = mbi.RegionSize;

            for (size_t offset = 0; offset + 0x200 < regionSize; offset += 4) {
                if (base[offset] == 'M' && base[offset + 1] == 'Z') {
                    size_t remaining = regionSize - offset;
                    if (isDotNetDll(base + offset, remaining)) {
                        size_t peSize = getPESize(base + offset, remaining);
                        if (peSize >= 512 && peSize <= remaining) {
                            std::string asmName = getAssemblyName(base + offset, remaining);
                            std::string filename;
                            if (!asmName.empty()) {
                                if (asmName.size() < 4 || asmName.substr(asmName.size() - 4) != ".dll")
                                    asmName += ".dll";
                                filename = asmName;
                            } else {
                                filename = "unknown_" + std::format("{:X}", reinterpret_cast<uintptr_t>(base + offset)) + ".dll";
                            }
                            if (saveFile(dir, filename, base + offset, peSize)) {
                                found++;
                                dumpCount_++;
                            }
                        }
                    }
                }
            }
        }

        addr = reinterpret_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }

    LOG_INFO("[UnityDumper] Scan complete — found {} .NET assemblies", found);
}

// ---------------------------------------------------------------------------
// Init & UI
// ---------------------------------------------------------------------------

void UnityDumper::init() {
}

void UnityDumper::onTick() {}

void UnityDumper::drawUI() {
    if (Widgets::Section("unity_dumper", TR("Unity Dumper"))) {
        ImGui::Text("Output: %s", static_cast<std::string>(outputDir).c_str());

        ImGui::Spacing();
        ImGui::SeparatorText("DLL Dump");
        ImGui::Text("Dumped: %d", dumpCount_);
        if (ImGui::Button("Dump DLLs")) {
            scanAndDumpDlls();
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Metadata");
        ImGui::Text("Status: %s", metadataDumped_ ? "Exported" : "Not exported");
        if (ImGui::Button("Dump global-metadata.dat")) {
            dumpGlobalMetadata();
        }

        Widgets::SectionEnd();
    }
}

// ---------------------------------------------------------------------------
static auto& _unitydumper = UnityDumper::Get();
