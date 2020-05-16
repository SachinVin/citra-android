// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <iostream>
#include <memory>
#include "core/frontend/emu_window.h"
#include "video_core/command_processor.h"

namespace Frontend {
class EmuWindow;
}

namespace Memory {
class MemorySystem;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

class GPUBackend;
class RendererBase;

extern std::unique_ptr<RendererBase> g_renderer; ///< Renderer plugin
extern std::unique_ptr<VideoCore::GPUBackend> g_gpu;

// TODO: Wrap these in a user settings struct along with any other graphics settings (often set from
// qt ui)
extern std::atomic<bool> g_hw_renderer_enabled;
extern std::atomic<bool> g_shader_jit_enabled;
extern std::atomic<bool> g_hw_shader_enabled;
extern std::atomic<bool> g_separable_shader_enabled;
extern std::atomic<bool> g_hw_shader_accurate_mul;
extern std::atomic<bool> g_use_disk_shader_cache;
extern std::atomic<bool> g_renderer_bg_color_update_requested;
extern std::atomic<bool> g_renderer_sampler_update_requested;
extern std::atomic<bool> g_renderer_shader_update_requested;
extern std::atomic<bool> g_texture_filter_update_requested;
// Screenshot
extern std::atomic<bool> g_renderer_screenshot_requested;
extern void* g_screenshot_bits;
extern std::function<void()> g_screenshot_complete_callback;
extern Layout::FramebufferLayout g_screenshot_framebuffer_layout;

extern Memory::MemorySystem* g_memory;

enum class ResultStatus {
    Success,
    ErrorGenericDrivers,
    ErrorBelowGL33,
};

/// Initialize the video core
ResultStatus Init(Core::System& system, Frontend::EmuWindow& emu_window,
                  Memory::MemorySystem& memory);

void ProcessCommandList(PAddr list, u32 size);

/// Notify rasterizer that it should swap the current framebuffer
void SwapBuffers();

/// Perform a DisplayTransfer (accelerated by the rasterizer if available)
void DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config);

/// Perform a MemoryFill (accelerated by the rasterizer if available)
void MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler);

/// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
void FlushRegion(VAddr addr, u64 size);

/// Notify rasterizer that any caches of the specified region should be flushed and invalidated
void FlushAndInvalidateRegion(VAddr addr, u64 size);

/// Notify rasterizer that any caches of the specified region should be invalidated
void InvalidateRegion(VAddr addr, u64 size);

/// Shutdown the video core
void Shutdown();

/// Request a screenshot of the next frame
void RequestScreenshot(void* data, std::function<void()> callback,
                       const Layout::FramebufferLayout& layout);

u16 GetResolutionScaleFactor();

template <class Archive>
void serialize(Archive& ar, const unsigned int file_version);

} // namespace VideoCore
