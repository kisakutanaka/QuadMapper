#include "core/Settings.h"

#include <fstream>
#include <json.hpp>

using nlohmann::json;

void to_json(json& j, const Vec2& v) { j = {v.x, v.y}; }
void from_json(const json& j, Vec2& v) { v = {j.at(0).get<float>(), j.at(1).get<float>()}; }

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Output, monitor, fullscreen, corners, crop, rotation, flipH, flipV, blend, gamma,
                                                masks)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Settings, source, pattern, outputs)

bool save(const Settings& s, const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path);
    f << json(s).dump(2);
    return f.good();
}

bool load(Settings& s, const std::filesystem::path& path)
{
    std::ifstream f(path);
    if (!f) return false;
    try {
        Settings d = json::parse(f).get<Settings>();
        if (d.outputs.empty()) d.outputs.emplace_back();
        if (d.outputs.size() > kMaxOutputs) d.outputs.resize(kMaxOutputs);
        s = d;
        return true;
    } catch (const json::exception&) {
        return false;
    }
}
