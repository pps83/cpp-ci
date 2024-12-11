#include "VsSolution.h"
#include "util.h"

static constexpr std::string_view g_project = "Project(\"{";
static constexpr std::string_view g_endProject = "EndProject";
static constexpr std::string_view g_vcProjectGuid = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}";
static constexpr std::string_view g_vcProj = "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"";
static constexpr std::string_view g_div = "\", \"";
static constexpr std::string_view g_ProjectGuid = "<ProjectGuid>";
static constexpr std::string_view g_ProjectReference = "<ProjectReference";
static constexpr std::string_view g_Include = "Include=\"";
static constexpr std::string_view g_Project = "<Project>";
static constexpr std::string_view g_ProjectEnd = "</Project>";

static bool ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static std::string_view trim(std::string_view s)
{
    while (!s.empty() && ws(s[0]))
        s.remove_prefix(1);
    while (!s.empty() && ws(s[s.size() - 1]))
        s.remove_suffix(1);
    return s;
}

static std::string_view skipBom(std::string_view s)
{
    if (s.starts_with("\xEF\xBB\xBF\r\n"))
        s = s.substr(5);
    else if (s.starts_with("\xEF\xBB\xBF\n"))
        s = s.substr(4);
    else if (s.starts_with("\xEF\xBB\xBF"))
        s = s.substr(3);
    return s;
}

static char hexUp(char c)
{
    return c >= 'a' && c <= 'f' ? ('A' + c - 'a') : c;
}

static int hexcmp(const std::string_view& a, const std::string_view& b, size_t sz)
{
    int ret = 0;
    for (size_t i = 0; i < sz && !ret; ++i)
    {
        char ca = hexUp(a[i]), cb = hexUp(b[i]);
        ret = ca < cb ? -1 : (ca > cb ? 1 : 0);
    }
    return ret;
}

static int hexcmp(const std::string_view& a, const std::string_view& b)
{
    const int r = hexcmp(a, b, std::min(a.size(), b.size()));
    if (r != 0)
        return r;
    if (a.size() < b.size())
        return -1;
    if (a.size() > b.size())
        return 1;
    return 0;
}

bool VsSolution::HexLess::operator()(const std::string_view& a, const std::string_view& b) const noexcept
{
    return hexcmp(a, b) == -1;
}

static std::string_view getLine(std::string_view& str)
{
    std::string_view ret;
    auto pos = str.find_first_of("\r\n");
    if (pos != std::string::npos)
    {
        ret = str.substr(0, pos);
        if (str[pos] == '\r' && str[pos + 1] == '\n')
            str = str.substr(pos + 2);
        else
            str = str.substr(pos + 1);
    }
    else
    {
        ret = str;
        str = {};
    }
    return ret;
}

static std::string_view getProjects(std::string_view sln)
{
    auto pos = sln.find(g_project);
    if (pos != std::string::npos && pos > 0 && (sln[pos - 1] == '\r' || sln[pos - 1] == '\n'))
    {
        auto end = sln.rfind(g_endProject, sln.size() - pos);
        if (end != std::string::npos && sln.size() >= end + 2)
        {
            end += g_endProject.size();
            if (sln[end] == '\r')
                end++;
            if (sln[end] == '\n')
                end++;
            return sln.substr(pos, end - pos);
        }
    }
    return {};
}

bool VsSolution::loadSlnFile(const char* slnFile)
{
    clear();

    slnFilename = slnFile;
    auto p = slnFilename.find_last_of("\\/");
    if (p != std::string::npos)
        slnDir = slnFilename.substr(0, p + 1);
    else
        slnDir = {};

    std::string& slnData = files.emplace_back();
    std::string_view sln;
    if (!readFile(slnFile, slnData))
        return false;
    sln = skipBom(slnData);

    buildVcProjects(getProjects(sln));
    buildProjectConfigurationPlatforms(sln);
    for (auto& proj : slnProjects)
        loadVcxproj(proj);
    calculateProjectsOrder();
    return true;
}

void VsSolution::clear()
{
    slnFilename.clear();
    slnDir.clear();
    slnProjects.clear();
    guidProjects.clear();
    projectsOrder.clear();
    files.clear();
}

void VsSolution::buildVcProjects(std::string_view projects)
{
    while (!projects.empty())
    {
        auto line = getLine(projects);
        if (line.starts_with(g_vcProj) && line.ends_with("\""))
        {
            line = line.substr(g_vcProj.size());
            auto p0 = line.find(g_div);
            auto p1 = line.find(g_div, p0 + g_div.size());
            if (p0 != std::string::npos && p1 != std::string::npos)
            {
                VcProject proj;
                proj.name = line.substr(0, p0);
                proj.path = line.substr(p0 + g_div.size(), p1 - (p0 + g_div.size()));
                proj.guid = line.substr(p1 + g_div.size(), line.size() - (p1 + g_div.size()) - 1);
                slnProjects.push_back(proj);
            }
        }
    }
    for (auto& proj : slnProjects)
        guidProjects[proj.guid] = &proj;
}

