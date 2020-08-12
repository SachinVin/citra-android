// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>
#include "common/threadsafe_queue.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/settings.h"
#include "video_core/command_processor.h"

namespace VideoCore {
class RendererBase;
}

namespace VideoCore::GPUThread {

/// Command to signal to the GPU thread that a command list is ready for processing
struct SubmitListCommand {
    // In order for the variant to be default constructable, the first element needs a default
    // constructor
    constexpr SubmitListCommand() : list(0), size(0) {}
    explicit constexpr SubmitListCommand(PAddr list, u32 size) : list(list), size(size) {}
    PAddr list;
    u32 size;
};

static_assert(std::is_copy_assignable<SubmitListCommand>::value,
              "SubmitListCommand is not copy assignable");
static_assert(std::is_copy_constructible<SubmitListCommand>::value,
              "SubmitListCommand is not copy constructable");

/// Command to signal to the GPU thread that a swap buffers is pending
struct SwapBuffersCommand final {
    explicit constexpr SwapBuffersCommand() {}
};

static_assert(std::is_copy_assignable<SwapBuffersCommand>::value,
              "SwapBuffersCommand is not copy assignable");
static_assert(std::is_copy_constructible<SwapBuffersCommand>::value,
              "SwapBuffersCommand is not copy constructable");

struct MemoryFillCommand final {
    explicit constexpr MemoryFillCommand(const GPU::Regs::MemoryFillConfig* config,
                                         bool is_second_filler)
        : config{config}, is_second_filler(is_second_filler) {}

    const GPU::Regs::MemoryFillConfig* config;
    bool is_second_filler;
};

static_assert(std::is_copy_assignable<MemoryFillCommand>::value,
              "MemoryFillCommand is not copy assignable");
static_assert(std::is_copy_constructible<MemoryFillCommand>::value,
              "MemoryFillCommand is not copy constructable");

struct DisplayTransferCommand final {
    explicit constexpr DisplayTransferCommand(const GPU::Regs::DisplayTransferConfig* config)
        : config{config} {}

    const GPU::Regs::DisplayTransferConfig* config;
};
static_assert(std::is_copy_assignable<DisplayTransferCommand>::value,
              "DisplayTransferCommand is not copy assignable");
static_assert(std::is_copy_constructible<DisplayTransferCommand>::value,
              "DisplayTransferCommand is not copy constructable");

/// Command to signal to the GPU thread to flush a region
struct FlushRegionCommand final {
    explicit constexpr FlushRegionCommand(VAddr addr, u64 size) : addr{addr}, size{size} {}

    VAddr addr;
    u64 size;
};
static_assert(std::is_copy_assignable<FlushRegionCommand>::value,
              "FlushRegionCommand is not copy assignable");
static_assert(std::is_copy_constructible<FlushRegionCommand>::value,
              "FlushRegionCommand is not copy constructable");

/// Command to signal to the GPU thread to flush and invalidate a region
struct FlushAndInvalidateRegionCommand final {
    explicit constexpr FlushAndInvalidateRegionCommand(VAddr addr, u64 size)
        : addr{addr}, size{size} {}

    VAddr addr;
    u64 size;
};
static_assert(std::is_copy_assignable<FlushAndInvalidateRegionCommand>::value,
              "FlushAndInvalidateRegionCommand is not copy assignable");
static_assert(std::is_copy_constructible<FlushAndInvalidateRegionCommand>::value,
              "FlushAndInvalidateRegionCommand is not copy constructable");

/// Command to signal to the GPU thread to flush a region
struct InvalidateRegionCommand final {
    explicit constexpr InvalidateRegionCommand(VAddr addr, u64 size) : addr{addr}, size{size} {}

    VAddr addr;
    u64 size;
};
static_assert(std::is_copy_assignable<InvalidateRegionCommand>::value,
              "InvalidateRegionCommand is not copy assignable");
static_assert(std::is_copy_constructible<InvalidateRegionCommand>::value,
              "InvalidateRegionCommand is not copy constructable");

using CommandData =
    std::variant<SubmitListCommand, SwapBuffersCommand, MemoryFillCommand, DisplayTransferCommand,
                 FlushRegionCommand, FlushAndInvalidateRegionCommand, InvalidateRegionCommand>;

struct CommandDataContainer {
    CommandDataContainer() = default;

    CommandDataContainer(CommandData&& data, u64 next_fence)
        : data{std::move(data)}, fence{next_fence} {}

    CommandData data;
    u64 fence{};
};

/// Struct used to synchronize the GPU thread
struct SynchState final {
    std::atomic_bool is_running{true};
    std::atomic_int queued_frame_count{};
    std::mutex synchronization_mutex;
    std::mutex commands_mutex;
    std::condition_variable commands_condition;
    std::condition_variable synchronization_condition;

    /// Returns true if the gap in GPU commands is small enough that we can consider the CPU and GPU
    /// synchronized. This is entirely empirical.
    bool IsSynchronized() const {
        constexpr std::size_t max_queue_gap{100};
        return queue.Size() <= max_queue_gap;
    }

    void TrySynchronize() {
        if (IsSynchronized()) {
            std::lock_guard lock{synchronization_mutex};
            synchronization_condition.notify_one();
        }
    }

    void WaitForSynchronization(u64 fence);

    void SignalCommands() {
        if (queue.Empty()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(commands_mutex);
            commands_condition.notify_one();
        }
    }

    void WaitForCommands() {
        while (queue.Empty() && is_running)
            ;
        // std::unique_lock lock{commands_mutex};
        // commands_condition.wait(lock, [this] { return !queue.Empty(); });
    }

    void WaitForProcessing() {
        while (!queue.Empty() && is_running)
            ;
    }

    using CommandQueue = Common::SPSCQueue<CommandDataContainer>;
    CommandQueue queue;
    std::atomic<u64> last_fence{};
    std::atomic<u64> signaled_fence{};
};

/// Class used to manage the GPU thread
class ThreadManager final {
public:
    explicit ThreadManager(Core::System& system, VideoCore::RendererBase& renderer);
    ~ThreadManager();

    void SubmitList(PAddr list, u32 size);

    void SwapBuffers();

    void DisplayTransfer(const GPU::Regs::DisplayTransferConfig*);

    void MemoryFill(const GPU::Regs::MemoryFillConfig*, bool is_second_filler);

    void FlushRegion(VAddr addr, u64 size);

    void FlushAndInvalidateRegion(VAddr addr, u64 size);

    void InvalidateRegion(VAddr addr, u64 size);

    void WaitForProcessing();

private:
    void Synchronize(u64 fence, Settings::GpuTimingMode mode);

    /// Pushes a command to be executed by the GPU thread
    u64 PushCommand(CommandData&& command_data);

    /// Returns true if this is called by the GPU thread
    bool IsGpuThread() const {
        return std::this_thread::get_id() == thread_id;
    }

private:
    SynchState state;
    std::unique_ptr<std::thread> thread;
    std::thread::id thread_id{};
    Core::System& system;
    VideoCore::RendererBase& renderer;
    Core::TimingEventType* synchronize_event{};
};

} // namespace VideoCore::GPUThread
