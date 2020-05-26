// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/settings.h"

namespace GameSettings {

void LoadOverrides(u64 program_id) {
    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_8ms;
    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch_2ms;
    Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Synch;
    Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Skip;
    Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Asynch;
    Settings::values.gpu_timing_mode_invalidate = Settings::GpuTimingMode::Synch;

    switch (program_id) {
        // JAP / Dragon Quest VII: Fragments of the Forgotten Past
        case 0x0004000000065E00:
        // USA / Dragon Quest VII: Fragments of the Forgotten Past
        case 0x000400000018EF00:
        // EUR / Dragon Quest VII: Fragments of the Forgotten Past
        case 0x000400000018F000:
            Settings::values.use_asynchronous_gpu_emulation = false;
            break;

        //// JAP / The Legend of Zelda: A Link Between Worlds
        // case 0x00040000000EC200:
        //// USA / The Legend of Zelda: A Link Between Worlds
        // case 0x00040000000EC300:
        //// EUR / The Legend of Zelda: A Link Between Worlds
        // case 0x00040000000EC400:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_2ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch_1ms;
        //    Settings::values.gpu_timing_mode_display_transfer =
        //    Settings::GpuTimingMode::Asynch_600us; Settings::values.gpu_timing_mode_flush =
        //    Settings::GpuTimingMode::Skip; Settings::values.gpu_timing_mode_flush_and_invalidate =
        //    Settings::GpuTimingMode::Skip; break;

        //// JAP / The Legend of Zelda: Majora's Mask 3D
        // case 0x00040000000D6E00:
        //// USA / The Legend of Zelda: Majora's Mask 3D
        // case 0x0004000000125500:
        //// EUR / The Legend of Zelda: Majora's Mask 3D
        // case 0x0004000000125600:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Skip;
        //    Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Skip;
        //    break;

        // JAP / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033400:
        // USA / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033500:
        // EUR / The Legend of Zelda: Ocarina of Time 3D
    case 0x0004000000033600:
        // KOR / The Legend of Zelda: Ocarina of Time 3D
    case 0x000400000008F800:
        // CHI / The Legend of Zelda: Ocarina of Time 3D
    case 0x000400000008F900:
        Settings::values.shaders_accurate_mul = true;
        Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
        Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Asynch;
        Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Skip;
        Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Skip;
        break;

        // JAP / Super Mario 3D Land
    case 0x0004000000054100:
        // USA / Super Mario 3D Land
    case 0x0004000000054000:
        // EUR / Super Mario 3D Land
    case 0x0004000000053F00:
        // KOR / Super Mario 3D Land
    case 0x0004000000089D00:
        Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Synch;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch_40us;
        //    Settings::values.gpu_timing_mode_display_transfer =
        //    Settings::GpuTimingMode::Asynch_40us; Settings::values.gpu_timing_mode_flush =
        //    Settings::GpuTimingMode::Skip; Settings::values.gpu_timing_mode_flush_and_invalidate =
        //    Settings::GpuTimingMode::Skip;
        break;

        //// USA / Animal Crossing: New Leaf
        // case 0x0004000000086300:
        //// EUR / Animal Crossing: New Leaf
        // case 0x0004000000086400:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_2ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch_1ms;
        //    Settings::values.gpu_timing_mode_display_transfer =
        //    Settings::GpuTimingMode::Asynch_600us; Settings::values.gpu_timing_mode_flush =
        //    Settings::GpuTimingMode::Skip; Settings::values.gpu_timing_mode_flush_and_invalidate =
        //    Settings::GpuTimingMode::Skip; break;

        //// USA / Pokemon Omega Ruby
        // case 0x000400000011C400:
        //// USA / Pokemon Alpha Sapphire
        // case 0x000400000011C500:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Synch;
        //    Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Skip;
        //    break;

        //// USA / Pokemon X
        // case 0x0004000000055D00:
        //// USA / Pokemon Y
        // case 0x0004000000055E00:
        //// USA / Pokemon X Update 1.x
        // case 0x0004000E00055D00:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Synch;
        //    Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Skip;
        //    break;

        // USA / Pokemon Ultra Sun
    case 0x00040000001B5000:
        // USA / Pokemon Ultra Moon
    case 0x00040000001B5100:
        // Settings::values.force_separable_shader_fix = true;
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Skip;
        //    Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Skip;
        break;

        //// USA / Kirby: Planet Robobot
        // case 0x0004000000183600:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_8ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch_1ms;
        //    Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Synch;
        //    Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Skip;
        //    Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Skip;
        //    break;

        //// JAP / Mario Kart 7
        // case 0x0004000000030600:
        //// USA / Mario Kart 7
        // case 0x0004000000030800:
        //// EUR / Mario Kart 7
        // case 0x0004000000030700:
        //// CHI / Mario Kart 7
        // case 0x000400000008B400:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_1ms;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_2ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_display_transfer = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_flush = Settings::GpuTimingMode::Skip;
        //    Settings::values.gpu_timing_mode_flush_and_invalidate = Settings::GpuTimingMode::Skip;
        //    break;

        //// USA / Super Smash Bros.
        // case 0x00040000000EDF00:
        //// EUR / Super Smash Bros.
        // case 0x00040000000EE000:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_2ms;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_display_transfer =
        //    Settings::GpuTimingMode::Asynch_20us; Settings::values.gpu_timing_mode_flush =
        //    Settings::GpuTimingMode::Skip; Settings::values.gpu_timing_mode_flush_and_invalidate =
        //    Settings::GpuTimingMode::Skip; break;

        //// JAP / New Super Mario Bros. 2
        // case 0x000400000007AD00:
        //// USA / New Super Mario Bros. 2
        // case 0x000400000007AE00:
        //// EUR / New Super Mario Bros. 2
        // case 0x000400000007AF00:
        //// CHI / New Super Mario Bros. 2
        // case 0x00040000000B8A00:
        //// All / New Super Mario Bros. 2
        // case 0x0004000000137E00:
        //    Settings::values.gpu_timing_mode_submit_list = Settings::GpuTimingMode::Asynch_2ms;
        //    Settings::values.gpu_timing_mode_swap_buffers = Settings::GpuTimingMode::Asynch_4ms;
        //    Settings::values.gpu_timing_mode_memory_fill = Settings::GpuTimingMode::Asynch;
        //    Settings::values.gpu_timing_mode_display_transfer =
        //    Settings::GpuTimingMode::Asynch_20us; Settings::values.gpu_timing_mode_flush =
        //    Settings::GpuTimingMode::Skip; Settings::values.gpu_timing_mode_flush_and_invalidate =
        //    Settings::GpuTimingMode::Skip; break;
    }
}

} // namespace GameSettings
