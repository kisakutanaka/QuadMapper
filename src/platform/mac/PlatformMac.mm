#define GL_SILENCE_DEPRECATION
#include "platform/Platform.h"

#import <Foundation/Foundation.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#import <Syphon/SyphonOpenGLClient.h>
#import <Syphon/SyphonOpenGLImage.h>
#import <Syphon/SyphonServerDirectory.h>

namespace {

std::string serverName(NSDictionary* d)
{
    NSString* app = d[SyphonServerDescriptionAppNameKey];
    NSString* name = d[SyphonServerDescriptionNameKey];
    return (name.length ? [NSString stringWithFormat:@"%@ - %@", app, name] : app).UTF8String;
}

class SyphonReceiver : public Receiver {
public:
    SyphonReceiver() { glGenFramebuffers(2, fbo_); }

    ~SyphonReceiver() override
    {
        [client_ stop];
        glDeleteFramebuffers(2, fbo_);
        glDeleteTextures(1, &tex_);
    }

    std::vector<std::string> sources() override
    {
        std::vector<std::string> names;
        for (NSDictionary* d in [SyphonServerDirectory sharedDirectory].servers) names.push_back(serverName(d));
        return names;
    }

    void setSource(const std::string& name) override
    {
        [client_ stop];
        client_ = nil;
        source_ = name;
        retry_ = 0;
        w_ = h_ = 0;
    }

    void update() override
    {
        if (client_ && !client_.isValid) setSource(source_);
        if (!client_ && !source_.empty() && retry_-- <= 0) {
            retry_ = 60;
            for (NSDictionary* d in [SyphonServerDirectory sharedDirectory].servers) {
                if (serverName(d) != source_) continue;
                client_ = [[SyphonOpenGLClient alloc] initWithServerDescription:d
                                                                        context:CGLGetCurrentContext()
                                                                        options:nil
                                                                newFrameHandler:nil];
                break;
            }
        }
        if (!client_ || !client_.hasNewFrame) return;
        SyphonOpenGLImage* image = [client_ newFrameImage];
        if (image) copy(image.textureName, image.textureSize.width, image.textureSize.height);
    }

    unsigned texture() const override { return w_ ? tex_ : 0; }
    int width() const override { return w_; }
    int height() const override { return h_; }

private:
    // Syphon vends GL_TEXTURE_RECTANGLE; copy it into a GL_TEXTURE_2D.
    void copy(GLuint rect, int w, int h)
    {
        if (!tex_ || w != w_ || h != h_) {
            if (!tex_) glGenTextures(1, &tex_);
            glBindTexture(GL_TEXTURE_2D, tex_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            w_ = w;
            h_ = h;
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_[0]);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, rect, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_[1]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    SyphonOpenGLClient* client_ = nil;
    std::string source_;
    int retry_ = 0;
    GLuint fbo_[2] = {};
    GLuint tex_ = 0;
    int w_ = 0, h_ = 0;
};

}

namespace platform {

std::unique_ptr<Receiver> createReceiver() { return std::make_unique<SyphonReceiver>(); }

std::filesystem::path appDir()
{
    return std::filesystem::path(NSBundle.mainBundle.bundlePath.UTF8String).parent_path();
}

std::filesystem::path resourceDir() { return NSBundle.mainBundle.resourcePath.UTF8String; }

}
