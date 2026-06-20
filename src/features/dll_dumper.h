#pragma once
#include "features/feature_base.h"

class DllDumper : public FeatureBase {
public:
    static DllDumper& Get() { static DllDumper inst; return inst; }
    void init() override;
    void onTick() override;
    void drawUI() override;

    const char* name() const override { return "DLL Dumper"; }
    const char* category() const override { return "Debug"; }

    ConfigVar<std::string> outputDir{"dll_dumper.output_dir", "dumped_dlls"};
    int dumpCount_ = 0;

    static void ensureOutputDir(const std::string& path);
    static bool saveDll(const std::string& dir, const std::string& name, const void* data, size_t size);
    void scanAndDump();

private:
    DllDumper() : FeatureBase("dll_dumper", true) {}
};
