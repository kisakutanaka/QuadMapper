#include "platform/Platform.h"

#include <SpoutReceiver.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Hybrid-GPU machines: run on the discrete GPU, where senders usually render.
// Spout cannot share textures across GPUs. Read by the NVIDIA / AMD drivers from the exe.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {

class SpoutRx : public Receiver {
public:
    ~SpoutRx() override
    {
        rx_.ReleaseReceiver();
        glDeleteTextures(1, &tex_);
    }

    std::vector<std::string> sources() override
    {
        std::vector<std::string> names;
        char name[256];
        for (int i = 0; i < rx_.GetSenderCount(); i++)
            if (rx_.GetSender(i, name, sizeof name)) names.push_back(name);
        return names;
    }

    void setSource(const std::string& name) override
    {
        rx_.ReleaseReceiver();
        source_ = name;
        rx_.SetReceiverName(name.c_str());
        w_ = h_ = 0;
    }

    void update() override
    {
        if (source_.empty()) return;
        if (!tex_) glGenTextures(1, &tex_);
        // Spout shares top-down images; invert to the GL convention.
        if (!rx_.ReceiveTexture(tex_, GL_TEXTURE_2D, true) || !rx_.IsUpdated()) return;
        w_ = rx_.GetSenderWidth();
        h_ = rx_.GetSenderHeight();
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    unsigned texture() const override { return w_ ? tex_ : 0; }
    int width() const override { return w_; }
    int height() const override { return h_; }

private:
    SpoutReceiver rx_;
    std::string source_;
    GLuint tex_ = 0;
    int w_ = 0, h_ = 0;
};

}

namespace platform {

std::unique_ptr<Receiver> createReceiver() { return std::make_unique<SpoutRx>(); }

std::filesystem::path appDir()
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return std::filesystem::path(exe).parent_path();
}

std::filesystem::path resourceDir() { return appDir(); }

void keepOnTop(GLFWwindow* w, bool onTop) { glfwSetWindowAttrib(w, GLFW_FLOATING, onTop); }

}
