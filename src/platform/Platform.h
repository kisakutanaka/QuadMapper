#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Receives a texture shared by another application (Syphon / Spout).
class Receiver {
public:
    virtual ~Receiver() = default;
    virtual std::vector<std::string> sources() = 0;
    // Empty name disconnects. Reconnects automatically when the source appears.
    virtual void setSource(const std::string& name) = 0;
    virtual void update() = 0;
    // GL_TEXTURE_2D with origin at bottom-left, 0 while nothing is received.
    virtual unsigned texture() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};

namespace platform {
// Call with the GL context that will use the texture current.
std::unique_ptr<Receiver> createReceiver();
// Directory the app sits in (mac: the folder containing the .app, win: exe directory).
std::filesystem::path appDir();
// Bundled read-only data (mac: .app/Contents/Resources, win: exe directory).
std::filesystem::path resourceDir();
}
