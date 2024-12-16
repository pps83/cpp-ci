#pragma once
#include <vector>
#include <string>
#include <map>

// Locates platform toolsets for all installed VS versions
class VsToolsets
{
    struct VsInstall
    {
        std::string installationPath;
        std::string displayName;
        std::string productDisplayVersion; // catalog_productDisplayVersion
        std::string installationVersion;

        std::string vcVersion; // last dir in MSBuild/Microsoft/VC
        std::vector<std::string> vcPlatforms; // dirs in MSBuild/Microsoft/%vcVersion%/v170/Platforms
        std::map<std::string, std::vector<std::string>> vcPlatformToolsets;
    };
    std::vector<VsInstall> locations;

public:
    void testVsToolsets();
};
