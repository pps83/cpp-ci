#pragma once
#include <map>
#include <list>
#include <vector>
#include <string>
#include <string_view>

class VsSolution
{
public:
    bool loadSlnFile(const char* slnFile);

private:
    struct PlatformConfig
    {
        std::string_view activeCfg;
        std::string_view buildCfg;
    };
    typedef std::map<std::string_view, std::map<std::string_view, PlatformConfig>> ConfigurationPlatforms;

    struct VcProject
    {
        std::string_view name;
        std::string_view path;
        std::string_view guid; // comes from sln file
        std::string_view ProjectGuid; // comes from .vcxproj file (only if different from guid)
        std::string_view vcxproj; // actual vcxproj file contents
        ConfigurationPlatforms configs; // from sln file
        std::map<std::string_view, std::string_view> refs; // ProjectReference (Include => Project)
    };

    // compare strings in such a way so that "ABCDEF" are equal to "abcdef"
    struct HexLess
    {
        bool operator()(const std::string_view& a, const std::string_view& b) const noexcept;
    };

    std::vector<VcProject> slnProjects;
    std::vector<VcProject*> projectsOrder;
    std::map<std::string_view, VcProject*, HexLess> guidProjects;

    std::string slnFilename;
    std::string slnDir; // folder where sln file located
    std::list<std::string> files; // all string_view objects of VsSolution refer to strings within loaded files

    void clear();

    void buildVcProjects(std::string_view projects);
    void buildProjectConfigurationPlatforms(std::string_view sln);
    void loadVcxproj(VcProject& vcproj);
    void calculateProjectsOrder();
    bool existsInProjectsOrder(const VcProject* proj) const;
};
