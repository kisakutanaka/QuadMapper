#include "core/Settings.h"

#include <fstream>
#include <json.hpp>

using nlohmann::json;

void to_json(json& j, const Vec2& v) { j = {v.x, v.y}; }
void from_json(const json& j, Vec2& v) { v = {j.at(0).get<float>(), j.at(1).get<float>()}; }

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Output, window, borderless, corners, crop, rotation, flipH, flipV,
                                                  blend, gamma, brightness, masks)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Settings, source, pattern, guides, outputs)

namespace {

// Missing or mistyped keys keep their defaults, so files from older versions still load.
template <class T>
void read(const json& j, const char* key, T& v)
{
    const auto it = j.find(key);
    if (it == j.end()) return;
    try {
        v = it->get<T>();
    } catch (const json::exception&) {
    }
}

}

void from_json(const json& j, Output& o)
{
    read(j, "window", o.window);
    read(j, "borderless", o.borderless);
    read(j, "corners", o.corners);
    read(j, "crop", o.crop);
    read(j, "rotation", o.rotation);
    read(j, "flipH", o.flipH);
    read(j, "flipV", o.flipV);
    read(j, "blend", o.blend);
    read(j, "gamma", o.gamma);
    read(j, "brightness", o.brightness);
    read(j, "masks", o.masks);
}

void from_json(const json& j, Settings& s)
{
    read(j, "source", s.source);
    read(j, "pattern", s.pattern);
    read(j, "guides", s.guides);
    read(j, "outputs", s.outputs);
}

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
