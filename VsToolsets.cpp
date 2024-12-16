#include "VsToolsets.h"
#include "util.h"
#include <boost/process.hpp>

static std::vector<std::string> getCmdOutput(const char* cmd)
{
    namespace bp = boost::process;

    std::vector<std::string> ret;
    bp::ipstream out;
    std::error_code ec;
    bp::child c(cmd, bp::std_out > out, ec);
    if (ec)
        return ret;
    for (std::string line; std::getline(out, line);)
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (!line.empty())
            ret.push_back(line);
    }
    c.wait();
    auto exit_code = c.exit_code();
    if (exit_code != 0)
        ret.clear();
    return ret;
}

static std::vector<std::string> getCmdOutput(const std::string& cmd)
{
    return getCmdOutput(cmd.c_str());
}

static std::vector<std::string> getVswhereProperties(const char* property)
{
    const char* vswherePath = "Microsoft Visual Studio\\Installer";
    return getCmdOutput(fmtStr("cmd /C set \"PATH=%%PATH%%;%%ProgramFiles%%\\%s;%%ProgramFiles(x86)%%\\%s\" && "
        "vswhere -nologo -prerelease -sort -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property %s",
        vswherePath, vswherePath, property));
}

static std::vector<std::string> getSortedDirs(const std::string& path)
{
    return getCmdOutput(fmtStr("cmd /C dir /A:D /B /O:N \"%s\"", path.c_str()));
}

void VsToolsets::testVsToolsets()
{
    std::vector<std::string> vsLocations = getVswhereProperties("installationPath");
    std::vector<std::string> vsNames = getVswhereProperties("displayName");
    std::vector<std::string> vsDisplayVersion = getVswhereProperties("catalog_productDisplayVersion");
    std::vector<std::string> vsVersion = getVswhereProperties("installationVersion");

    if (vsLocations.size() != vsNames.size() || vsLocations.size() != vsDisplayVersion.size() || vsLocations.size() != vsVersion.size())
        return;
    for (size_t i=0; i<vsLocations.size(); ++i)
    {
        VsInstall vsInstall;
        vsInstall.installationPath = vsLocations[i];
        vsInstall.displayName = vsNames[i];
        vsInstall.productDisplayVersion = vsDisplayVersion[i];
        vsInstall.installationVersion = vsVersion[i];

        std::vector<std::string> vcVersions = getSortedDirs(fmtStr("%s\\MSBuild\\Microsoft\\VC", vsInstall.installationPath.c_str()));
        if (vcVersions.empty())
            continue;
        vsInstall.vcVersion = vcVersions.back(); // use last (latest) vc tools

        vsInstall.vcPlatforms = getSortedDirs(fmtStr("%s\\MSBuild\\Microsoft\\VC\\%s\\Platforms", vsInstall.installationPath.c_str(), vsInstall.vcVersion.c_str()));

        for (const auto& platform : vsInstall.vcPlatforms)
        {
            auto toolsets = getSortedDirs(fmtStr("%s\\MSBuild\\Microsoft\\VC\\%s\\Platforms\\%s\\PlatformToolsets", vsInstall.installationPath.c_str(), vsInstall.vcVersion.c_str(), platform.c_str()));
            if (!toolsets.empty())
                vsInstall.vcPlatformToolsets[platform] = std::move(toolsets);
        }
        if (!vsInstall.vcPlatforms.empty() && !vsInstall.vcPlatformToolsets.empty())
            locations.push_back(std::move(vsInstall));
    }
}
