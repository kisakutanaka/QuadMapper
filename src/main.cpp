#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>

#include "core/Settings.h"
#include "platform/Platform.h"
#include "render/Image.h"
#include "render/Renderer.h"

namespace {

struct OutputWindow {
    GLFWwindow* win = nullptr;
    unsigned vao = 0;
    Canvas canvas;
    bool wasDown = false;
};

Settings s;
GLFWwindow* ctrl = nullptr;
Renderer renderer;
std::vector<OutputWindow> wins;  // wins[i] shows s.outputs[i]
int cur = 0;                     // output edited in the GUI
Vec2* sel = nullptr;  // selected corner or mask vertex of s.outputs[cur]
int dragView = -1;    // 0: preview, 1 + i: output window i

// Test pattern images, sorted by file name.
std::vector<std::filesystem::path> patterns;
unsigned patternTex = 0;
std::string patternLoaded;

bool placing = false;  // while applyOutput moves windows; the saved rect is not overwritten

void applyOutput(int i)
{
    const Output& o = s.outputs[i];
    GLFWwindow* w = wins[i].win;
    placing = true;
    glfwSetWindowAttrib(w, GLFW_DECORATED, !o.borderless);
    platform::keepOnTop(w, o.borderless);
    // Twice: moving onto a display with another scale lets the OS adjust the rect once.
    for (int k = 0; k < 2; k++) glfwSetWindowMonitor(w, nullptr, o.window[0], o.window[1], o.window[2], o.window[3], 0);
    placing = false;
}

void selectOutput(int i)
{
    if (cur != i) sel = nullptr;
    cur = i;
}

// Nearest editable point of o within 10 px of p. vw, vh: view size in pixels.
Vec2* pick(Output& o, Vec2 p, float vw, float vh)
{
    Vec2* best = nullptr;
    float bestD = 10.0f;
    auto test = [&](Vec2& q) {
        const float d = std::hypot((q.x - p.x) * vw, (q.y - p.y) * vh);
        if (d < bestD) {
            bestD = d;
            best = &q;
        }
    };
    for (auto& c : o.corners) test(c);
    for (auto& m : o.masks)
        for (auto& q : m) test(q);
    return best;
}

void pointer(int view, int out, Vec2 p, bool pressed, bool down, float vw, float vh)
{
    if (pressed) {
        Vec2* hit = pick(s.outputs[out], p, vw, vh);
        if (hit) {
            selectOutput(out);
            sel = hit;
        }
        dragView = hit ? view : -1;
    }
    if (dragView != view) return;
    if (down)
        *sel = p;
    else
        dragView = -1;
}

void key(int k, bool shift)
{
    auto& corners = s.outputs[cur].corners;
    if (k == GLFW_KEY_TAB) {
        sel = (sel >= &corners[0] && sel < &corners[3]) ? sel + 1 : &corners[0];
        return;
    }
    const Canvas& c = wins[cur].canvas;
    if (!sel || !c.w || !c.h) return;
    const float step = shift ? 10.0f : 1.0f;
    if (k == GLFW_KEY_LEFT) sel->x -= step / c.w;
    if (k == GLFW_KEY_RIGHT) sel->x += step / c.w;
    if (k == GLFW_KEY_UP) sel->y -= step / c.h;
    if (k == GLFW_KEY_DOWN) sel->y += step / c.h;
}

int indexOf(GLFWwindow* w)
{
    for (size_t i = 0; i < wins.size(); i++)
        if (wins[i].win == w) return (int)i;
    return -1;
}

void onKey(GLFWwindow* w, int k, int, int action, int mods)
{
    if (action == GLFW_RELEASE) return;
    const int i = indexOf(w);
    if (i < 0) return;
    if (k == GLFW_KEY_ESCAPE && s.outputs[i].borderless) {
        s.outputs[i].borderless = false;
        applyOutput(i);
    }
    selectOutput(i);
    key(k, mods & GLFW_MOD_SHIFT);
}

// Only the first output waits for vsync, so two windows do not halve the frame rate.
void setVsync()
{
    for (size_t i = 0; i < wins.size(); i++) {
        glfwMakeContextCurrent(wins[i].win);
        glfwSwapInterval(i == 0 ? 1 : 0);
    }
    glfwMakeContextCurrent(ctrl);
}

void closeWindow(int i)
{
    renderer.destroy(wins[i].canvas);
    glfwDestroyWindow(wins[i].win);
    wins.erase(wins.begin() + i);
}

// Opens or closes windows to match s.outputs and places them. Call with ctrl current.
void syncWindows()
{
    while (wins.size() > s.outputs.size()) closeWindow((int)wins.size() - 1);
    while (wins.size() < s.outputs.size()) {
        char title[32];
        std::snprintf(title, sizeof title, "Output %zu", wins.size() + 1);
        OutputWindow w;
        w.win = glfwCreateWindow(960, 540, title, nullptr, ctrl);
        glfwMakeContextCurrent(w.win);
        w.vao = renderer.makeVao();
        glfwSetKeyCallback(w.win, onKey);
        // Moving or resizing the window by hand updates its saved rect.
        glfwSetWindowPosCallback(w.win, [](GLFWwindow* w, int x, int y) {
            if (const int i = indexOf(w); i >= 0 && !placing) {
                s.outputs[i].window[0] = x;
                s.outputs[i].window[1] = y;
            }
        });
        glfwSetWindowSizeCallback(w.win, [](GLFWwindow* w, int cx, int cy) {
            if (const int i = indexOf(w); i >= 0 && !placing && cx > 0 && cy > 0) {
                s.outputs[i].window[2] = cx;
                s.outputs[i].window[3] = cy;
            }
        });
        wins.push_back(w);
    }
    for (size_t i = 0; i < wins.size(); i++) applyOutput((int)i);
    setVsync();
    cur = std::min(cur, (int)wins.size() - 1);
    sel = nullptr;
    dragView = -1;
}

void removeOutput(int i)
{
    closeWindow(i);
    s.outputs.erase(s.outputs.begin() + i);
    syncWindows();
}

void scanPatterns()
{
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(platform::resourceDir() / "patterns", ec)) {
        std::string ext = e.path().extension().string();
        for (auto& ch : ext) ch = (char)std::tolower((unsigned char)ch);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") patterns.push_back(e.path());
    }
    std::sort(patterns.begin(), patterns.end());
}

