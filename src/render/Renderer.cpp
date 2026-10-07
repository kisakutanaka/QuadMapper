#include "render/Renderer.h"

#include <glad/gl.h>

#include <cstdio>

namespace {

// Positions are normalized with the origin at top-left.
const char* kVert = R"(#version 330 core
layout(location = 0) in vec2 pos;
void main() {
    gl_Position = vec4(pos.x * 2.0 - 1.0, 1.0 - pos.y * 2.0, 0.0, 1.0);
    gl_PointSize = 9.0;
})";

const char* kMapFrag = R"(#version 330 core
uniform mat3 invH;   // output -> quad space
uniform vec2 size;   // viewport size
uniform vec4 crop;   // source rect x0 y0 x1 y1
uniform int rotation;  // quarter turns clockwise
uniform bvec2 flip;
uniform vec4 blend;  // edge width L T R B
uniform float gamma;
uniform vec4 brightness;  // at corners TL TR BR BL
uniform sampler2D tex;
out vec4 color;
void main() {
    vec3 q = invH * vec3(gl_FragCoord.x / size.x, 1.0 - gl_FragCoord.y / size.y, 1.0);
    vec2 uv = q.xy / q.z;
    if (q.z <= 0.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) discard;
    vec2 t = vec2(flip.x ? 1.0 - uv.x : uv.x, flip.y ? 1.0 - uv.y : uv.y);
    for (int i = 0; i < rotation; i++) t = vec2(t.y, 1.0 - t.x);
    vec2 s = mix(crop.xy, crop.zw, t);
    vec3 c = texture(tex, vec2(s.x, 1.0 - s.y)).rgb;
    // Linear ramp in light, encoded for the projector gamma.
    vec4 e = clamp(vec4(uv, 1.0 - uv) / max(blend, 1e-6), 0.0, 1.0);
    float b = mix(mix(brightness.x, brightness.y, uv.x), mix(brightness.w, brightness.z, uv.x), uv.y);
    color = vec4(c * b * pow(e.x * e.y * e.z * e.w, 1.0 / gamma), 1.0);
})";

const char* kSolidFrag = R"(#version 330 core
uniform vec4 col;
out vec4 color;
void main() { color = col; })";

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

GLuint program(const char* vs, const char* fs)
{
    GLuint p = glCreateProgram();
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

const std::vector<Vec2> kUnit{{0, 0}, {1, 0}, {1, 1}, {0, 1}};

}

void Renderer::init()
{
    mapProg_ = program(kVert, kMapFrag);
    solidProg_ = program(kVert, kSolidFrag);
    glGenBuffers(1, &vbo_);
    vao_ = makeVao();
}

unsigned Renderer::makeVao() const
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), nullptr);
    return vao;
}

void Renderer::resize(Canvas& c, int w, int h) const
{
    if (!c.fbo) {
        glGenFramebuffers(1, &c.fbo);
        glGenTextures(1, &c.tex);
        glGenRenderbuffers(1, &c.rbo);
    }
    c.w = w;
    c.h = h;
    glBindTexture(GL_TEXTURE_2D, c.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindRenderbuffer(GL_RENDERBUFFER, c.rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, c.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c.tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, c.rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::destroy(Canvas& c) const
{
    glDeleteFramebuffers(1, &c.fbo);
    glDeleteTextures(1, &c.tex);
    glDeleteRenderbuffers(1, &c.rbo);
    c = {};
}

void Renderer::draw(const std::vector<Vec2>& pts, unsigned mode) const
{
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(Vec2), pts.data(), GL_STREAM_DRAW);
    glDrawArrays(mode, 0, (GLsizei)pts.size());
}

void Renderer::setMapUniforms(const Output& o, int w, int h) const
{
    glUseProgram(mapProg_);
    glUniformMatrix3fv(glGetUniformLocation(mapProg_, "invH"), 1, GL_TRUE, inverse(squareToQuad(o.corners)).data());
    glUniform2f(glGetUniformLocation(mapProg_, "size"), (float)w, (float)h);
    glUniform4fv(glGetUniformLocation(mapProg_, "crop"), 1, o.crop.data());
    glUniform1i(glGetUniformLocation(mapProg_, "rotation"), o.rotation & 3);
    glUniform2i(glGetUniformLocation(mapProg_, "flip"), o.flipH, o.flipV);
    glUniform4fv(glGetUniformLocation(mapProg_, "blend"), 1, o.blend.data());
    glUniform1f(glGetUniformLocation(mapProg_, "gamma"), o.gamma);
    glUniform4fv(glGetUniformLocation(mapProg_, "brightness"), 1, o.brightness.data());
    glUniform1i(glGetUniformLocation(mapProg_, "tex"), 0);
}

void Renderer::render(const Canvas& c, const Output& o, unsigned source, bool guides, const Vec2* selected) const
{
    if (!c.w || !c.h) return;
    glBindFramebuffer(GL_FRAMEBUFFER, c.fbo);
    glViewport(0, 0, c.w, c.h);
    glClearColor(0, 0, 0, 1);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBindVertexArray(vao_);

    // Masks: even-odd fill into the stencil, so concave polygons work as-is.
    glEnable(GL_STENCIL_TEST);
    glUseProgram(solidProg_);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilFunc(GL_ALWAYS, 0, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
    for (const auto& m : o.masks) draw(m, GL_TRIANGLE_FAN);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 0, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    if (source) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, source);
        setMapUniforms(o, c.w, c.h);
        draw(kUnit, GL_TRIANGLE_FAN);
    }
    glDisable(GL_STENCIL_TEST);

    if (guides) {
        glEnable(GL_PROGRAM_POINT_SIZE);
        glUseProgram(solidProg_);
        const GLint col = glGetUniformLocation(solidProg_, "col");
        auto outline = [&](const std::vector<Vec2>& pts, float r, float g, float b) {
            glUniform4f(col, r, g, b, 1);
            draw(pts, GL_LINE_LOOP);
            draw(pts, GL_POINTS);
        };
        outline({o.corners.begin(), o.corners.end()}, 0, 1, 0);
        for (const auto& m : o.masks) outline(m, 1, 0, 0);
        if (selected) {
            glUniform4f(col, 1, 1, 0, 1);
            draw({*selected}, GL_POINTS);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFlush();
}

void Renderer::drawTexture(unsigned vao, unsigned tex, int w, int h) const
{
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    setMapUniforms(Output{}, w, h);  // identity
    draw(kUnit, GL_TRIANGLE_FAN);
}
