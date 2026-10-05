#pragma once
#include <vector>

#include "core/Settings.h"

// Render target for one output. Its texture is shared with the output window's context.
struct Canvas {
    unsigned fbo = 0, tex = 0, rbo = 0;
    int w = 0, h = 0;
};

// Renders mapped outputs into canvases. GL objects other than VAOs and FBOs
// are shared, so a canvas can be displayed from another context.
class Renderer {
public:
    void init();
    unsigned makeVao() const;  // one per context
    void resize(Canvas& c, int w, int h) const;
    void destroy(Canvas& c) const;

    // source: GL_TEXTURE_2D with origin at bottom-left (0 draws nothing)
    void render(const Canvas& c, const Output& o, unsigned source, bool guides, const Vec2* selected) const;
    // Draws tex over the current viewport of size w x h.
    void drawTexture(unsigned vao, unsigned tex, int w, int h) const;

private:
    void draw(const std::vector<Vec2>& pts, unsigned mode) const;
    void setMapUniforms(const Output& o, int w, int h) const;

    unsigned mapProg_ = 0, solidProg_ = 0;
    unsigned vbo_ = 0, vao_ = 0;
};
