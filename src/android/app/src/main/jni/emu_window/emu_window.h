// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "core/frontend/emu_window.h"

struct ANativeWindow;

class EmuWindow_Android : public Frontend::EmuWindow {
public:
    EmuWindow_Android(ANativeWindow* surface);
    ~EmuWindow_Android();

    /// Called by the onSurfaceChanges() method to change the surface
    void OnSurfaceChanged(ANativeWindow* surface);

    /// Handles touch event that occur.(Touched or released)
    void OnTouchEvent(int x, int y, bool pressed);

    /// Handles movement of touch pointer
    void OnTouchMoved(int x, int y);

    void SwapBuffers() override;
    void PollEvents() override;
    void MakeCurrent() override;
    void DoneCurrent() override;

private:
    void OnFramebufferSizeChanged();
    bool CreateWindowSurface();
    void DestroyWindowSurface();
    void DestroyContext();

    ANativeWindow* render_window{};
    ANativeWindow* host_window{};

    bool is_shared{};
    int window_width{};
    int window_height{};
    std::vector<int> shared_attribs;

    EGLConfig config;
    EGLSurface egl_surface{};
    EGLContext egl_context{};
    EGLDisplay egl_display{};
};
