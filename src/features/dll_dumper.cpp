#include "pch.h"
#include "features/dll_dumper.h"
#include <filesystem>
#include <fstream>
#include <Windows.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DllDumper::ensureOutputDir(const std::string& path) {
    std::filesystem::create_directories(path);
}

bool DllDumper::saveDll(const std::string& dir, const std::string& name, const void* data, size_t size) {
    ensureOutputDir(dir);
    auto path = std::filesystem::path(dir) / name;

    if (std::filesystem::exists(path)) {
        // Same size = likely same content, skip
        auto existingSize = std::filesystem::file_size(path);
        if (existingSize == size) {
            LOG_INFO("[DllDumper] Skipped duplicate: {}", path.string());
            return false;
        }
        int idx = 1;
        while (true) {
            auto stem = path.stem().string();
            auto alt = std::filesystem::path(dir) / (stem + "_" + std::to_string(idx) + ".dll");
            if (!std::filesystem::exists(alt)) { path = alt; break; }
            idx++;
        }
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        LOG_ERROR("[DllDumper] Failed to write: {}", path.string());
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data), size);
    LOG_INFO("[DllDumper] Saved {} ({} bytes)", path.string(), size);
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

    // COFF header starts at peOffset+4
    // Optional header starts at peOffset+4+20
    size_t optOffset = peOffset + 4 + 20;
    if (optOffset + 2 > regionSize) return false;

    auto magic = *reinterpret_cast<const uint16_t*>(base + optOffset);
    size_t clrDirOffset;
    if (magic == 0x10B) {
        // PE32: COM descriptor is data directory index 14, starts at optOffset+208
        clrDirOffset = optOffset + 208;
    } else if (magic == 0x20B) {
        // PE32+: COM descriptor at optOffset+224
        clrDirOffset = optOffset + 224;
    } else {
        return false;
    }

    if (clrDirOffset + 8 > regionSize) return false;

    auto clrRva = *reinterpret_cast<const uint32_t*>(base + clrDirOffset);
    auto clrSize = *reinterpret_cast<const uint32_t*>(base + clrDirOffset + 4);

    // Valid .NET DLL has a non-zero CLR header
    return clrRva != 0 && clrSize != 0;
}

// Get the size of the PE from headers (SizeOfImage or end of last section)
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

    // Find the furthest section end (raw data)
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

