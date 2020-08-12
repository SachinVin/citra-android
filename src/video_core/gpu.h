// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hw/gpu.h"
#include "video_core/gpu_thread.h"

namespace Core {
class System;
}

namespace VideoCore {

class RendererBase;

class GPUBackend {
public:
    explicit GPUBackend(VideoCore::RendererBase& renderer);

    virtual ~GPUBackend();

    virtual void ProcessCommandList(PAddr list, u32 size) = 0;
    virtual void SwapBuffers() = 0;
    virtual void DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) = 0;
    virtual void MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler) = 0;
    virtual void FlushRegion(VAddr addr, u64 size) = 0;
    virtual void FlushAndInvalidateRegion(VAddr addr, u64 size) = 0;
    virtual void InvalidateRegion(VAddr addr, u64 size) = 0;
    virtual void WaitForProcessing();

protected:
    VideoCore::RendererBase& renderer;
};

class GPUSerial : public GPUBackend {
public:
    explicit GPUSerial(Core::System& system, VideoCore::RendererBase& renderer);

    ~GPUSerial();

    void ProcessCommandList(PAddr list, u32 size) override;
    void SwapBuffers() override;
    void DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) override;
    void MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler) override;
    void FlushRegion(VAddr addr, u64 size) override;
    void FlushAndInvalidateRegion(VAddr addr, u64 size) override;
    void InvalidateRegion(VAddr addr, u64 size) override;

private:
    Core::System& system;
};

class GPUParallel : public GPUBackend {
public:
    explicit GPUParallel(Core::System& system, VideoCore::RendererBase& renderer);

    ~GPUParallel();

    void ProcessCommandList(PAddr list, u32 size) override;
    void SwapBuffers() override;
    void DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) override;
    void MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler) override;
    void FlushRegion(VAddr addr, u64 size) override;
    void FlushAndInvalidateRegion(VAddr addr, u64 size) override;
    void InvalidateRegion(VAddr addr, u64 size) override;
    void WaitForProcessing() override;

private:
    GPUThread::ThreadManager gpu_thread;
};

} // namespace VideoCore
