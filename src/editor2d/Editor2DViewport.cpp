// ============================================================================
// GameVoid Engine — 2D Editor Viewport Implementation
// ============================================================================
#ifdef GV_HAS_GLFW

#include "editor2d/Editor2DViewport.h"
#include "renderer/Renderer.h"
#include "core/Window.h"
#include "core/GLDefs.h"
#include "core/GameObject.h"

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

namespace gv {

// ── Log (shared with EditorUI) ─────────────────────────────────────────────
void Editor2DViewport::PushLog(const std::string& msg) {
    // Forward to EditorUI's static log (declared in EditorUI.cpp)
    extern void EditorUI_PushLog2D(const std::string&);
    EditorUI_PushLog2D(msg);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

bool Editor2DViewport::Init(Window* window, OpenGLRenderer* renderer) {
    m_Window   = window;
    m_Renderer = renderer;

    CreateFBO(800, 600);

    // Create default 2D scene with a background
    m_Scene.SetName("2D Scene");
    m_Camera.Reset();

    m_Initialised = true;
    return true;
}

void Editor2DViewport::Shutdown() {
    DestroyFBO();
    m_Initialised = false;
}

// ── FBO management ─────────────────────────────────────────────────────────

void Editor2DViewport::CreateFBO(u32 w, u32 h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    m_FBOW = w;
    m_FBOH = h;

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    // Color texture
    glGenTextures(1, &m_FBOColor);
    glBindTexture(GL_TEXTURE_2D, m_FBOColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_FBOColor, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &m_FBODepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_FBODepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_FBODepth);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Editor2DViewport::DestroyFBO() {
    if (m_FBO)      { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    if (m_FBOColor) { glDeleteTextures(1, &m_FBOColor); m_FBOColor = 0; }
    if (m_FBODepth) { glDeleteRenderbuffers(1, &m_FBODepth); m_FBODepth = 0; }
}

void Editor2DViewport::ResizeFBO(u32 w, u32 h) {
    if (w == m_FBOW && h == m_FBOH) return;
    DestroyFBO();
    CreateFBO(w, h);
}

// ── Main viewport render ──────────────────────────────────────────────────

void Editor2DViewport::RenderViewport(f32 dt, f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    m_VpX = vpX; m_VpY = vpY; m_VpW = vpW; m_VpH = vpH;

    // Resize FBO if needed
    u32 fw = static_cast<u32>(vpW);
    u32 fh = static_cast<u32>(vpH);
    if (fw < 1) fw = 1;
    if (fh < 1) fh = 1;
    ResizeFBO(fw, fh);

    // Handle input
    HandleInput(dt, vpX, vpY, vpW, vpH);

    // Update camera
    m_Camera.Update(dt);

    // Update scene
    m_Scene.Update(dt);

    // ── Render to FBO ──────────────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_FBOW, m_FBOH);

    // Clear with dark blue-grey background
    glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable blending for 2D sprites
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    // Draw grid
    if (showGrid) {
        RenderGrid2D(vpW, vpH);
    }

    // Draw sprites (sorted by layer)
    RenderSprites();

    // Draw collider outlines
    RenderColliders2D(vpW, vpH);

    // Draw tilemap overlay if in tilemap mode
    if (tilemapMode) {
        RenderTilemapGrid(vpW, vpH);
    }

    // Draw gizmo for selected object
    if (m_Selected) {
        RenderGizmo2D(vpW, vpH);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ── Draw FBO texture in ImGui ──────────────────────────────────────────
    ImGui::SetCursorScreenPos(ImVec2(vpX, vpY));
    ImGui::Image(static_cast<ImTextureID>(m_FBOColor),
                 ImVec2(vpW, vpH), ImVec2(0, 1), ImVec2(1, 0));

    // ── Overlay: coordinate readout ────────────────────────────────────────
    ImVec2 mousePos = ImGui::GetMousePos();
    f32 localMX = mousePos.x - vpX;
    f32 localMY = mousePos.y - vpY;
    Vec2 worldMouse = m_Camera.ScreenToWorld(localMX, localMY, vpW, vpH);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Bottom-left: world coordinates
    char coordBuf[64];
    std::snprintf(coordBuf, sizeof(coordBuf), "X: %.1f  Y: %.1f  Zoom: %.1fx",
                  worldMouse.x, worldMouse.y, m_Camera.GetZoom());
    ImVec2 coordPos(vpX + 8, vpY + vpH - 24);
    ImVec2 coordSz = ImGui::CalcTextSize(coordBuf);
    dl->AddRectFilled(ImVec2(coordPos.x - 2, coordPos.y - 2),
                      ImVec2(coordPos.x + coordSz.x + 6, coordPos.y + coordSz.y + 4),
                      IM_COL32(20, 20, 30, 200), 3.0f);
    dl->AddText(coordPos, IM_COL32(180, 220, 255, 255), coordBuf);

    // Top-left: mode indicator
    const char* modeLabel = "2D Viewport";
    ImVec2 modePos(vpX + 8, vpY + 6);
    dl->AddText(modePos, IM_COL32(100, 200, 255, 200), modeLabel);
}

// ── Grid rendering ─────────────────────────────────────────────────────────

void Editor2DViewport::RenderGrid2D(f32 vpW, f32 vpH) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    f32 zoom = m_Camera.GetZoom();
    Vec2 camPos = m_Camera.GetPosition();

    // Adaptive grid spacing
    f32 baseStep = gridSize;
    while (baseStep * zoom * vpH < 20.0f) baseStep *= 2.0f;  // double until visible
    while (baseStep * zoom * vpH > 200.0f) baseStep *= 0.5f; // halve if too large

    // Compute visible range in world coords
    f32 aspect = vpW / vpH;
    f32 halfH = 1.0f / zoom;
    f32 halfW = halfH * aspect;

    f32 worldLeft   = camPos.x - halfW;
    f32 worldRight  = camPos.x + halfW;
    f32 worldBottom = camPos.y - halfH;
    f32 worldTop    = camPos.y + halfH;

    // Snap range to grid
    f32 startX = std::floor(worldLeft / baseStep) * baseStep;
    f32 startY = std::floor(worldBottom / baseStep) * baseStep;

    // Draw vertical lines
    for (f32 x = startX; x <= worldRight; x += baseStep) {
        Vec2 top = m_Camera.WorldToScreen({ x, worldTop }, vpW, vpH);
        Vec2 bot = m_Camera.WorldToScreen({ x, worldBottom }, vpW, vpH);

        ImU32 col;
        if (std::fabs(x) < baseStep * 0.01f) {
            // Y-axis (green)
            col = IM_COL32((int)(gridAxisY.x * 255), (int)(gridAxisY.y * 255),
                           (int)(gridAxisY.z * 255), (int)(gridAxisY.w * 255));
        } else {
            col = IM_COL32((int)(gridColor.x * 255), (int)(gridColor.y * 255),
                           (int)(gridColor.z * 255), (int)(gridColor.w * 255));
        }

        dl->AddLine(ImVec2(m_VpX + top.x, m_VpY + top.y),
                     ImVec2(m_VpX + bot.x, m_VpY + bot.y), col, 1.0f);
    }

    // Draw horizontal lines
    for (f32 y = startY; y <= worldTop; y += baseStep) {
        Vec2 left  = m_Camera.WorldToScreen({ worldLeft, y }, vpW, vpH);
        Vec2 right = m_Camera.WorldToScreen({ worldRight, y }, vpW, vpH);

        ImU32 col;
        if (std::fabs(y) < baseStep * 0.01f) {
            // X-axis (red)
            col = IM_COL32((int)(gridAxisX.x * 255), (int)(gridAxisX.y * 255),
                           (int)(gridAxisX.z * 255), (int)(gridAxisX.w * 255));
        } else {
            col = IM_COL32((int)(gridColor.x * 255), (int)(gridColor.y * 255),
                           (int)(gridColor.z * 255), (int)(gridColor.w * 255));
        }

        dl->AddLine(ImVec2(m_VpX + left.x,  m_VpY + left.y),
                     ImVec2(m_VpX + right.x, m_VpY + right.y), col, 1.0f);
    }

    // Origin marker
    Vec2 origin = m_Camera.WorldToScreen({ 0, 0 }, vpW, vpH);
    dl->AddCircleFilled(ImVec2(m_VpX + origin.x, m_VpY + origin.y), 4.0f,
                        IM_COL32(255, 255, 255, 120));
}

// ── Sprite rendering ───────────────────────────────────────────────────────

void Editor2DViewport::RenderSprites() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    auto sorted = m_Scene.GetSortedRenderList();

    for (auto* obj : sorted) {
        auto* spr = obj->GetComponent<SpriteComponent>();
        if (!spr) continue;

        auto& t = obj->GetTransform();
        Vec2 pos(t.position.x, t.position.y);
        Vec2 size = spr->size;

        // Apply scale
        size.x *= t.scale.x;
        size.y *= t.scale.y;

        // Pivot offset
        Vec2 pivotOff(
            -(spr->pivot.x - 0.5f) * size.x,
            -(spr->pivot.y - 0.5f) * size.y
        );

        // Corner positions in world
        Vec2 halfSz(size.x * 0.5f, size.y * 0.5f);
        Vec2 bl(pos.x + pivotOff.x - halfSz.x, pos.y + pivotOff.y - halfSz.y);
        Vec2 tr(pos.x + pivotOff.x + halfSz.x, pos.y + pivotOff.y + halfSz.y);

        // Convert to screen
        Vec2 blScreen = m_Camera.WorldToScreen(bl, m_VpW, m_VpH);
        Vec2 trScreen = m_Camera.WorldToScreen(tr, m_VpW, m_VpH);

        ImU32 col = IM_COL32(
            (int)(spr->color.x * 255),
            (int)(spr->color.y * 255),
            (int)(spr->color.z * 255),
            (int)(spr->color.w * 255)
        );

        if (spr->textureID != 0) {
            // Textured quad
            ImVec2 uv0(spr->uvMin.x, spr->flipY ? spr->uvMin.y : spr->uvMax.y);
            ImVec2 uv1(spr->uvMax.x, spr->flipY ? spr->uvMax.y : spr->uvMin.y);
            if (spr->flipX) { std::swap(uv0.x, uv1.x); }

            dl->AddImage(
                static_cast<ImTextureID>(spr->textureID),
                ImVec2(m_VpX + blScreen.x, m_VpY + trScreen.y),   // ImGui origin top-left
                ImVec2(m_VpX + trScreen.x, m_VpY + blScreen.y),
                uv0, uv1, col);
        } else {
            // Colored rectangle
            dl->AddRectFilled(
                ImVec2(m_VpX + blScreen.x, m_VpY + trScreen.y),
                ImVec2(m_VpX + trScreen.x, m_VpY + blScreen.y),
                col);
            // Outline
            dl->AddRect(
                ImVec2(m_VpX + blScreen.x, m_VpY + trScreen.y),
                ImVec2(m_VpX + trScreen.x, m_VpY + blScreen.y),
                IM_COL32(255, 255, 255, 40));
        }

        // Selection highlight
        if (obj == m_Selected) {
            dl->AddRect(
                ImVec2(m_VpX + blScreen.x - 1, m_VpY + trScreen.y - 1),
                ImVec2(m_VpX + trScreen.x + 1, m_VpY + blScreen.y + 1),
                IM_COL32(80, 180, 255, 220), 0.0f, 0, 2.0f);
        }
    }

    // ── Render non-sprite objects as icon markers ──────────────────────────
    for (auto& obj : m_Scene.GetAllObjects()) {
        if (!obj || !obj->IsActive()) continue;
        if (obj->GetComponent<SpriteComponent>()) continue; // already rendered above

        auto& t = obj->GetTransform();
        Vec2 pos(t.position.x, t.position.y);
        Vec2 screenPos = m_Camera.WorldToScreen(pos, m_VpW, m_VpH);
        f32 sx = m_VpX + screenPos.x;
        f32 sy = m_VpY + screenPos.y;

        // Determine type and draw an icon marker
        auto* lbl = obj->GetComponent<Label2D>();
        auto* pe  = obj->GetComponent<ParticleEmitter2D>();
        auto* tm  = obj->GetComponent<TileMapComponent>();

        ImU32 markerCol;
        const char* typeLabel;
        f32 markerSize = 14.0f;

        if (lbl) {
            markerCol = IM_COL32(255, 200, 80, 220);
            typeLabel = "T";  // Text
            // Also render the label text preview
            dl->AddText(ImVec2(sx - 20, sy + markerSize + 2),
                        IM_COL32(255, 200, 80, 180), lbl->text.c_str());
        } else if (pe) {
            markerCol = IM_COL32(255, 100, 200, 220);
            typeLabel = "P";  // Particles
        } else if (tm) {
            markerCol = IM_COL32(100, 200, 100, 220);
            typeLabel = "M";  // Map
        } else {
            markerCol = IM_COL32(200, 200, 200, 180);
            typeLabel = "?";
        }

        // Draw diamond marker
        dl->AddQuadFilled(
            ImVec2(sx, sy - markerSize),
            ImVec2(sx + markerSize, sy),
            ImVec2(sx, sy + markerSize),
            ImVec2(sx - markerSize, sy),
            markerCol);
        dl->AddQuad(
            ImVec2(sx, sy - markerSize),
            ImVec2(sx + markerSize, sy),
            ImVec2(sx, sy + markerSize),
            ImVec2(sx - markerSize, sy),
            IM_COL32(255, 255, 255, 100), 1.5f);

        // Type label centered in diamond
        ImVec2 textSz = ImGui::CalcTextSize(typeLabel);
        dl->AddText(ImVec2(sx - textSz.x * 0.5f, sy - textSz.y * 0.5f),
                    IM_COL32(255, 255, 255, 255), typeLabel);

        // Object name below
        ImVec2 nameSz = ImGui::CalcTextSize(obj->GetName().c_str());
        dl->AddText(ImVec2(sx - nameSz.x * 0.5f, sy + markerSize + (lbl ? 16.0f : 2.0f)),
                    IM_COL32(200, 200, 200, 160), obj->GetName().c_str());

        // Selection highlight
        if (obj.get() == m_Selected) {
            dl->AddQuad(
                ImVec2(sx, sy - markerSize - 2),
                ImVec2(sx + markerSize + 2, sy),
                ImVec2(sx, sy + markerSize + 2),
                ImVec2(sx - markerSize - 2, sy),
                IM_COL32(80, 180, 255, 255), 2.5f);
        }
    }
}

// ── Gizmo rendering ───────────────────────────────────────────────────────

void Editor2DViewport::RenderGizmo2D(f32 vpW, f32 vpH) {
    if (!m_Selected) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    auto& t = m_Selected->GetTransform();
    Vec2 pos(t.position.x, t.position.y);
    Vec2 screenPos = m_Camera.WorldToScreen(pos, vpW, vpH);
    f32 sx = m_VpX + screenPos.x;
    f32 sy = m_VpY + screenPos.y;

    f32 arrowLen = 60.0f;

    switch (m_GizmoMode) {
    case Gizmo2DMode::Translate: {
        // X axis (red arrow right)
        dl->AddLine(ImVec2(sx, sy), ImVec2(sx + arrowLen, sy), IM_COL32(230, 60, 60, 255), 2.5f);
        dl->AddTriangleFilled(
            ImVec2(sx + arrowLen + 10, sy),
            ImVec2(sx + arrowLen - 3, sy - 6),
            ImVec2(sx + arrowLen - 3, sy + 6),
            IM_COL32(230, 60, 60, 255));

        // Y axis (green arrow up)
        dl->AddLine(ImVec2(sx, sy), ImVec2(sx, sy - arrowLen), IM_COL32(60, 230, 60, 255), 2.5f);
        dl->AddTriangleFilled(
            ImVec2(sx, sy - arrowLen - 10),
            ImVec2(sx - 6, sy - arrowLen + 3),
            ImVec2(sx + 6, sy - arrowLen + 3),
            IM_COL32(60, 230, 60, 255));

        // Center square (free move)
        dl->AddRectFilled(ImVec2(sx - 5, sy - 5), ImVec2(sx + 5, sy + 5),
                          IM_COL32(255, 255, 100, 200));
        break;
    }
    case Gizmo2DMode::Rotate: {
        // Arc circle
        dl->AddCircle(ImVec2(sx, sy), arrowLen * 0.7f, IM_COL32(100, 150, 255, 200), 48, 2.0f);
        // Angle indicator
        f32 angle = 0.0f; // Would use t.rotation euler Z
        Vec3 euler = t.rotation.ToEuler();
        angle = euler.z;
        f32 ex = sx + std::cos(angle) * arrowLen * 0.7f;
        f32 ey = sy - std::sin(angle) * arrowLen * 0.7f;
        dl->AddLine(ImVec2(sx, sy), ImVec2(ex, ey), IM_COL32(100, 150, 255, 255), 2.0f);
        dl->AddCircleFilled(ImVec2(ex, ey), 4.0f, IM_COL32(100, 150, 255, 255));
        break;
    }
    case Gizmo2DMode::Scale: {
        // X axis (red with square end)
        dl->AddLine(ImVec2(sx, sy), ImVec2(sx + arrowLen, sy), IM_COL32(230, 60, 60, 255), 2.5f);
        dl->AddRectFilled(ImVec2(sx + arrowLen - 4, sy - 4),
                          ImVec2(sx + arrowLen + 4, sy + 4),
                          IM_COL32(230, 60, 60, 255));

        // Y axis (green with square end)
        dl->AddLine(ImVec2(sx, sy), ImVec2(sx, sy - arrowLen), IM_COL32(60, 230, 60, 255), 2.5f);
        dl->AddRectFilled(ImVec2(sx - 4, sy - arrowLen - 4),
                          ImVec2(sx + 4, sy - arrowLen + 4),
                          IM_COL32(60, 230, 60, 255));

        // Center (uniform scale)
        dl->AddRectFilled(ImVec2(sx - 5, sy - 5), ImVec2(sx + 5, sy + 5),
                          IM_COL32(255, 255, 100, 200));
        break;
    }
    }
}

// ── Collider visualization ─────────────────────────────────────────────────

void Editor2DViewport::RenderColliders2D(f32 vpW, f32 vpH) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    for (auto& obj : m_Scene.GetAllObjects()) {
        auto* col = obj->GetComponent<Collider2D>();
        if (!col) continue;

        auto& t = obj->GetTransform();
        Vec2 pos(t.position.x + col->offset.x, t.position.y + col->offset.y);
        Vec2 screenPos = m_Camera.WorldToScreen(pos, vpW, vpH);
        f32 sx = m_VpX + screenPos.x;
        f32 sy = m_VpY + screenPos.y;

        ImU32 colColor = col->isTrigger ? IM_COL32(100, 255, 100, 100) : IM_COL32(100, 200, 255, 100);

        switch (col->shape) {
        case ColliderShape2D::Box: {
            Vec2 halfExt(col->boxSize.x * t.scale.x, col->boxSize.y * t.scale.y);
            Vec2 tl = m_Camera.WorldToScreen({ pos.x - halfExt.x, pos.y + halfExt.y }, vpW, vpH);
            Vec2 br = m_Camera.WorldToScreen({ pos.x + halfExt.x, pos.y - halfExt.y }, vpW, vpH);
            dl->AddRect(ImVec2(m_VpX + tl.x, m_VpY + tl.y),
                        ImVec2(m_VpX + br.x, m_VpY + br.y), colColor, 0, 0, 1.5f);
            break;
        }
        case ColliderShape2D::Circle: {
            f32 radiusScreen = col->radius * m_Camera.GetZoom() * vpH * 0.5f;
            dl->AddCircle(ImVec2(sx, sy), radiusScreen, colColor, 32, 1.5f);
            break;
        }
        case ColliderShape2D::Capsule: {
            // Simplified: draw as elongated shape
            f32 r = col->radius * m_Camera.GetZoom() * vpH * 0.5f;
            f32 h = col->height * 0.5f * m_Camera.GetZoom() * vpH * 0.5f;
            dl->AddRect(ImVec2(sx - r, sy - h), ImVec2(sx + r, sy + h), colColor, r, 0, 1.5f);
            break;
        }
        default:
            break;
        }
    }
}

// ── Tilemap grid overlay ───────────────────────────────────────────────────

void Editor2DViewport::RenderTilemapGrid(f32 vpW, f32 vpH) {
    // Find a tilemap component in the scene
    TileMapComponent* tm = nullptr;
    for (auto& obj : m_Scene.GetAllObjects()) {
        tm = obj->GetComponent<TileMapComponent>();
        if (tm) break;
    }
    if (!tm) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 ts = tm->tileSize;

    for (i32 x = 0; x <= tm->mapWidth; x++) {
        Vec2 top = m_Camera.WorldToScreen({ x * ts, static_cast<f32>(tm->mapHeight) * ts }, vpW, vpH);
        Vec2 bot = m_Camera.WorldToScreen({ x * ts, 0.0f }, vpW, vpH);
        dl->AddLine(ImVec2(m_VpX + top.x, m_VpY + top.y),
                     ImVec2(m_VpX + bot.x, m_VpY + bot.y),
                     IM_COL32(200, 200, 100, 60));
    }
    for (i32 y = 0; y <= tm->mapHeight; y++) {
        Vec2 left  = m_Camera.WorldToScreen({ 0.0f, y * ts }, vpW, vpH);
        Vec2 right = m_Camera.WorldToScreen({ static_cast<f32>(tm->mapWidth) * ts, y * ts }, vpW, vpH);
        dl->AddLine(ImVec2(m_VpX + left.x,  m_VpY + left.y),
                     ImVec2(m_VpX + right.x, m_VpY + right.y),
                     IM_COL32(200, 200, 100, 60));
    }
}

// ── Input handling ─────────────────────────────────────────────────────────

void Editor2DViewport::HandleInput(f32 dt, f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    ImVec2 mousePos = ImGui::GetMousePos();
    bool hovered = (mousePos.x >= vpX && mousePos.x < vpX + vpW &&
                    mousePos.y >= vpY && mousePos.y < vpY + vpH);
    if (!hovered) return;

    ImGuiIO& io = ImGui::GetIO();
    f32 localMX = mousePos.x - vpX;
    f32 localMY = mousePos.y - vpY;

    // ── Pan (MMB drag or Space+LMB) ───────────────────────────────────────
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        m_Camera.Pan(delta.x, delta.y);
    }

    // ── Zoom (scroll) ─────────────────────────────────────────────────────
    if (std::fabs(io.MouseWheel) > 0.01f) {
        Vec2 worldMouse = m_Camera.ScreenToWorld(localMX, localMY, vpW, vpH);
        m_Camera.Zoom(io.MouseWheel, worldMouse.x, worldMouse.y);
    }

    // ── Keyboard shortcuts ────────────────────────────────────────────────
    if (!io.WantTextInput) {
        // Home = reset view
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
            m_Camera.Reset();
        }
        // F = focus on selected
        if (ImGui::IsKeyPressed(ImGuiKey_F) && m_Selected) {
            Vec2 pos(m_Selected->GetTransform().position.x, m_Selected->GetTransform().position.y);
            m_Camera.FocusOn(pos);
        }
        // Delete
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_Selected) {
            m_Scene.DestroyGameObject(m_Selected);
            m_Selected = nullptr;
        }
        // Gizmo mode shortcuts
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoMode = Gizmo2DMode::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoMode = Gizmo2DMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoMode = Gizmo2DMode::Scale;
    }

    // ── Tilemap painting ──────────────────────────────────────────────────
    if (tilemapMode && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        HandleTilemapPaint(vpX, vpY, vpW, vpH);
        return;
    }

    // ── Object picking (LMB click) ────────────────────────────────────────
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_Dragging) {
        HandlePicking(vpX, vpY, vpW, vpH);
    }

    // ── Object dragging ───────────────────────────────────────────────────
    if (m_Selected && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !tilemapMode) {
        HandleDrag(vpX, vpY, vpW, vpH);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_Dragging = false;
    }
}

