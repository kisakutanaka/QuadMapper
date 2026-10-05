#pragma once
#include <filesystem>

// Loads a PNG/JPEG into a new GL_TEXTURE_2D with origin at bottom-left. 0 on failure.
unsigned loadTexture(const std::filesystem::path& path);
