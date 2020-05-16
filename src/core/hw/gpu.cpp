// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <numeric>
#include <type_traits>
#include "common/alignment.h"
#include "common/color.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/vector_math.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/memory.h"
#include "core/tracer/recorder.h"
#include "video_core/command_processor.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"
#include "video_core/utils.h"
#include "video_core/video_core.h"

namespace GPU {

Regs g_regs;
Memory::MemorySystem* g_memory;

/// Event id for CoreTiming
static Core::TimingEventType* vblank_event;

template <typename T>
inline void Read(T& var, const u32 raw_addr) {
    u32 addr = raw_addr - HW::VADDR_GPU;
    u32 index = addr / 4;

    // Reads other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= Regs::NumIds() || !std::is_same<T, u32>::value) {
        LOG_ERROR(HW_GPU, "unknown Read{} @ {:#010X}", sizeof(var) * 8, addr);
        return;
    }

    var = g_regs[addr / 4];
}

template <typename T>
inline void Write(u32 addr, const T data) {
    addr -= HW::VADDR_GPU;
    u32 index = addr / 4;

    // Writes other than u32 are untested, so I'd rather have them abort than silently fail
    if (index >= Regs::NumIds() || !std::is_same<T, u32>::value) {
        LOG_ERROR(HW_GPU, "unknown Write{} {:#010X} @ {:#010X}", sizeof(data) * 8, (u32)data, addr);
        return;
    }

    g_regs[index] = static_cast<u32>(data);

    switch (index) {

    // Memory fills are triggered once the fill value is written.
    case GPU_REG_INDEX(memory_fill_config[0].trigger):
    case GPU_REG_INDEX(memory_fill_config[1].trigger): {
        const bool is_second_filler = (index != GPU_REG_INDEX(memory_fill_config[0].trigger));
        const auto& config = g_regs.memory_fill_config[is_second_filler];

        if (config.trigger) {
            LOG_TRACE(HW_GPU, "MemoryFill started from {:#010X} to {:#010X}",
                      config.GetStartAddress(), config.GetEndAddress());
            VideoCore::MemoryFill(&config, is_second_filler);
        }
        break;
    }

    case GPU_REG_INDEX(display_transfer_config.trigger): {
        if (g_regs.display_transfer_config.trigger & 1) {

            if (Pica::g_debug_context)
                Pica::g_debug_context->OnEvent(Pica::DebugContext::Event::IncomingDisplayTransfer,
                                               nullptr);
            VideoCore::DisplayTransfer(&g_regs.display_transfer_config);
        }
        break;
    }

    // Seems like writing to this register triggers processing
    case GPU_REG_INDEX(command_processor_config.trigger): {
        const auto& config = g_regs.command_processor_config;
        if (config.trigger & 1) {
            VideoCore::ProcessCommandList(config.GetPhysicalAddress(), config.size);
        }
        break;
    }
    default:
        break;
    }

    // Notify tracer about the register write
    // This is happening *after* handling the write to make sure we properly catch all memory reads.
    if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
        // addr + GPU VBase - IO VBase + IO PBase
        Pica::g_debug_context->recorder->RegisterWritten<T>(
            addr + 0x1EF00000 - 0x1EC00000 + 0x10100000, data);
    }
}

// Explicitly instantiate template functions because we aren't defining this in the header:

template void Read<u64>(u64& var, const u32 addr);
template void Read<u32>(u32& var, const u32 addr);
template void Read<u16>(u16& var, const u32 addr);
template void Read<u8>(u8& var, const u32 addr);

template void Write<u64>(u32 addr, const u64 data);
template void Write<u32>(u32 addr, const u32 data);
template void Write<u16>(u32 addr, const u16 data);
template void Write<u8>(u32 addr, const u8 data);

/// Update hardware
static void VBlankCallback(u64 userdata, s64 cycles_late) {
    VideoCore::SwapBuffers();

    // Reschedule recurrent event
    Core::System::GetInstance().CoreTiming().ScheduleEvent(frame_ticks - cycles_late, vblank_event);
}

/// Initialize hardware
void Init(Memory::MemorySystem& memory) {
    g_memory = &memory;
    memset(&g_regs, 0, sizeof(g_regs));

    auto& framebuffer_top = g_regs.framebuffer_config[0];
    auto& framebuffer_sub = g_regs.framebuffer_config[1];

    // Setup default framebuffer addresses (located in VRAM)
    // .. or at least these are the ones used by system applets.
    // There's probably a smarter way to come up with addresses
    // like this which does not require hardcoding.
    framebuffer_top.address_left1 = 0x181E6000;
    framebuffer_top.address_left2 = 0x1822C800;
    framebuffer_top.address_right1 = 0x18273000;
    framebuffer_top.address_right2 = 0x182B9800;
    framebuffer_sub.address_left1 = 0x1848F000;
    framebuffer_sub.address_left2 = 0x184C7800;

    framebuffer_top.width.Assign(240);
    framebuffer_top.height.Assign(400);
    framebuffer_top.stride = 3 * 240;
    framebuffer_top.color_format.Assign(Regs::PixelFormat::RGB8);
    framebuffer_top.active_fb = 0;

    framebuffer_sub.width.Assign(240);
    framebuffer_sub.height.Assign(320);
    framebuffer_sub.stride = 3 * 240;
    framebuffer_sub.color_format.Assign(Regs::PixelFormat::RGB8);
    framebuffer_sub.active_fb = 0;

    Core::Timing& timing = Core::System::GetInstance().CoreTiming();
    vblank_event = timing.RegisterEvent("GPU::VBlankCallback", VBlankCallback);
    timing.ScheduleEvent(frame_ticks, vblank_event);

    LOG_DEBUG(HW_GPU, "initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    LOG_DEBUG(HW_GPU, "shutdown OK");
}

} // namespace GPU