void Editor2DViewport::HandlePicking(f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    ImVec2 mousePos = ImGui::GetMousePos();
    f32 localMX = mousePos.x - vpX;
    f32 localMY = mousePos.y - vpY;
    Vec2 worldMouse = m_Camera.ScreenToWorld(localMX, localMY, vpW, vpH);

    // Build a picking list of ALL objects (not just sprites)
    // Check in reverse order so front-most objects are picked first
    auto& allObjs = m_Scene.GetAllObjects();
    m_Selected = nullptr;

    for (int i = static_cast<int>(allObjs.size()) - 1; i >= 0; i--) {
        auto* obj = allObjs[i].get();
        if (!obj || !obj->IsActive()) continue;

        auto& t = obj->GetTransform();
        Vec2 pos(t.position.x, t.position.y);
        Vec2 halfSz(0.5f, 0.5f); // default half-size for non-sprite objects

        // Determine bounds based on component type
        auto* spr = obj->GetComponent<SpriteComponent>();
        if (spr) {
            halfSz = { spr->size.x * t.scale.x * 0.5f, spr->size.y * t.scale.y * 0.5f };
        } else {
            // Use a default clickable area based on scale (1x1 unit * scale)
            halfSz = { t.scale.x * 0.5f, t.scale.y * 0.5f };
            // Minimum clickable size so tiny objects are still selectable
            if (halfSz.x < 0.3f) halfSz.x = 0.3f;
            if (halfSz.y < 0.3f) halfSz.y = 0.3f;
        }

        if (worldMouse.x >= pos.x - halfSz.x && worldMouse.x <= pos.x + halfSz.x &&
            worldMouse.y >= pos.y - halfSz.y && worldMouse.y <= pos.y + halfSz.y) {
            m_Selected = obj;
            m_Dragging = true;
            m_DragStart = worldMouse;
            m_DragObjStart = { t.position.x, t.position.y };
            m_DragRotStart = 0.0f;
            m_DragScaleStart = { t.scale.x, t.scale.y };
            break;
        }
    }
}

