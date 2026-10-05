// Test Syphon server publishing an animated timecode pattern.
// Usage: SyphonTestSender [width height]   (default 1920 1080)
#define GL_SILENCE_DEPRECATION
#import <Foundation/Foundation.h>
#import <OpenGL/gl3.h>
#import <Syphon/SyphonOpenGLServer.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

namespace {

const char* kVert = R"(#version 330 core
const vec2 p[4] = vec2[4](vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
void main() { gl_Position = vec4(p[gl_VertexID], 0.0, 1.0); })";

const char* kFrag = R"(#version 330 core
uniform vec2 size;
uniform float time;
uniform int digits[8];  // HHMMSSFF
out vec4 color;

const int kSeg[10] = int[10](0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F);

float seg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return step(length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0)), 0.12);
}

// Seven-segment digit in a 1 x 2 cell, origin top-left.
float digit(vec2 p, int d) {
    int m = kSeg[d];
    float v = 0.0;
    if ((m & 1) != 0) v = max(v, seg(p, vec2(0, 0), vec2(1, 0)));
    if ((m & 2) != 0) v = max(v, seg(p, vec2(1, 0), vec2(1, 1)));
    if ((m & 4) != 0) v = max(v, seg(p, vec2(1, 1), vec2(1, 2)));
    if ((m & 8) != 0) v = max(v, seg(p, vec2(0, 2), vec2(1, 2)));
    if ((m & 16) != 0) v = max(v, seg(p, vec2(0, 1), vec2(0, 2)));
    if ((m & 32) != 0) v = max(v, seg(p, vec2(0, 0), vec2(0, 1)));
    if ((m & 64) != 0) v = max(v, seg(p, vec2(0, 1), vec2(1, 1)));
    return v;
}

// HH:MM:SS:FF, 13.6 units wide and 2 high.
float timecode(vec2 p) {
    float x = 0.0, v = 0.0;
    for (int i = 0; i < 8; i++) {
        if (i == 2 || i == 4 || i == 6) {
            v = max(v, step(length(p - vec2(x + 0.35, 0.6)), 0.13));
            v = max(v, step(length(p - vec2(x + 0.35, 1.4)), 0.13));
            x += 0.7;
        }
        v = max(v, digit(p - vec2(x, 0.0), digits[i]));
        x += 1.5;
    }
    return v;
}

void main() {
    vec2 s = vec2(gl_FragCoord.x, size.y - gl_FragCoord.y);  // pixels, origin top-left
    vec2 g = abs(fract(s / 120.0 - 0.5) - 0.5) * 120.0;
    vec3 c = min(g.x, g.y) < 1.0 ? vec3(0.45) : vec3(0.12);
    if (abs(s.x - fract(time / 2.0) * size.x) < size.x * 0.01) c = vec3(1.0, 0.5, 0.0);

    float unit = size.y * 0.075;
    vec2 p = (s - (size - vec2(13.6, 2.0) * unit) * 0.5) / unit;
    if (all(greaterThan(p, vec2(-0.5))) && all(lessThan(p, vec2(14.1, 2.5)))) c = vec3(0.0);
    color = vec4(mix(c, vec3(1.0), timecode(p)), 1.0);
})";

GLuint compile(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader: %s\n", log);
    }
    return s;
}

}

int main(int argc, char** argv)
{
    const int w = argc > 2 ? std::atoi(argv[1]) : 1920;
    const int h = argc > 2 ? std::atoi(argv[2]) : 1080;
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    GLFWwindow* win = glfwCreateWindow(w / 2, h / 2, "Syphon Test Sender", nullptr, nullptr);
    if (!win) return 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, kVert));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, kFrag));
    glLinkProgram(prog);
    GLuint vao, tex, fbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    SyphonOpenGLServer* server = [[SyphonOpenGLServer alloc] initWithName:@"Timecode"
                                                                   context:CGLGetCurrentContext()
                                                                   options:nil];
    while (!glfwWindowShouldClose(win)) {
        @autoreleasepool {
            glfwPollEvents();
            const double t = glfwGetTime();
            const long frame = (long)(t * 60.0), sec = frame / 60;
            const int n[4] = {int(sec / 3600 % 100), int(sec / 60 % 60), int(sec % 60), int(frame % 60)};
            const GLint digits[8] = {n[0] / 10, n[0] % 10, n[1] / 10, n[1] % 10,
                                     n[2] / 10, n[2] % 10, n[3] / 10, n[3] % 10};

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, w, h);
            glUseProgram(prog);
            glBindVertexArray(vao);
            glUniform2f(glGetUniformLocation(prog, "size"), w, h);
            glUniform1f(glGetUniformLocation(prog, "time"), (float)t);
            glUniform1iv(glGetUniformLocation(prog, "digits"), 8, digits);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            [server publishFrameTexture:tex
                          textureTarget:GL_TEXTURE_2D
                            imageRegion:NSMakeRect(0, 0, w, h)
                      textureDimensions:NSMakeSize(w, h)
                                flipped:NO];

            int fw, fh;
            glfwGetFramebufferSize(win, &fw, &fh);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, w, h, 0, 0, fw, fh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glfwSwapBuffers(win);
        }
    }
    [server stop];
    glfwTerminate();
    return 0;
}
