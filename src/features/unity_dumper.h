#pragma once
#include "features/feature_base.h"

class UnityDumper : public FeatureBase {
public:
    static UnityDumper& Get() { static UnityDumper inst; return inst; }
    bool init() override;
    void onTick() override;
    void drawUI() override;

    const char* name() const override { return "Unity Dumper"; }
    const char* category() const override { return "Debug"; }

    ConfigVar<std::string> outputDir{"unity_dumper.output_dir", "dumped"};
    int dumpCount_ = 0;
    bool metadataDumped_ = false;

    static void ensureOutputDir(const std::string& path);
    static bool saveFile(const std::string& dir, const std::string& name, const void* data, size_t size);
    void scanAndDumpDlls();
    void dumpGlobalMetadata();

private:
    UnityDumper() : FeatureBase("unity_dumper", true) {}
};
