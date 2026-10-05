#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "core/Homography.h"

// All coordinates are normalized (0..1) with the origin at top-left.
struct Output {
    int monitor = 0;
    bool fullscreen = false;
    std::array<Vec2, 4> corners{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};  // output quad TL TR BR BL
    std::array<float, 4> crop{0, 0, 1, 1};                          // source rect x0 y0 x1 y1
    int rotation = 0;                                               // source quarter turns clockwise
    bool flipH = false, flipV = false;                              // applied in output orientation
    std::array<float, 4> blend{0, 0, 0, 0};                         // edge width L T R B (quad space)
    float gamma = 2.2f;
    std::vector<std::vector<Vec2>> masks;  // output-space polygons to hide
};

constexpr size_t kMaxOutputs = 2;

struct Settings {
    std::string source;   // empty: test pattern
    std::string pattern;  // test pattern file name
    std::vector<Output> outputs{Output{}};  // 1..kMaxOutputs
};

bool save(const Settings& s, const std::filesystem::path& path);
bool load(Settings& s, const std::filesystem::path& path);