void Editor2DViewport::HandleDrag(f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    if (!m_Selected || !m_Dragging) return;

    ImVec2 mousePos = ImGui::GetMousePos();
    f32 localMX = mousePos.x - vpX;
    f32 localMY = mousePos.y - vpY;
    Vec2 worldMouse = m_Camera.ScreenToWorld(localMX, localMY, vpW, vpH);
    Vec2 delta(worldMouse.x - m_DragStart.x, worldMouse.y - m_DragStart.y);

    auto& t = m_Selected->GetTransform();

    switch (m_GizmoMode) {
    case Gizmo2DMode::Translate: {
        f32 newX = m_DragObjStart.x + delta.x;
        f32 newY = m_DragObjStart.y + delta.y;
        if (snapEnabled) {
            newX = std::round(newX / snapSize) * snapSize;
            newY = std::round(newY / snapSize) * snapSize;
        }
        t.position.x = newX;
        t.position.y = newY;
        break;
    }
    case Gizmo2DMode::Rotate: {
        f32 angle = std::atan2(delta.y, delta.x);
        t.SetEulerDeg(0.0f, 0.0f, angle * 180.0f / 3.14159265f);
        break;
    }
    case Gizmo2DMode::Scale: {
        f32 scaleFactor = 1.0f + delta.x * 0.02f;
        if (scaleFactor < 0.1f) scaleFactor = 0.1f;
        t.scale.x = m_DragScaleStart.x * scaleFactor;
        t.scale.y = m_DragScaleStart.y * scaleFactor;
        break;
    }
    }
}

void Editor2DViewport::HandleTilemapPaint(f32 vpX, f32 vpY, f32 vpW, f32 vpH) {
    // Find tilemap
    TileMapComponent* tm = nullptr;
    for (auto& obj : m_Scene.GetAllObjects()) {
        tm = obj->GetComponent<TileMapComponent>();
        if (tm) break;
    }
    if (!tm) return;

    ImVec2 mousePos = ImGui::GetMousePos();
    Vec2 worldMouse = m_Camera.ScreenToWorld(mousePos.x - vpX, mousePos.y - vpY, vpW, vpH);

    i32 tileX = static_cast<i32>(std::floor(worldMouse.x / tm->tileSize));
    i32 tileY = static_cast<i32>(std::floor(worldMouse.y / tm->tileSize));

    tm->SetTile(tileX, tileY, selectedTile);
}

} // namespace gv

#endif // GV_HAS_GLFW
