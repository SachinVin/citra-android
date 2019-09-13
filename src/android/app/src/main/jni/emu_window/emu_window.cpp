// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>

#include <android/native_window_jni.h>
#include <glad/glad.h>

#include "common/logging/log.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "jni/button_manager.h"
#include "jni/emu_window/emu_window.h"
#include "jni/id_cache.h"
#include "network/network.h"

static void* EGL_GetFuncAddress(const char* name) {
    return (void*)eglGetProcAddress(name);
}

static bool IsPortraitMode() {
    return JNI_FALSE != IDCache::GetEnvForThread()->CallStaticBooleanMethod(
                            IDCache::GetNativeLibraryClass(), IDCache::GetIsPortraitMode());
}

static void UpdateLandscapeScreenLayout() {
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(IDCache::GetEnvForThread()->CallStaticIntMethod(
            IDCache::GetNativeLibraryClass(), IDCache::GetLandscapeScreenLayout()));
}

void EmuWindow_Android::OnSurfaceChanged(ANativeWindow* surface) {
    render_window = surface;
}

void EmuWindow_Android::OnTouchEvent(int x, int y, bool pressed) {
    if (pressed) {
        TouchPressed((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
    } else {
        TouchReleased();
    }
}

void EmuWindow_Android::OnTouchMoved(int x, int y) {
    TouchMoved((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
}

void EmuWindow_Android::OnFramebufferSizeChanged() {
    UpdateLandscapeScreenLayout();
    UpdateCurrentFramebufferLayout(window_width, window_height, IsPortraitMode());
}

EmuWindow_Android::EmuWindow_Android(ANativeWindow* surface) {
    LOG_DEBUG(Frontend, "Initializing Emuwindow");

    Network::Init();

    host_window = surface;
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    EGLint num_configs{}, egl_major{}, egl_minor{};

    if (!egl_display) {
        LOG_CRITICAL(Frontend, "eglGetDisplay() failed");
        return;
    }

    if (!eglInitialize(egl_display, &egl_major, &egl_minor)) {
        LOG_CRITICAL(Frontend, "eglInitialize() failed");
        return;
    }

    constexpr std::array<int, 11> attribs{EGL_RENDERABLE_TYPE,
                                          EGL_OPENGL_ES3_BIT_KHR,
                                          EGL_RED_SIZE,
                                          8,
                                          EGL_GREEN_SIZE,
                                          8,
                                          EGL_BLUE_SIZE,
                                          8,
                                          EGL_SURFACE_TYPE,
                                          EGL_WINDOW_BIT,
                                          EGL_NONE};

    if (!eglChooseConfig(egl_display, attribs.data(), &config, 1, &num_configs)) {
        LOG_CRITICAL(Frontend, "eglChooseConfig() failed");
        return;
    }

    eglBindAPI(EGL_OPENGL_ES_API);

    if (!egl_context) {
        std::vector<EGLint> ctx_attribs{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, &ctx_attribs[0]);
        shared_attribs = std::move(ctx_attribs);

        if (!egl_context) {
            LOG_CRITICAL(Frontend, "eglCreateContext() failed");
            return;
        }
    }

    if (!CreateWindowSurface()) {
        LOG_CRITICAL(Frontend, "CreateWindowSurface() failed");
        return;
    }

    MakeCurrent();

    OnFramebufferSizeChanged();

    gladLoadGLES2Loader(EGL_GetFuncAddress);
}

bool EmuWindow_Android::CreateWindowSurface() {
    if (!host_window) {
        return true;
    }

    EGLint format{};
    eglGetConfigAttrib(egl_display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(host_window, 0, 0, format);
    window_width = ANativeWindow_getWidth(host_window);
    window_height = ANativeWindow_getHeight(host_window);
    UpdateCurrentFramebufferLayout(window_width, window_height);
    NotifyClientAreaSizeChanged(std::make_pair(window_width, window_height));

    egl_surface = eglCreateWindowSurface(egl_display, config,
                                         static_cast<EGLNativeWindowType>(host_window), nullptr);

    return !!egl_surface;
}

void EmuWindow_Android::DestroyWindowSurface() {
    if (egl_surface == EGL_NO_SURFACE) {
        return;
    }

    if (eglGetCurrentSurface(EGL_DRAW) == egl_surface) {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (!eglDestroySurface(egl_display, egl_surface)) {
        LOG_CRITICAL(Frontend, "eglDestroySurface() failed");
    }

    egl_surface = EGL_NO_SURFACE;
}

void EmuWindow_Android::DestroyContext() {
    if (!egl_context) {
        return;
    }

    if (eglGetCurrentContext() == egl_context) {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (!eglDestroyContext(egl_display, egl_context)) {
        LOG_CRITICAL(Frontend, "eglDestroySurface() failed");
    }

    if (!is_shared && !eglTerminate(egl_display)) {
        LOG_CRITICAL(Frontend, "eglTerminate() failed");
    }

    egl_context = EGL_NO_CONTEXT;
    egl_display = EGL_NO_DISPLAY;
}

EmuWindow_Android::~EmuWindow_Android() {
    DestroyWindowSurface();
    DestroyContext();
}

void EmuWindow_Android::SwapBuffers() {
    eglSwapBuffers(egl_display, egl_surface);
}

void EmuWindow_Android::PollEvents() {
    if (!render_window) {
        return;
    }

    host_window = render_window;
    render_window = nullptr;
    DoneCurrent();
    DestroyWindowSurface();
    CreateWindowSurface();
    MakeCurrent();
    OnFramebufferSizeChanged();
}

void EmuWindow_Android::MakeCurrent() {
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
}

void EmuWindow_Android::DoneCurrent() {
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}
