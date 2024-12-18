#include "VsToolsets.h"
#include "util.h"
#include <boost/process.hpp>

static std::vector<std::string> getCmdOutput(const char* cmd, bool readStderr = false)
{
    namespace bp = boost::process;

    std::vector<std::string> ret;
    bp::ipstream out;
    std::error_code ec;
    bp::child c;
    if (readStderr)
        c = bp::child(cmd, bp::std_err > out, ec);
    else
        c = bp::child(cmd, bp::std_out > out, ec);
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
    if (exit_code != 0 && !readStderr)
        ret.clear();
    return ret;
}

static std::vector<std::string> getCmdOutput(const std::string& cmd, bool readStderr = false)
{
    return getCmdOutput(cmd.c_str(), readStderr);
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

// Extract version string that looks like 12.34.56.789
static std::string parseDottedVersion(const std::string& s)
{
    size_t start = s.find_first_of("0123456789");
    if (start == std::string::npos)
        return {};
    size_t end = start;
    while (end < s.size() && ((s[end] >= '0' && s[end] <= '9') || s[end] == '.'))
        ++end;
    return s.substr(start, end - start);
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

        if (!readFile(fmtStr("%s\\VC\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt", vsInstall.installationPath.c_str()), vsInstall.vcToolsVersion))
            continue;
        vsInstall.vcToolsVersion = parseDottedVersion(vsInstall.vcToolsVersion);

        auto clOutput = getCmdOutput(fmtStr("\"%s\\VC\\Tools\\MSVC\\%s\\bin\\Hostx86\\x86\\cl.exe\"", vsInstall.installationPath.c_str(), vsInstall.vcToolsVersion.c_str()), true);
        if (clOutput.empty())
            continue;
        vsInstall.clVersion = parseDottedVersion(clOutput[0]);
        if (vsInstall.clVersion.empty())
            continue;

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