// Texture of s.pattern (the first image if missing), loaded on change.
unsigned patternTexture()
{
    if (patterns.empty()) return 0;
    auto it = std::find_if(patterns.begin(), patterns.end(), [](const auto& p) { return p.filename() == s.pattern; });
    if (it == patterns.end()) {
        it = patterns.begin();
        s.pattern = it->filename().string();
    }
    if (patternLoaded != s.pattern) {
        glDeleteTextures(1, &patternTex);
        patternTex = loadTexture(*it);
        patternLoaded = s.pattern;
    }
    return patternTex;
}

// Relative paths are relative to the app's directory, not the working directory.
std::filesystem::path resolve(const std::filesystem::path& p)
{
    return p.is_absolute() ? p : platform::appDir() / p;
}

// Dragging points directly on output window i.
void outputMouse(int i)
{
    OutputWindow& w = wins[i];
    const bool down = glfwGetMouseButton(w.win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    int ww, wh;
    glfwGetWindowSize(w.win, &ww, &wh);
    if (s.guides && ww > 0 && wh > 0 && (down || w.wasDown)) {
        double mx, my;
        glfwGetCursorPos(w.win, &mx, &my);
        pointer(1 + i, i, {float(mx / ww), float(my / wh)}, down && !w.wasDown, down, ww, wh);
    }
    w.wasDown = down;
}

// Finds sel among mask vertices of o.
bool selectedMaskVertex(Output& o, size_t& mi, size_t& vi)
{
    for (mi = 0; mi < o.masks.size(); mi++)
        for (vi = 0; vi < o.masks[mi].size(); vi++)
            if (&o.masks[mi][vi] == sel) return true;
    return false;
}

void gui(Receiver& rx, char* path, size_t pathSize)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("QuadMapper", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    const std::string preview = s.source.empty() ? std::filesystem::path(s.pattern).stem().string() : s.source;
    if (ImGui::BeginCombo("Source", preview.c_str())) {
        for (const auto& p : patterns) {
            if (ImGui::Selectable(p.stem().string().c_str(), s.source.empty() && p.filename() == s.pattern)) {
                s.source.clear();
                s.pattern = p.filename().string();
                rx.setSource("");
            }
        }
        ImGui::Separator();
        for (const auto& n : rx.sources()) {
            if (ImGui::Selectable(n.c_str(), n == s.source)) {
                s.source = n;
                rx.setSource(n);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Text("%dx%d", rx.width(), rx.height());
    ImGui::Checkbox("Guides", &s.guides);

    for (int i = 0; i < (int)s.outputs.size(); i++) {
        char label[32];
        std::snprintf(label, sizeof label, "Output %d", i + 1);
        if (ImGui::RadioButton(label, cur == i)) selectOutput(i);
        ImGui::SameLine();
    }
    if (s.outputs.size() < kMaxOutputs) {
        if (ImGui::Button("Add output")) {
            Output o;
            o.window[0] += 50;
            o.window[1] += 50;
            s.outputs.push_back(o);
            syncWindows();
            cur = (int)s.outputs.size() - 1;
        }
    } else if (ImGui::Button("Remove output")) {
        removeOutput(cur);
    }
    ImGui::Separator();

    Output& o = s.outputs[cur];
    if (ImGui::DragInt4("x y w h", o.window.data())) {
        o.window[2] = std::max(o.window[2], 1);
        o.window[3] = std::max(o.window[3], 1);
        applyOutput(cur);
    }
    if (ImGui::Checkbox("Borderless", &o.borderless)) applyOutput(cur);
    ImGui::SameLine();
    // Copies a display's rect into x y w h; only the numbers are saved.
    if (ImGui::BeginCombo("Fit to", "display")) {
        int n;
        GLFWmonitor** mons = glfwGetMonitors(&n);
        for (int i = 0; i < n; i++) {
            int x, y;
            glfwGetMonitorPos(mons[i], &x, &y);
            const GLFWvidmode* vm = glfwGetVideoMode(mons[i]);
            char label[256];
            std::snprintf(label, sizeof label, "%s (%d, %d) %dx%d", glfwGetMonitorName(mons[i]), x, y, vm->width,
                          vm->height);
            ImGui::PushID(i);
            if (ImGui::Selectable(label)) {
                o.window = {x, y, vm->width, vm->height};
                applyOutput(cur);
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::Combo("Rotation", &o.rotation, "0\0" "90\0" "180\0" "270\0");
    ImGui::Checkbox("Flip H", &o.flipH);
    ImGui::SameLine();
    ImGui::Checkbox("Flip V", &o.flipV);

    if (ImGui::CollapsingHeader("Corners", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* labels[] = {"Top left", "Top right", "Bottom right", "Bottom left"};
        for (int i = 0; i < 4; i++) {
            ImGui::DragFloat2(labels[i], &o.corners[i].x, 0.001f);
            if (ImGui::IsItemActivated()) sel = &o.corners[i];  // then arrow keys move it
        }
        if (ImGui::Button("Reset corners")) o.corners = Output{}.corners;
    }
    if (ImGui::CollapsingHeader("Source crop"))
        ImGui::DragFloat4("x0 y0 x1 y1", o.crop.data(), 0.001f, 0.0f, 1.0f);
    if (ImGui::CollapsingHeader("Edge blend")) {
        ImGui::DragFloat4("L T R B", o.blend.data(), 0.001f, 0.0f, 0.5f);
        ImGui::DragFloat("Gamma", &o.gamma, 0.01f, 0.1f, 4.0f);
    }
    if (ImGui::CollapsingHeader("Brightness")) {
        ImGui::SliderFloat4("TL TR BR BL", o.brightness.data(), 0.0f, 1.0f);
        if (ImGui::Button("Reset brightness")) o.brightness = Output{}.brightness;
    }
    if (ImGui::CollapsingHeader("Masks")) {
        size_t mi, vi;
        const bool onMask = selectedMaskVertex(o, mi, vi);
        if (ImGui::Button("Add mask")) {
            o.masks.push_back({{0.4f, 0.4f}, {0.6f, 0.4f}, {0.6f, 0.6f}, {0.4f, 0.6f}});
            sel = nullptr;
        }
        ImGui::BeginDisabled(!onMask);
        ImGui::SameLine();
        if (ImGui::Button("Insert point")) {
            auto& m = o.masks[mi];
            const Vec2 a = m[vi], b = m[(vi + 1) % m.size()];
            m.insert(m.begin() + vi + 1, {(a.x + b.x) / 2, (a.y + b.y) / 2});
            sel = nullptr;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete point")) {
            auto& m = o.masks[mi];
            if (m.size() > 3)
                m.erase(m.begin() + vi);
            else
                o.masks.erase(o.masks.begin() + mi);
            sel = nullptr;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete mask")) {
            o.masks.erase(o.masks.begin() + mi);
            sel = nullptr;
        }
        ImGui::EndDisabled();
    }

    ImGui::InputText("File", path, pathSize);
    if (ImGui::Button("Save")) save(s, resolve(path));
    ImGui::SameLine();
    if (ImGui::Button("Load") && load(s, resolve(path))) {
        rx.setSource(s.source);
        syncWindows();
    }

    // Preview of the current output; points can be dragged here too.
    const Canvas& c = wins[cur].canvas;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float scale = c.w && c.h ? std::min(avail.x / c.w, avail.y / c.h) : 0.0f;
    const ImVec2 size{c.w * scale, c.h * scale};
    if (size.x >= 1 && size.y >= 1) {
        ImGui::Image((ImTextureID)(intptr_t)c.tex, size, {0, 1}, {1, 0});
        const ImVec2 mn = ImGui::GetItemRectMin();
        const Vec2 p{(io.MousePos.x - mn.x) / size.x, (io.MousePos.y - mn.y) / size.y};
        const bool hovered = ImGui::IsItemHovered();
        if (hovered || dragView == 0)
            pointer(0, cur, p, hovered && ImGui::IsMouseClicked(0), ImGui::IsMouseDown(0), size.x, size.y);
    }

    if (!io.WantTextInput) {
        const std::pair<ImGuiKey, int> keys[] = {
            {ImGuiKey_LeftArrow, GLFW_KEY_LEFT}, {ImGuiKey_RightArrow, GLFW_KEY_RIGHT},
            {ImGuiKey_UpArrow, GLFW_KEY_UP},     {ImGuiKey_DownArrow, GLFW_KEY_DOWN},
            {ImGuiKey_Tab, GLFW_KEY_TAB}};
        for (auto [ik, gk] : keys)
            if (ImGui::IsKeyPressed(ik)) key(gk, io.KeyShift);
    }

    ImGui::End();
}

}

int main()
{
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    ctrl = glfwCreateWindow(640, 800, "QuadMapper", nullptr, nullptr);
    if (!ctrl) return 1;

    const char* kSettings = "QuadMapper.json";
    load(s, resolve(kSettings));
    char path[1024];
    std::snprintf(path, sizeof path, "%s", kSettings);

    glfwMakeContextCurrent(ctrl);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(0);
    renderer.init();
    scanPatterns();
    auto rx = platform::createReceiver();
    rx->setSource(s.source);
    syncWindows();

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(ctrl, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    for (;;) {
        glfwPollEvents();
        // Checked first: quitting the app requests every window to close.
        if (glfwWindowShouldClose(ctrl)) break;

        // Closing an output window removes it; closing the last one quits.
        for (size_t i = 0; i < wins.size(); i++) {
            if (!glfwWindowShouldClose(wins[i].win)) continue;
            if (wins.size() > 1)
                removeOutput((int)i);
            else
                glfwSetWindowShouldClose(ctrl, GLFW_TRUE);
            break;
        }

        rx->update();
        const unsigned source = s.source.empty() ? patternTexture() : rx->texture();
        for (size_t i = 0; i < wins.size(); i++) {
            outputMouse((int)i);
            OutputWindow& w = wins[i];
            int fw, fh;
            glfwGetFramebufferSize(w.win, &fw, &fh);
            if (fw > 0 && fh > 0 && (fw != w.canvas.w || fh != w.canvas.h)) renderer.resize(w.canvas, fw, fh);
            renderer.render(w.canvas, s.outputs[i], source, s.guides, sel);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        gui(*rx, path, sizeof path);
        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(ctrl, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(ctrl);

        for (OutputWindow& w : wins) {
            glfwMakeContextCurrent(w.win);
            if (w.canvas.w) {
                glViewport(0, 0, w.canvas.w, w.canvas.h);
                renderer.drawTexture(w.vao, w.canvas.tex, w.canvas.w, w.canvas.h);
            }
            glfwSwapBuffers(w.win);
        }
        glfwMakeContextCurrent(ctrl);
    }

    rx.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
