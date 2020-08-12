// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "video_core/command_processor.h"
#include "video_core/gpu.h"
#include "video_core/gpu_thread.h"
#include "video_core/renderer_base.h"

namespace VideoCore {
GPUBackend::GPUBackend(VideoCore::RendererBase& renderer) : renderer{renderer} {}

GPUBackend::~GPUBackend() = default;

void GPUBackend::WaitForProcessing() {}

GPUSerial::GPUSerial(Core::System& system, VideoCore::RendererBase& renderer)
    : GPUBackend(renderer), system{system} {}

GPUSerial::~GPUSerial() {}

void GPUSerial::ProcessCommandList(PAddr list, u32 size) {
    Pica::CommandProcessor::ProcessCommandList(list, size);
    Pica::CommandProcessor::AfterCommandList();
}

void GPUSerial::SwapBuffers() {
    renderer.SwapBuffers();
    Pica::CommandProcessor::AfterSwapBuffers();
}

void GPUSerial::DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) {
    Pica::CommandProcessor::ProcessDisplayTransfer(*config);
    Pica::CommandProcessor::AfterDisplayTransfer();
}

void GPUSerial::MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler) {
    Pica::CommandProcessor::ProcessMemoryFill(*config);
    Pica::CommandProcessor::AfterMemoryFill(is_second_filler);
}

void GPUSerial::FlushRegion(VAddr addr, u64 size) {
    renderer.Rasterizer()->FlushRegion(addr, size);
}

void GPUSerial::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    renderer.Rasterizer()->FlushAndInvalidateRegion(addr, size);
}

void GPUSerial::InvalidateRegion(VAddr addr, u64 size) {
    renderer.Rasterizer()->InvalidateRegion(addr, size);
}

GPUParallel::GPUParallel(Core::System& system, VideoCore::RendererBase& renderer)
    : GPUBackend(renderer), gpu_thread(system, renderer) {}

GPUParallel::~GPUParallel() = default;

void GPUParallel::ProcessCommandList(PAddr list, u32 size) {
    gpu_thread.SubmitList(list, size);
}

void GPUParallel::SwapBuffers() {
    gpu_thread.SwapBuffers();
}

void GPUParallel::DisplayTransfer(const GPU::Regs::DisplayTransferConfig* config) {
    gpu_thread.DisplayTransfer(config);
}

void GPUParallel::MemoryFill(const GPU::Regs::MemoryFillConfig* config, bool is_second_filler) {
    gpu_thread.MemoryFill(config, is_second_filler);
}

void GPUParallel::FlushRegion(VAddr addr, u64 size) {
    gpu_thread.FlushRegion(addr, size);
}

void GPUParallel::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    gpu_thread.FlushAndInvalidateRegion(addr, size);
}

void GPUParallel::InvalidateRegion(VAddr addr, u64 size) {
    gpu_thread.InvalidateRegion(addr, size);
}

void GPUParallel::WaitForProcessing() {
    gpu_thread.WaitForProcessing();
}

} // namespace VideoCore