// Extract assembly name from .NET metadata
static std::string getAssemblyName(const uint8_t* base, size_t regionSize) {
    auto e_lfanew = *reinterpret_cast<const int32_t*>(base + 0x3C);
    size_t peOffset = static_cast<size_t>(e_lfanew);
    size_t optOffset = peOffset + 4 + 20;

    auto magic = *reinterpret_cast<const uint16_t*>(base + optOffset);
    size_t clrDirOffset = (magic == 0x10B) ? optOffset + 208 : optOffset + 224;

    auto clrRva = *reinterpret_cast<const uint32_t*>(base + clrDirOffset);

    // RVA to file offset: find which section contains clrRva
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

    // CLR header → Metadata RVA is at offset 8 in IMAGE_COR20_HEADER
    size_t clrFileOff = rvaToOffset(clrRva);
    if (clrFileOff == 0 || clrFileOff + 16 > regionSize) return "";

    auto metadataRva = *reinterpret_cast<const uint32_t*>(base + clrFileOff + 8);
    size_t metaOff = rvaToOffset(metadataRva);
    if (metaOff == 0 || metaOff + 20 > regionSize) return "";

    // Metadata root: signature 0x424A5342 ("BSJB")
    if (*reinterpret_cast<const uint32_t*>(base + metaOff) != 0x424A5342) return "";

    // Skip version string: offset 12 = version length (4 bytes), then version string
    auto versionLen = *reinterpret_cast<const uint32_t*>(base + metaOff + 12);
    size_t streamsStart = metaOff + 16 + versionLen;
    if (streamsStart + 4 > regionSize) return "";

    // Flags(2) + Streams count(2)
    streamsStart += 2;
    uint16_t numStreams = *reinterpret_cast<const uint16_t*>(base + streamsStart);
    streamsStart += 2;

    // Find #Strings and #~ (or #-) streams
    size_t stringsOff = 0, stringsSize = 0;
    size_t tablesOff = 0;
    size_t pos = streamsStart;

    for (uint16_t i = 0; i < numStreams && pos + 8 < regionSize; i++) {
        auto sOff = *reinterpret_cast<const uint32_t*>(base + pos);
        auto sSize = *reinterpret_cast<const uint32_t*>(base + pos + 4);
        pos += 8;
        // Read stream name (null-terminated, 4-byte aligned)
        std::string sName;
        while (pos < regionSize && base[pos] != 0) {
            sName += static_cast<char>(base[pos]);
            pos++;
        }
        pos++; // skip null
        pos = (pos + 3) & ~3; // align to 4

        if (sName == "#Strings") { stringsOff = metaOff + sOff; stringsSize = sSize; }
        else if (sName == "#~" || sName == "#-") { tablesOff = metaOff + sOff; }
    }

    if (stringsOff == 0 || tablesOff == 0) return "";
    if (tablesOff + 24 > regionSize) return "";

    // #~ stream header: 4(reserved) + 1(MajorVersion) + 1(MinorVersion) + 1(HeapSizes) + 1(reserved) + 8(Valid) + 8(Sorted)
    uint8_t heapSizes = base[tablesOff + 6];
    uint64_t validMask = *reinterpret_cast<const uint64_t*>(base + tablesOff + 8);

    // Row counts start at offset 24
    size_t rowCountPos = tablesOff + 24;

    // Count how many table bits are set, find Assembly table (index 0x20 = 32)
    // First, read row counts for all valid tables
    uint32_t rowCounts[64] = {};
    for (int t = 0; t < 64 && rowCountPos + 4 <= regionSize; t++) {
        if (validMask & (1ULL << t)) {
            rowCounts[t] = *reinterpret_cast<const uint32_t*>(base + rowCountPos);
            rowCountPos += 4;
        }
    }

    // Assembly table is table 0x20; if it doesn't exist, no assembly name
    if (!(validMask & (1ULL << 0x20)) || rowCounts[0x20] == 0) return "";

    // Now we need to find the offset of the Assembly table rows in the stream
    // Tables are stored sequentially after the row counts
    size_t tableDataStart = rowCountPos;
    bool strIdx4 = (heapSizes & 0x01) != 0; // #Strings index size
    bool guidIdx4 = (heapSizes & 0x02) != 0;
    bool blobIdx4 = (heapSizes & 0x04) != 0;
    size_t strIdxSize = strIdx4 ? 4 : 2;
    size_t guidIdxSize = guidIdx4 ? 4 : 2;
    size_t blobIdxSize = blobIdx4 ? 4 : 2;

    // Calculate row sizes for tables before Assembly (0x20)
    // This is complex — simplified: skip tables 0x00..0x1F
    auto getRowSize = [&](int tableIdx) -> size_t {
        switch (tableIdx) {
            case 0x00: return 4 + strIdxSize + guidIdxSize + guidIdxSize + guidIdxSize; // Module
            case 0x01: return 2 + strIdxSize + strIdxSize; // TypeRef (ResolutionScope coded 2 bytes min)
            case 0x02: return 4 + strIdxSize + strIdxSize + 2 + 2 + 2; // TypeDef (simplified)
            case 0x04: return 2 + strIdxSize + blobIdxSize; // Field
            case 0x06: return 4 + strIdxSize + blobIdxSize; // MethodDef (simplified with 2-byte indices)
            case 0x08: return 2 + strIdxSize + blobIdxSize; // Param
            case 0x20: return 4 + 2 + 2 + 2 + strIdxSize + strIdxSize + blobIdxSize + blobIdxSize + strIdxSize; // Assembly
            default: return 0;
        }
    };
    (void)getRowSize;

    // Simpler approach: Assembly row layout is fixed:
    // HashAlgId(4) + MajorVersion(2) + MinorVersion(2) + BuildNumber(2) + RevisionNumber(2)
    // + Flags(4) + PublicKey(blob) + Name(string) + Culture(string)
    // Name field offset = 4+2+2+2+2+4+blobIdxSize = 16 + blobIdxSize

    // But we need to skip all tables before 0x20 to find where Assembly rows start.
    // Too complex to fully generalize. Shortcut: just search #Strings for known patterns.

    // Easier approach: the Module table (0x00) row 0 has the module name at a known offset.
    // Module row: Generation(2) + Name(string idx) + Mvid(guid) + EncId(guid) + EncBaseId(guid)
    // The Name string index is at offset 2 in the first row of table data.
    if (tableDataStart + 2 + strIdxSize > regionSize) return "";

    // But let's try a more robust method: scan #Strings for ".dll" patterns
    // Actually, the simplest reliable method: read Module table Name field
    // Module is table 0x00, always first if present
    if (!(validMask & 1ULL)) return ""; // no Module table

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
// Memory scan for .NET DLLs
// ---------------------------------------------------------------------------

void DllDumper::scanAndDump() {
    std::string dir = static_cast<std::string>(outputDir);
    ensureOutputDir(dir);

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    auto* addr = reinterpret_cast<uint8_t*>(si.lpMinimumApplicationAddress);
    auto* maxAddr = reinterpret_cast<uint8_t*>(si.lpMaximumApplicationAddress);

    MEMORY_BASIC_INFORMATION mbi;
    int found = 0;

    LOG_INFO("[DllDumper] Scanning process memory for .NET assemblies...");

    while (addr < maxAddr) {
        if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) break;

        // Only scan committed, readable, private/mapped memory (skip images — those are on-disk modules)
        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
            !(mbi.Protect & PAGE_GUARD) &&
            mbi.Type != MEM_IMAGE) {

            auto* base = reinterpret_cast<uint8_t*>(mbi.BaseAddress);
            size_t regionSize = mbi.RegionSize;

            // Scan for MZ headers within this region
            for (size_t offset = 0; offset + 0x200 < regionSize; offset += 4) {
                if (base[offset] == 'M' && base[offset + 1] == 'Z') {
                    size_t remaining = regionSize - offset;
                    if (isDotNetDll(base + offset, remaining)) {
                        size_t peSize = getPESize(base + offset, remaining);
                        if (peSize >= 512 && peSize <= remaining) {
                            std::string asmName = getAssemblyName(base + offset, remaining);
                            std::string filename;
                            if (!asmName.empty()) {
                                // Use assembly name; ensure it ends with .dll
                                if (asmName.size() < 4 || asmName.substr(asmName.size() - 4) != ".dll")
                                    asmName += ".dll";
                                filename = asmName;
                            } else {
                                filename = "unknown_" + std::format("{:X}", reinterpret_cast<uintptr_t>(base + offset)) + ".dll";
                            }
                            if (saveDll(dir, filename, base + offset, peSize)) {
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

    LOG_INFO("[DllDumper] Scan complete — found {} .NET assemblies", found);
}

// ---------------------------------------------------------------------------
// Init & UI
// ---------------------------------------------------------------------------

void DllDumper::init() {
}

void DllDumper::onTick() {}

void DllDumper::drawUI() {
    if (Widgets::Section("dll_dumper", TR("DLL Dumper"))) {
        ImGui::Text("Output: %s", static_cast<std::string>(outputDir).c_str());
        ImGui::Text("Dumped: %d", dumpCount_);
        if (ImGui::Button("Rescan")) {
            scanAndDump();
        }
        Widgets::SectionEnd();
    }
}

// ---------------------------------------------------------------------------
static auto& _dlldumper = DllDumper::Get();