void VsSolution::buildProjectConfigurationPlatforms(std::string_view sln)
{
    auto pos = sln.find("GlobalSection(ProjectConfigurationPlatforms)");
    auto end = sln.find("EndGlobalSection", pos);
    if (pos != std::string::npos && end != std::string::npos)
    {
        pos = sln.find("\n", pos);
        if (pos != std::string::npos)
            pos++;
        end = sln.rfind("\n", end);
        if (end != std::string::npos && sln[end - 1] == '\r')
            end--;
    }
    if (pos != std::string::npos && end != std::string::npos)
    {
        auto cfgs = sln.substr(pos, end - pos);
        while (!cfgs.empty())
        {
            auto line = trim(getLine(cfgs));
            auto p0 = line.find('.');
            auto p1 = line.find('.', p0 + 1);
            auto p2 = line.find('=', p1 + 1);
            if (p0 != std::string::npos && p1 != std::string::npos && p2 != std::string::npos)
            {
                auto guid = line.substr(0, p0);
                auto cfg = line.substr(p0 + 1, p1 - p0 - 1);
                auto type = trim(line.substr(p1 + 1, p2 - p1 - 1));
                auto pcfg = trim(line.substr(p2 + 1));

                auto sp = cfg.find('|');
                auto it = guidProjects.find(guid);
                if (sp != std::string::npos && it != guidProjects.end())
                {
                    auto s_conf = trim(cfg.substr(0, sp));
                    auto s_plat = trim(cfg.substr(sp + 1));
                    if (type == "Build.0")
                        it->second->configs[s_plat][s_conf].buildCfg = pcfg;
                    else if (type == "ActiveCfg")
                        it->second->configs[s_plat][s_conf].activeCfg = pcfg;
                }
            }
        }
    }
}

void VsSolution::loadVcxproj(VcProject& vcproj)
{
    {
        std::string vcxprojData;
        std::string path = slnDir;
        path.append(vcproj.path);
        if (!readFile(path, vcxprojData))
            return;
        vcproj.vcxproj = skipBom(vcxprojData);
        files.emplace_back().swap(vcxprojData);
    }

    auto p = vcproj.vcxproj.find(g_ProjectGuid);
    if (p != std::string::npos)
    {
        auto e = vcproj.vcxproj.find("</ProjectGuid>");
        auto ProjectGuid = vcproj.vcxproj.substr(p + g_ProjectGuid.size(), e - (p + g_ProjectGuid.size()));
        assert(hexcmp(ProjectGuid, vcproj.guid) == 0); // check that ProjectGuid and guid are the same
        if (hexcmp(ProjectGuid, vcproj.guid))
        {
            vcproj.ProjectGuid = ProjectGuid; // update ProjectGuid only if different from guid
            // TODO: should ProjectGuid be inserted into guidProjects (possibly different) guid for this vcproj?
            guidProjects[ProjectGuid] = &vcproj;
        }
    }

    // <ProjectReference Include="http.vcxproj">
    //   <Project>{83dfc3ff-49f8-4a6d-8c6a-a1126a709387}</Project>
    // </ProjectReference>

    p = vcproj.vcxproj.find(g_ProjectReference);
    while (p != std::string::npos)
    {
        p = vcproj.vcxproj.find(g_Include, p + g_ProjectReference.size());
        if (p == std::string::npos)
            break;
        p += g_Include.size();
        auto e = vcproj.vcxproj.find('"', p);
        if (e == std::string::npos)
            break;
        auto name = vcproj.vcxproj.substr(p, e - p);
        p = vcproj.vcxproj.find(g_Project, e);
        if (p == std::string::npos)
            break;
        p += g_Project.size();
        e = vcproj.vcxproj.find(g_ProjectEnd, p);
        if (e == std::string::npos)
            break;
        auto guid = vcproj.vcxproj.substr(p, e - p);
        vcproj.refs[name] = guid;

        p = vcproj.vcxproj.find(g_ProjectReference, e);
    }
}

// TODO: possibly should use topological sort from https://www.boost.org/doc/libs/1_39_0/libs/graph/doc/file_dependency_example.html
void VsSolution::calculateProjectsOrder()
{
    while (projectsOrder.size() < slnProjects.size())
    {
        for (auto& proj : slnProjects)
        {
            if (existsInProjectsOrder(guidProjects[proj.guid]))
                continue;
            bool missing = false;
            for (const auto& ref : proj.refs)
            {
                if (!existsInProjectsOrder(guidProjects[ref.second]))
                {
                    missing = true;
                    break;
                }
            }
            if (!missing)
                projectsOrder.push_back(&proj);
        }
    }
    assert(projectsOrder.size() == slnProjects.size());
}

bool VsSolution::existsInProjectsOrder(const VcProject* proj) const
{
    for (const auto& p : projectsOrder)
    {
        if (p == proj)
            return true;
    }
    return false;
}
