// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/archives.h"
#include "common/logging/log.h"
#include "common/vector_math.h"
#include "core/memory.h"
#include "core/settings.h"
#include "video_core/gpu.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_vars.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/video_core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

std::unique_ptr<RendererBase> g_renderer; ///< Renderer plugin
std::unique_ptr<GPUBackend> g_gpu;

std::atomic<bool> g_hw_renderer_enabled;
std::atomic<bool> g_shader_jit_enabled;
std::atomic<bool> g_hw_shader_enabled;
std::atomic<bool> g_separable_shader_enabled;
std::atomic<bool> g_hw_shader_accurate_mul;
std::atomic<bool> g_use_disk_shader_cache;
std::atomic<bool> g_renderer_bg_color_update_requested;
std::atomic<bool> g_renderer_sampler_update_requested;
std::atomic<bool> g_renderer_shader_update_requested;
std::atomic<bool> g_texture_filter_update_requested;
// Screenshot
std::atomic<bool> g_renderer_screenshot_requested;
void* g_screenshot_bits;
std::function<void()> g_screenshot_complete_callback;
Layout::FramebufferLayout g_screenshot_framebuffer_layout;

Memory::MemorySystem* g_memory;

/// Initialize the video core
ResultStatus Init(Core::System& system, Frontend::EmuWindow& emu_window,
                  Memory::MemorySystem& memory) {
    g_memory = &memory;
    Pica::Init();

    OpenGL::GLES = Settings::values.use_gles;

    g_renderer = std::make_unique<OpenGL::RendererOpenGL>(emu_window);

    if (Settings::values.use_asynchronous_gpu_emulation) {
        g_gpu = std::make_unique<VideoCore::GPUParallel>(system, *g_renderer);
    } else {
        g_gpu = std::make_unique<VideoCore::GPUSerial>(system, *g_renderer);
    }
    ResultStatus result = g_renderer->Init();

    if (result != ResultStatus::Success) {
        LOG_ERROR(Render, "initialization failed !");
    } else {
        LOG_DEBUG(Render, "initialized OK");
    }

    return result;
}

/// Shutdown the video core
void Shutdown() {
    Pica::Shutdown();

    g_renderer->ShutDown();
    g_gpu.reset();
    g_renderer.reset();

    LOG_DEBUG(Render, "shutdown OK");
}

void RequestScreenshot(void* data, std::function<void()> callback,
                       const Layout::FramebufferLayout& layout) {
    if (g_renderer_screenshot_requested) {
        LOG_ERROR(Render, "A screenshot is already requested or in progress, ignoring the request");
        return;
    }
    g_screenshot_bits = data;
    g_screenshot_complete_callback = std::move(callback);
    g_screenshot_framebuffer_layout = layout;
    g_renderer_screenshot_requested = true;
}

u16 GetResolutionScaleFactor() {
    if (g_hw_renderer_enabled) {
        return Settings::values.resolution_factor
                   ? Settings::values.resolution_factor
                   : g_renderer->GetRenderWindow().GetFramebufferLayout().GetScalingRatio();
    } else {
        // Software renderer always render at native resolution
        return 1;
    }
}

template <class Archive>
void serialize(Archive& ar, const unsigned int) {
    g_gpu->WaitForProcessing();
    ar& Pica::g_state;
}

void ProcessCommandList(PAddr list, u32 size) {
    g_gpu->ProcessCommandList(list, size);
}

void SwapBuffers() {
    g_gpu->SwapBuffers();
}

void DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) {
    g_gpu->DisplayTransfer(config);
}

void MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler) {
    g_gpu->MemoryFill(config, is_second_filler);
}

void FlushRegion(VAddr addr, u64 size) {
    g_gpu->FlushRegion(addr, size);
}

void FlushAndInvalidateRegion(VAddr addr, u64 size) {
    g_gpu->FlushAndInvalidateRegion(addr, size);
}

void InvalidateRegion(VAddr addr, u64 size) {
    g_gpu->InvalidateRegion(addr, size);
}

} // namespace VideoCore

SERIALIZE_IMPL(VideoCore)
