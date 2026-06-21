#include "pch.h"
#include "features/lua_dumper.h"
#include <filesystem>
#include <fstream>
#include <shellapi.h>

static std::filesystem::path getExeDir() {
    char buf[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
}

// Hook for LuaDLL.luaL_loadbuffer(IntPtr luaState, byte[] buff, int size, string name)
// In il2cpp: int luaL_loadbuffer(void* luaState, Il2CppArray* buff, int size, void* name)
static int hkLuaLoadBuffer(void* luaState, Il2CppArray* buff, int size, void* name) {
    auto& dumper = LuaDumper::Get();

    if (buff && size > 0) {
        std::string scriptName = "unknown";
        if (name) {
            auto* str = reinterpret_cast<UString*>(name);
            if (str && str->m_stringLength > 0) {
                scriptName.resize(str->m_stringLength);
                for (int i = 0; i < str->m_stringLength; i++)
                    scriptName[i] = static_cast<char>(str->m_firstChar[i]);
            }
        }

        const uint8_t* data = buff->data;
        auto outDir = getExeDir() / static_cast<std::string>(dumper.outputDir);

        // Sanitize filename: replace path separators
        std::string filename = scriptName;
        for (auto& c : filename) {
            if (c == '/' || c == '\\') c = '_';
            if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
        }

        // Detect if it's bytecode (Lua 5.1 starts with 0x1B4C7561)
        bool isBytecode = (size >= 4 && data[0] == 0x1B && data[1] == 0x4C && data[2] == 0x75 && data[3] == 0x61);
        if (!filename.ends_with(".lua") && !filename.ends_with(".luac")) {
            filename += isBytecode ? ".luac" : ".lua";
        }

        std::filesystem::create_directories(outDir);
        auto path = outDir / filename;

        if (!std::filesystem::exists(path)) {
            std::ofstream ofs(path, std::ios::binary);
            if (ofs) {
                ofs.write(reinterpret_cast<const char*>(data), size);
                dumper.dumpCount_++;
            }
        }
    }

    return LuaDLL::luaL_loadbuffer_original(luaState, buff, size, name);
}

bool LuaDumper::init() {
    auto* addr = LuaDLL::luaL_loadbuffer_address();
    if (!addr) {
        return false;
    }
    LOG_INFO("[LuaDumper] luaL_loadbuffer at {:p}", addr);

    if (LuaDLL::luaL_loadbuffer_hook(hkLuaLoadBuffer)) {
        hooked_ = true;
        LOG_INFO("[LuaDumper] Hooked OK");
    } else {
        LOG_ERROR("[LuaDumper] Hook failed");
        return false;
    }
    return true;
}

void LuaDumper::drawUI() {
    if (Widgets::Section("lua_dumper", TR("Lua Dumper"))) {
        ImGui::Text("Status: %s", hooked_ ? "Hooked" : "NOT hooked");
        ImGui::Text("Scripts dumped: %d", dumpCount_);
        ImGui::Text("Output: %s", static_cast<std::string>(outputDir).c_str());

        if (ImGui::Button("Open Folder")) {
            auto dir = getExeDir() / static_cast<std::string>(outputDir);
            std::filesystem::create_directories(dir);
            ShellExecuteA(nullptr, "open", dir.string().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
        }

        Widgets::SectionEnd();
    }
}

static auto& _luadumper = LuaDumper::Get();
