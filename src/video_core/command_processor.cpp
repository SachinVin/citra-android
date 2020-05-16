// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/color.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/vector_math.h"
#include "core/hle/lock.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hw/gpu.h"
#include "core/memory.h"
#include "core/tracer/recorder.h"
#include "video_core/command_processor.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/primitive_assembly.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/regs.h"
#include "video_core/regs_pipeline.h"
#include "video_core/regs_texturing.h"
#include "video_core/renderer_base.h"
#include "video_core/shader/shader.h"
#include "video_core/utils.h"
#include "video_core/vertex_loader.h"
#include "video_core/video_core.h"

MICROPROFILE_DEFINE(GPU_Drawing, "GPU", "Drawing", MP_RGB(50, 50, 240));
MICROPROFILE_DEFINE(GPU_MemoryFill, "GPU", "MemoryFill", MP_RGB(100, 100, 255));
MICROPROFILE_DEFINE(GPU_TextureCopy, "GPU", "Texture Copy", MP_RGB(100, 100, 255));
MICROPROFILE_DEFINE(GPU_DisplayTransfer, "GPU", "DisplayTransfer", MP_RGB(100, 100, 255));
MICROPROFILE_DEFINE(GPU_CmdlistProcessing, "GPU", "Cmdlist Processing", MP_RGB(100, 255, 100));

namespace Pica::CommandProcessor {

// Expand a 4-bit mask to 4-byte mask, e.g. 0b0101 -> 0x00FF00FF
constexpr std::array<u32, 16> expand_bits_to_bytes{
    0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff, 0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
    0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff, 0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff,
};

static const char* GetShaderSetupTypeName(Shader::ShaderSetup& setup) {
    if (&setup == &g_state.vs) {
        return "vertex shader";
    }
    if (&setup == &g_state.gs) {
        return "geometry shader";
    }
    return "unknown shader";
}

static void WriteUniformBoolReg(Shader::ShaderSetup& setup, u32 value) {
    for (unsigned i = 0; i < setup.uniforms.b.size(); ++i)
        setup.uniforms.b[i] = (value & (1 << i)) != 0;
}

static void WriteUniformIntReg(Shader::ShaderSetup& setup, unsigned index,
                               const Common::Vec4<u8>& values) {
    ASSERT(index < setup.uniforms.i.size());
    setup.uniforms.i[index] = values;
    LOG_TRACE(HW_GPU, "Set {} integer uniform {} to {:02x} {:02x} {:02x} {:02x}",
              GetShaderSetupTypeName(setup), index, values.x, values.y, values.z, values.w);
}

static void WriteUniformFloatReg(ShaderRegs& config, Shader::ShaderSetup& setup,
                                 int& float_regs_counter, std::array<u32, 4>& uniform_write_buffer,
                                 u32 value) {
    auto& uniform_setup = config.uniform_setup;

    // TODO: Does actual hardware indeed keep an intermediate buffer or does
    //       it directly write the values?
    uniform_write_buffer[float_regs_counter++] = value;

    // Uniforms are written in a packed format such that four float24 values are encoded in
    // three 32-bit numbers. We write to internal memory once a full such vector is
    // written.
    if ((float_regs_counter >= 4 && uniform_setup.IsFloat32()) ||
        (float_regs_counter >= 3 && !uniform_setup.IsFloat32())) {
        float_regs_counter = 0;

        auto& uniform = setup.uniforms.f[uniform_setup.index];

        if (uniform_setup.index >= 96) {
            LOG_ERROR(HW_GPU, "Invalid {} float uniform index {}", GetShaderSetupTypeName(setup),
                      (int)uniform_setup.index);
        } else {

            // NOTE: The destination component order indeed is "backwards"
            if (uniform_setup.IsFloat32()) {
                for (auto i : {0, 1, 2, 3}) {
                    float buffer_value;
                    std::memcpy(&buffer_value, &uniform_write_buffer[i], sizeof(float));
                    uniform[3 - i] = float24::FromFloat32(buffer_value);
                }
            } else {
                // TODO: Untested
                uniform.w = float24::FromRaw(uniform_write_buffer[0] >> 8);
                uniform.z = float24::FromRaw(((uniform_write_buffer[0] & 0xFF) << 16) |
                                             ((uniform_write_buffer[1] >> 16) & 0xFFFF));
                uniform.y = float24::FromRaw(((uniform_write_buffer[1] & 0xFFFF) << 8) |
                                             ((uniform_write_buffer[2] >> 24) & 0xFF));
                uniform.x = float24::FromRaw(uniform_write_buffer[2] & 0xFFFFFF);
            }

            LOG_TRACE(HW_GPU, "Set {} float uniform {:x} to ({} {} {} {})",
                      GetShaderSetupTypeName(setup), (int)uniform_setup.index,
                      uniform.x.ToFloat32(), uniform.y.ToFloat32(), uniform.z.ToFloat32(),
                      uniform.w.ToFloat32());

            // TODO: Verify that this actually modifies the register!
            uniform_setup.index.Assign(uniform_setup.index + 1);
        }
    }
}

static void WritePicaReg(u32 id, u32 value, u32 mask) {
    auto& regs = g_state.regs;

    if (id >= Regs::NUM_REGS) {
        LOG_ERROR(
            HW_GPU,
            "Commandlist tried to write to invalid register 0x{:03X} (value: {:08X}, mask: {:X})",
            id, value, mask);
        return;
    }

    // TODO: Figure out how register masking acts on e.g. vs.uniform_setup.set_value
    u32 old_value = regs.reg_array[id];

    const u32 write_mask = expand_bits_to_bytes[mask];

    regs.reg_array[id] = (old_value & ~write_mask) | (value & write_mask);

    // Double check for is_pica_tracing to avoid call overhead
    if (DebugUtils::IsPicaTracing()) {
        DebugUtils::OnPicaRegWrite({(u16)id, (u16)mask, regs.reg_array[id]});
    }

    if (g_debug_context)
        g_debug_context->OnEvent(DebugContext::Event::PicaCommandLoaded,
                                 reinterpret_cast<void*>(&id));

    switch (id) {
    // Trigger IRQ
    case PICA_REG_INDEX(trigger_irq):
        Service::GSP::SignalInterrupt(Service::GSP::InterruptId::P3D);
        break;

    case PICA_REG_INDEX(pipeline.triangle_topology):
        g_state.primitive_assembler.Reconfigure(regs.pipeline.triangle_topology);
        break;

    case PICA_REG_INDEX(pipeline.restart_primitive):
        g_state.primitive_assembler.Reset();
        break;

    case PICA_REG_INDEX(pipeline.vs_default_attributes_setup.index):
        g_state.immediate.current_attribute = 0;
        g_state.immediate.reset_geometry_pipeline = true;
        g_state.default_attr_counter = 0;
        break;

    // Load default vertex input attributes
    case PICA_REG_INDEX(pipeline.vs_default_attributes_setup.set_value[0]):
    case PICA_REG_INDEX(pipeline.vs_default_attributes_setup.set_value[1]):
    case PICA_REG_INDEX(pipeline.vs_default_attributes_setup.set_value[2]): {
        // TODO: Does actual hardware indeed keep an intermediate buffer or does
        //       it directly write the values?
        g_state.default_attr_write_buffer[g_state.default_attr_counter++] = value;

        // Default attributes are written in a packed format such that four float24 values are
        // encoded in
        // three 32-bit numbers. We write to internal memory once a full such vector is
        // written.
        if (g_state.default_attr_counter >= 3) {
            g_state.default_attr_counter = 0;

            auto& setup = regs.pipeline.vs_default_attributes_setup;

            if (setup.index >= 16) {
                LOG_ERROR(HW_GPU, "Invalid VS default attribute index {}", (int)setup.index);
                break;
            }

            Common::Vec4<float24> attribute;

            // NOTE: The destination component order indeed is "backwards"
            attribute.w = float24::FromRaw(g_state.default_attr_write_buffer[0] >> 8);
            attribute.z = float24::FromRaw(((g_state.default_attr_write_buffer[0] & 0xFF) << 16) |
                                           ((g_state.default_attr_write_buffer[1] >> 16) & 0xFFFF));
            attribute.y = float24::FromRaw(((g_state.default_attr_write_buffer[1] & 0xFFFF) << 8) |
                                           ((g_state.default_attr_write_buffer[2] >> 24) & 0xFF));
            attribute.x = float24::FromRaw(g_state.default_attr_write_buffer[2] & 0xFFFFFF);

            LOG_TRACE(HW_GPU, "Set default VS attribute {:x} to ({} {} {} {})", (int)setup.index,
                      attribute.x.ToFloat32(), attribute.y.ToFloat32(), attribute.z.ToFloat32(),
                      attribute.w.ToFloat32());

            // TODO: Verify that this actually modifies the register!
            if (setup.index < 15) {
                g_state.input_default_attributes.attr[setup.index] = attribute;
                setup.index++;
            } else {
                // Put each attribute into an immediate input buffer.  When all specified immediate
                // attributes are present, the Vertex Shader is invoked and everything is sent to
                // the primitive assembler.

                auto& immediate_input = g_state.immediate.input_vertex;
                auto& immediate_attribute_id = g_state.immediate.current_attribute;

                immediate_input.attr[immediate_attribute_id] = attribute;

                if (immediate_attribute_id < regs.pipeline.max_input_attrib_index) {
                    immediate_attribute_id += 1;
                } else {
                    MICROPROFILE_SCOPE(GPU_Drawing);
                    immediate_attribute_id = 0;

                    Shader::OutputVertex::ValidateSemantics(regs.rasterizer);

                    auto* shader_engine = Shader::GetEngine();
                    shader_engine->SetupBatch(g_state.vs, regs.vs.main_offset);

                    // Send to vertex shader
                    if (g_debug_context)
                        g_debug_context->OnEvent(DebugContext::Event::VertexShaderInvocation,
                                                 static_cast<void*>(&immediate_input));
                    Shader::UnitState shader_unit;
                    Shader::AttributeBuffer output{};

                    shader_unit.LoadInput(regs.vs, immediate_input);
                    shader_engine->Run(g_state.vs, shader_unit);
                    shader_unit.WriteOutput(regs.vs, output);

                    // Send to geometry pipeline
                    if (g_state.immediate.reset_geometry_pipeline) {
                        g_state.geometry_pipeline.Reconfigure();
                        g_state.immediate.reset_geometry_pipeline = false;
                    }
                    ASSERT(!g_state.geometry_pipeline.NeedIndexInput());
                    g_state.geometry_pipeline.Setup(shader_engine);
                    g_state.geometry_pipeline.SubmitVertex(output);

                    // TODO: If drawing after every immediate mode triangle kills performance,
                    // change it to flush triangles whenever a drawing config register changes
                    // See: https://github.com/citra-emu/citra/pull/2866#issuecomment-327011550
                    VideoCore::g_renderer->Rasterizer()->DrawTriangles();
                    if (g_debug_context) {
                        g_debug_context->OnEvent(DebugContext::Event::FinishedPrimitiveBatch,
                                                 nullptr);
                    }
                }
            }
        }
        break;
    }

    case PICA_REG_INDEX(pipeline.gpu_mode):
        // This register likely just enables vertex processing and doesn't need any special handling
        break;

    case PICA_REG_INDEX(pipeline.command_buffer.trigger[0]):
    case PICA_REG_INDEX(pipeline.command_buffer.trigger[1]): {
        unsigned index =
            static_cast<unsigned>(id - PICA_REG_INDEX(pipeline.command_buffer.trigger[0]));

        u32* start = (u32*)VideoCore::g_memory->GetPhysicalPointer(
            regs.pipeline.command_buffer.GetPhysicalAddress(index));
        auto& cmd_list = g_state.cmd_list;
        cmd_list.head_ptr = cmd_list.current_ptr = start;
        cmd_list.length = regs.pipeline.command_buffer.GetSize(index) / sizeof(u32);
        break;
    }

    // It seems like these trigger vertex rendering
    case PICA_REG_INDEX(pipeline.trigger_draw):
    case PICA_REG_INDEX(pipeline.trigger_draw_indexed): {
        MICROPROFILE_SCOPE(GPU_Drawing);

#if PICA_LOG_TEV
        DebugUtils::DumpTevStageConfig(regs.GetTevStages());
#endif
        if (g_debug_context)
            g_debug_context->OnEvent(DebugContext::Event::IncomingPrimitiveBatch, nullptr);

        PrimitiveAssembler<Shader::OutputVertex>& primitive_assembler = g_state.primitive_assembler;

        bool accelerate_draw = VideoCore::g_hw_shader_enabled && primitive_assembler.IsEmpty();

        if (regs.pipeline.use_gs == PipelineRegs::UseGS::No) {
            auto topology = primitive_assembler.GetTopology();
            if (topology == PipelineRegs::TriangleTopology::Shader ||
                topology == PipelineRegs::TriangleTopology::List) {
                accelerate_draw = accelerate_draw && (regs.pipeline.num_vertices % 3) == 0;
            }
            // TODO (wwylele): for Strip/Fan topology, if the primitive assember is not restarted
            // after this draw call, the buffered vertex from this draw should "leak" to the next
            // draw, in which case we should buffer the vertex into the software primitive assember,
            // or disable accelerate draw completely. However, there is not game found yet that does
            // this, so this is left unimplemented for now. Revisit this when an issue is found in
            // games.
        } else {
            accelerate_draw = false;
        }

        bool is_indexed = (id == PICA_REG_INDEX(pipeline.trigger_draw_indexed));

        if (accelerate_draw &&
            VideoCore::g_renderer->Rasterizer()->AccelerateDrawBatch(is_indexed)) {
            if (g_debug_context) {
                g_debug_context->OnEvent(DebugContext::Event::FinishedPrimitiveBatch, nullptr);
            }
            break;
        }

        // Processes information about internal vertex attributes to figure out how a vertex is
        // loaded.
        // Later, these can be compiled and cached.
        const u32 base_address = regs.pipeline.vertex_attributes.GetPhysicalBaseAddress();
        VertexLoader loader(regs.pipeline);
        Shader::OutputVertex::ValidateSemantics(regs.rasterizer);

        // Load vertices
        const auto& index_info = regs.pipeline.index_array;
        const u8* index_address_8 =
            VideoCore::g_memory->GetPhysicalPointer(base_address + index_info.offset);
        const u16* index_address_16 = reinterpret_cast<const u16*>(index_address_8);
        bool index_u16 = index_info.format != 0;

        if (g_debug_context && g_debug_context->recorder) {
            for (int i = 0; i < 3; ++i) {
                const auto texture = regs.texturing.GetTextures()[i];
                if (!texture.enabled)
                    continue;

                u8* texture_data =
                    VideoCore::g_memory->GetPhysicalPointer(texture.config.GetPhysicalAddress());
                g_debug_context->recorder->MemoryAccessed(
                    texture_data,
                    Pica::TexturingRegs::NibblesPerPixel(texture.format) * texture.config.width /
                        2 * texture.config.height,
                    texture.config.GetPhysicalAddress());
            }
        }

        DebugUtils::MemoryAccessTracker memory_accesses;

        // Simple circular-replacement vertex cache
        // The size has been tuned for optimal balance between hit-rate and the cost of lookup
        const std::size_t VERTEX_CACHE_SIZE = 32;
        std::array<bool, VERTEX_CACHE_SIZE> vertex_cache_valid{};
        std::array<u16, VERTEX_CACHE_SIZE> vertex_cache_ids;
        std::array<Shader::AttributeBuffer, VERTEX_CACHE_SIZE> vertex_cache;
        Shader::AttributeBuffer vs_output;

        unsigned int vertex_cache_pos = 0;

        auto* shader_engine = Shader::GetEngine();
        Shader::UnitState shader_unit;

        shader_engine->SetupBatch(g_state.vs, regs.vs.main_offset);

        g_state.geometry_pipeline.Reconfigure();
        g_state.geometry_pipeline.Setup(shader_engine);
        if (g_state.geometry_pipeline.NeedIndexInput())
            ASSERT(is_indexed);

        for (unsigned int index = 0; index < regs.pipeline.num_vertices; ++index) {
            // Indexed rendering doesn't use the start offset
            unsigned int vertex =
                is_indexed ? (index_u16 ? index_address_16[index] : index_address_8[index])
                           : (index + regs.pipeline.vertex_offset);

            bool vertex_cache_hit = false;

            if (is_indexed) {
                if (g_state.geometry_pipeline.NeedIndexInput()) {
                    g_state.geometry_pipeline.SubmitIndex(vertex);
                    continue;
                }

                if (g_debug_context && Pica::g_debug_context->recorder) {
                    int size = index_u16 ? 2 : 1;
                    memory_accesses.AddAccess(base_address + index_info.offset + size * index,
                                              size);
                }

                for (unsigned int i = 0; i < VERTEX_CACHE_SIZE; ++i) {
                    if (vertex_cache_valid[i] && vertex == vertex_cache_ids[i]) {
                        vs_output = vertex_cache[i];
                        vertex_cache_hit = true;
                        break;
                    }
                }
            }

            if (!vertex_cache_hit) {
                // Initialize data for the current vertex
                Shader::AttributeBuffer input;
                loader.LoadVertex(base_address, index, vertex, input, memory_accesses);

                // Send to vertex shader
                if (g_debug_context)
                    g_debug_context->OnEvent(DebugContext::Event::VertexShaderInvocation,
                                             (void*)&input);
                shader_unit.LoadInput(regs.vs, input);
                shader_engine->Run(g_state.vs, shader_unit);
                shader_unit.WriteOutput(regs.vs, vs_output);

                if (is_indexed) {
                    vertex_cache[vertex_cache_pos] = vs_output;
                    vertex_cache_valid[vertex_cache_pos] = true;
                    vertex_cache_ids[vertex_cache_pos] = vertex;
                    vertex_cache_pos = (vertex_cache_pos + 1) % VERTEX_CACHE_SIZE;
                }
            }

            // Send to geometry pipeline
            g_state.geometry_pipeline.SubmitVertex(vs_output);
        }

        for (auto& range : memory_accesses.ranges) {
            g_debug_context->recorder->MemoryAccessed(
                VideoCore::g_memory->GetPhysicalPointer(range.first), range.second, range.first);
        }

        VideoCore::g_renderer->Rasterizer()->DrawTriangles();
        if (g_debug_context) {
            g_debug_context->OnEvent(DebugContext::Event::FinishedPrimitiveBatch, nullptr);
        }

        break;
    }

    case PICA_REG_INDEX(gs.bool_uniforms):
        WriteUniformBoolReg(g_state.gs, g_state.regs.gs.bool_uniforms.Value());
        break;

    case PICA_REG_INDEX(gs.int_uniforms[0]):
    case PICA_REG_INDEX(gs.int_uniforms[1]):
    case PICA_REG_INDEX(gs.int_uniforms[2]):
    case PICA_REG_INDEX(gs.int_uniforms[3]): {
        unsigned index = (id - PICA_REG_INDEX(gs.int_uniforms[0]));
        auto values = regs.gs.int_uniforms[index];
        WriteUniformIntReg(g_state.gs, index,
                           Common::Vec4<u8>(values.x, values.y, values.z, values.w));
        break;
    }

    case PICA_REG_INDEX(gs.uniform_setup.set_value[0]):
    case PICA_REG_INDEX(gs.uniform_setup.set_value[1]):
    case PICA_REG_INDEX(gs.uniform_setup.set_value[2]):
    case PICA_REG_INDEX(gs.uniform_setup.set_value[3]):
    case PICA_REG_INDEX(gs.uniform_setup.set_value[4]):
    case PICA_REG_INDEX(gs.uniform_setup.set_value[5]):
    case PICA_REG_INDEX(gs.uniform_setup.set_value[6]):
    case PICA_REG_INDEX(gs.uniform_setup.set_value[7]): {
        WriteUniformFloatReg(g_state.regs.gs, g_state.gs, g_state.gs_float_regs_counter,
                             g_state.gs_uniform_write_buffer, value);
        break;
    }

    case PICA_REG_INDEX(gs.program.set_word[0]):
    case PICA_REG_INDEX(gs.program.set_word[1]):
    case PICA_REG_INDEX(gs.program.set_word[2]):
    case PICA_REG_INDEX(gs.program.set_word[3]):
    case PICA_REG_INDEX(gs.program.set_word[4]):
    case PICA_REG_INDEX(gs.program.set_word[5]):
    case PICA_REG_INDEX(gs.program.set_word[6]):
    case PICA_REG_INDEX(gs.program.set_word[7]): {
        u32& offset = g_state.regs.gs.program.offset;
        if (offset >= 4096) {
            LOG_ERROR(HW_GPU, "Invalid GS program offset {}", offset);
        } else {
            g_state.gs.program_code[offset] = value;
            g_state.gs.MarkProgramCodeDirty();
            offset++;
        }
        break;
    }

    case PICA_REG_INDEX(gs.swizzle_patterns.set_word[0]):
    case PICA_REG_INDEX(gs.swizzle_patterns.set_word[1]):
    case PICA_REG_INDEX(gs.swizzle_patterns.set_word[2]):
    case PICA_REG_INDEX(gs.swizzle_patterns.set_word[3]):
    case PICA_REG_INDEX(gs.swizzle_patterns.set_word[4]):
    case PICA_REG_INDEX(gs.swizzle_patterns.set_word[5]):
    case PICA_REG_INDEX(gs.swizzle_patterns.set_word[6]):
    case PICA_REG_INDEX(gs.swizzle_patterns.set_word[7]): {
        u32& offset = g_state.regs.gs.swizzle_patterns.offset;
        if (offset >= g_state.gs.swizzle_data.size()) {
            LOG_ERROR(HW_GPU, "Invalid GS swizzle pattern offset {}", offset);
        } else {
            g_state.gs.swizzle_data[offset] = value;
            g_state.gs.MarkSwizzleDataDirty();
            offset++;
        }
        break;
    }

    case PICA_REG_INDEX(vs.bool_uniforms):
        // TODO (wwylele): does regs.pipeline.gs_unit_exclusive_configuration affect this?
        WriteUniformBoolReg(g_state.vs, g_state.regs.vs.bool_uniforms.Value());
        break;

    case PICA_REG_INDEX(vs.int_uniforms[0]):
    case PICA_REG_INDEX(vs.int_uniforms[1]):
    case PICA_REG_INDEX(vs.int_uniforms[2]):
    case PICA_REG_INDEX(vs.int_uniforms[3]): {
        // TODO (wwylele): does regs.pipeline.gs_unit_exclusive_configuration affect this?
        unsigned index = (id - PICA_REG_INDEX(vs.int_uniforms[0]));
        auto values = regs.vs.int_uniforms[index];
        WriteUniformIntReg(g_state.vs, index,
                           Common::Vec4<u8>(values.x, values.y, values.z, values.w));
        break;
    }

    case PICA_REG_INDEX(vs.uniform_setup.set_value[0]):
    case PICA_REG_INDEX(vs.uniform_setup.set_value[1]):
    case PICA_REG_INDEX(vs.uniform_setup.set_value[2]):
    case PICA_REG_INDEX(vs.uniform_setup.set_value[3]):
    case PICA_REG_INDEX(vs.uniform_setup.set_value[4]):
    case PICA_REG_INDEX(vs.uniform_setup.set_value[5]):
    case PICA_REG_INDEX(vs.uniform_setup.set_value[6]):
    case PICA_REG_INDEX(vs.uniform_setup.set_value[7]): {
        // TODO (wwylele): does regs.pipeline.gs_unit_exclusive_configuration affect this?
        WriteUniformFloatReg(g_state.regs.vs, g_state.vs, g_state.vs_float_regs_counter,
                             g_state.vs_uniform_write_buffer, value);
        break;
    }

    case PICA_REG_INDEX(vs.program.set_word[0]):
    case PICA_REG_INDEX(vs.program.set_word[1]):
    case PICA_REG_INDEX(vs.program.set_word[2]):
    case PICA_REG_INDEX(vs.program.set_word[3]):
    case PICA_REG_INDEX(vs.program.set_word[4]):
    case PICA_REG_INDEX(vs.program.set_word[5]):
    case PICA_REG_INDEX(vs.program.set_word[6]):
    case PICA_REG_INDEX(vs.program.set_word[7]): {
        u32& offset = g_state.regs.vs.program.offset;
        if (offset >= 512) {
            LOG_ERROR(HW_GPU, "Invalid VS program offset {}", offset);
        } else {
            g_state.vs.program_code[offset] = value;
            g_state.vs.MarkProgramCodeDirty();
            if (!g_state.regs.pipeline.gs_unit_exclusive_configuration) {
                g_state.gs.program_code[offset] = value;
                g_state.gs.MarkProgramCodeDirty();
            }
            offset++;
        }
        break;
    }

    case PICA_REG_INDEX(vs.swizzle_patterns.set_word[0]):
    case PICA_REG_INDEX(vs.swizzle_patterns.set_word[1]):
    case PICA_REG_INDEX(vs.swizzle_patterns.set_word[2]):
    case PICA_REG_INDEX(vs.swizzle_patterns.set_word[3]):
    case PICA_REG_INDEX(vs.swizzle_patterns.set_word[4]):
    case PICA_REG_INDEX(vs.swizzle_patterns.set_word[5]):
    case PICA_REG_INDEX(vs.swizzle_patterns.set_word[6]):
    case PICA_REG_INDEX(vs.swizzle_patterns.set_word[7]): {
        u32& offset = g_state.regs.vs.swizzle_patterns.offset;
        if (offset >= g_state.vs.swizzle_data.size()) {
            LOG_ERROR(HW_GPU, "Invalid VS swizzle pattern offset {}", offset);
        } else {
            g_state.vs.swizzle_data[offset] = value;
            g_state.vs.MarkSwizzleDataDirty();
            if (!g_state.regs.pipeline.gs_unit_exclusive_configuration) {
                g_state.gs.swizzle_data[offset] = value;
                g_state.gs.MarkSwizzleDataDirty();
            }
            offset++;
        }
        break;
    }

    case PICA_REG_INDEX(lighting.lut_data[0]):
    case PICA_REG_INDEX(lighting.lut_data[1]):
    case PICA_REG_INDEX(lighting.lut_data[2]):
    case PICA_REG_INDEX(lighting.lut_data[3]):
    case PICA_REG_INDEX(lighting.lut_data[4]):
    case PICA_REG_INDEX(lighting.lut_data[5]):
    case PICA_REG_INDEX(lighting.lut_data[6]):
    case PICA_REG_INDEX(lighting.lut_data[7]): {
        auto& lut_config = regs.lighting.lut_config;

        ASSERT_MSG(lut_config.index < 256, "lut_config.index exceeded maximum value of 255!");

        g_state.lighting.luts[lut_config.type][lut_config.index].raw = value;
        lut_config.index.Assign(lut_config.index + 1);
        break;
    }

    case PICA_REG_INDEX(texturing.fog_lut_data[0]):
    case PICA_REG_INDEX(texturing.fog_lut_data[1]):
    case PICA_REG_INDEX(texturing.fog_lut_data[2]):
    case PICA_REG_INDEX(texturing.fog_lut_data[3]):
    case PICA_REG_INDEX(texturing.fog_lut_data[4]):
    case PICA_REG_INDEX(texturing.fog_lut_data[5]):
    case PICA_REG_INDEX(texturing.fog_lut_data[6]):
    case PICA_REG_INDEX(texturing.fog_lut_data[7]): {
        g_state.fog.lut[regs.texturing.fog_lut_offset % 128].raw = value;
        regs.texturing.fog_lut_offset.Assign(regs.texturing.fog_lut_offset + 1);
        break;
    }

    case PICA_REG_INDEX(texturing.proctex_lut_data[0]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[1]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[2]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[3]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[4]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[5]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[6]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[7]): {
        auto& index = regs.texturing.proctex_lut_config.index;
        auto& pt = g_state.proctex;

        switch (regs.texturing.proctex_lut_config.ref_table.Value()) {
        case TexturingRegs::ProcTexLutTable::Noise:
            pt.noise_table[index % pt.noise_table.size()].raw = value;
            break;
        case TexturingRegs::ProcTexLutTable::ColorMap:
            pt.color_map_table[index % pt.color_map_table.size()].raw = value;
            break;
        case TexturingRegs::ProcTexLutTable::AlphaMap:
            pt.alpha_map_table[index % pt.alpha_map_table.size()].raw = value;
            break;
        case TexturingRegs::ProcTexLutTable::Color:
            pt.color_table[index % pt.color_table.size()].raw = value;
            break;
        case TexturingRegs::ProcTexLutTable::ColorDiff:
            pt.color_diff_table[index % pt.color_diff_table.size()].raw = value;
            break;
        }
        index.Assign(index + 1);
        break;
    }
    default:
        break;
    }

    VideoCore::g_renderer->Rasterizer()->NotifyPicaRegisterChanged(id);

    if (g_debug_context)
        g_debug_context->OnEvent(DebugContext::Event::PicaCommandProcessed,
                                 reinterpret_cast<void*>(&id));
}

void ProcessCommandList(PAddr list, u32 size) {

    u32* buffer = (u32*)VideoCore::g_memory->GetPhysicalPointer(list);

    if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
        Pica::g_debug_context->recorder->MemoryAccessed((u8*)buffer, size, list);
    }

    g_state.cmd_list.addr = list;
    g_state.cmd_list.head_ptr = g_state.cmd_list.current_ptr = buffer;
    g_state.cmd_list.length = size / sizeof(u32);

    while (g_state.cmd_list.current_ptr < g_state.cmd_list.head_ptr + g_state.cmd_list.length) {

        // Align read pointer to 8 bytes
        if ((g_state.cmd_list.head_ptr - g_state.cmd_list.current_ptr) % 2 != 0)
            ++g_state.cmd_list.current_ptr;

        u32 value = *g_state.cmd_list.current_ptr++;
        const CommandHeader header = {*g_state.cmd_list.current_ptr++};

        WritePicaReg(header.cmd_id, value, header.parameter_mask);

        for (unsigned i = 0; i < header.extra_data_length; ++i) {
            u32 cmd = header.cmd_id + (header.group_commands ? i + 1 : 0);
            WritePicaReg(cmd, *g_state.cmd_list.current_ptr++, header.parameter_mask);
        }
    }
}

static Common::Vec4<u8> DecodePixel(GPU::Regs::PixelFormat input_format, const u8* src_pixel) {
    switch (input_format) {
    case GPU::Regs::PixelFormat::RGBA8:
        return Color::DecodeRGBA8(src_pixel);

    case GPU::Regs::PixelFormat::RGB8:
        return Color::DecodeRGB8(src_pixel);

    case GPU::Regs::PixelFormat::RGB565:
        return Color::DecodeRGB565(src_pixel);

    case GPU::Regs::PixelFormat::RGB5A1:
        return Color::DecodeRGB5A1(src_pixel);

    case GPU::Regs::PixelFormat::RGBA4:
        return Color::DecodeRGBA4(src_pixel);

    default:
        LOG_ERROR(HW_GPU, "Unknown source framebuffer format {:x}", static_cast<u32>(input_format));
        return {0, 0, 0, 0};
    }
}

void ProcessMemoryFill(const GPU::Regs::MemoryFillConfig& config) {
    MICROPROFILE_SCOPE(GPU_MemoryFill);
    const PAddr start_addr = config.GetStartAddress();
    const PAddr end_addr = config.GetEndAddress();

    // TODO: do hwtest with these cases
    if (!VideoCore::g_memory->IsValidPhysicalAddress(start_addr)) {
        LOG_CRITICAL(HW_GPU, "invalid start address {:#010X}", start_addr);
        return;
    }

    if (!VideoCore::g_memory->IsValidPhysicalAddress(end_addr)) {
        LOG_CRITICAL(HW_GPU, "invalid end address {:#010X}", end_addr);
        return;
    }

    if (end_addr <= start_addr) {
        LOG_CRITICAL(HW_GPU, "invalid memory range from {:#010X} to {:#010X}", start_addr,
                     end_addr);
        return;
    }

    u8* start = VideoCore::g_memory->GetPhysicalPointer(start_addr);
    u8* end = VideoCore::g_memory->GetPhysicalPointer(end_addr);

    if (VideoCore::g_renderer->Rasterizer()->AccelerateFill(config))
        return;

    Memory::RasterizerInvalidateRegion(config.GetStartAddress(),
                                       config.GetEndAddress() - config.GetStartAddress());

    if (config.fill_24bit) {
        // fill with 24-bit values
        for (u8* ptr = start; ptr < end; ptr += 3) {
            ptr[0] = config.value_24bit_r;
            ptr[1] = config.value_24bit_g;
            ptr[2] = config.value_24bit_b;
        }
    } else if (config.fill_32bit) {
        // fill with 32-bit values
        if (end > start) {
            u32 value = config.value_32bit;
            std::size_t len = (end - start) / sizeof(u32);
            for (std::size_t i = 0; i < len; ++i)
                memcpy(&start[i * sizeof(u32)], &value, sizeof(u32));
        }
    } else {
        // fill with 16-bit values
        u16 value_16bit = config.value_16bit.Value();
        for (u8* ptr = start; ptr < end; ptr += sizeof(u16))
            memcpy(ptr, &value_16bit, sizeof(u16));
    }
}

static void DisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) {
    MICROPROFILE_SCOPE(GPU_DisplayTransfer);
    const PAddr src_addr = config.GetPhysicalInputAddress();
    const PAddr dst_addr = config.GetPhysicalOutputAddress();

    // TODO: do hwtest with these cases
    if (!VideoCore::g_memory->IsValidPhysicalAddress(src_addr)) {
        LOG_CRITICAL(HW_GPU, "invalid input address {:#010X}", src_addr);
        return;
    }

    if (!VideoCore::g_memory->IsValidPhysicalAddress(dst_addr)) {
        LOG_CRITICAL(HW_GPU, "invalid output address {:#010X}", dst_addr);
        return;
    }

    if (config.input_width == 0) {
        LOG_CRITICAL(HW_GPU, "zero input width");
        return;
    }

    if (config.input_height == 0) {
        LOG_CRITICAL(HW_GPU, "zero input height");
        return;
    }

    if (config.output_width == 0) {
        LOG_CRITICAL(HW_GPU, "zero output width");
        return;
    }

    if (config.output_height == 0) {
        LOG_CRITICAL(HW_GPU, "zero output height");
        return;
    }

    if (VideoCore::g_renderer->Rasterizer()->AccelerateDisplayTransfer(config))
        return;

    u8* src_pointer = VideoCore::g_memory->GetPhysicalPointer(src_addr);
    u8* dst_pointer = VideoCore::g_memory->GetPhysicalPointer(dst_addr);

    if (config.scaling > config.ScaleXY) {
        LOG_CRITICAL(HW_GPU, "Unimplemented display transfer scaling mode {}",
                     config.scaling.Value());
        UNIMPLEMENTED();
        return;
    }

    if (config.input_linear && config.scaling != config.NoScale) {
        LOG_CRITICAL(HW_GPU, "Scaling is only implemented on tiled input");
        UNIMPLEMENTED();
        return;
    }

    int horizontal_scale = config.scaling != config.NoScale ? 1 : 0;
    int vertical_scale = config.scaling == config.ScaleXY ? 1 : 0;

    u32 output_width = config.output_width >> horizontal_scale;
    u32 output_height = config.output_height >> vertical_scale;

    u32 input_size =
        config.input_width * config.input_height * GPU::Regs::BytesPerPixel(config.input_format);
    u32 output_size = output_width * output_height * GPU::Regs::BytesPerPixel(config.output_format);

    Memory::RasterizerFlushRegion(config.GetPhysicalInputAddress(), input_size);
    Memory::RasterizerInvalidateRegion(config.GetPhysicalOutputAddress(), output_size);

    for (u32 y = 0; y < output_height; ++y) {
        for (u32 x = 0; x < output_width; ++x) {
            Common::Vec4<u8> src_color;

            // Calculate the [x,y] position of the input image
            // based on the current output position and the scale
            u32 input_x = x << horizontal_scale;
            u32 input_y = y << vertical_scale;

            u32 output_y;
            if (config.flip_vertically) {
                // Flip the y value of the output data,
                // we do this after calculating the [x,y] position of the input image
                // to account for the scaling options.
                output_y = output_height - y - 1;
            } else {
                output_y = y;
            }

            u32 dst_bytes_per_pixel = GPU::Regs::BytesPerPixel(config.output_format);
            u32 src_bytes_per_pixel = GPU::Regs::BytesPerPixel(config.input_format);
            u32 src_offset;
            u32 dst_offset;

            if (config.input_linear) {
                if (!config.dont_swizzle) {
                    // Interpret the input as linear and the output as tiled
                    u32 coarse_y = output_y & ~7;
                    u32 stride = output_width * dst_bytes_per_pixel;

                    src_offset = (input_x + input_y * config.input_width) * src_bytes_per_pixel;
                    dst_offset = VideoCore::GetMortonOffset(x, output_y, dst_bytes_per_pixel) +
                                 coarse_y * stride;
                } else {
                    // Both input and output are linear
                    src_offset = (input_x + input_y * config.input_width) * src_bytes_per_pixel;
                    dst_offset = (x + output_y * output_width) * dst_bytes_per_pixel;
                }
            } else {
                if (!config.dont_swizzle) {
                    // Interpret the input as tiled and the output as linear
                    u32 coarse_y = input_y & ~7;
                    u32 stride = config.input_width * src_bytes_per_pixel;

                    src_offset = VideoCore::GetMortonOffset(input_x, input_y, src_bytes_per_pixel) +
                                 coarse_y * stride;
                    dst_offset = (x + output_y * output_width) * dst_bytes_per_pixel;
                } else {
                    // Both input and output are tiled
                    u32 out_coarse_y = output_y & ~7;
                    u32 out_stride = output_width * dst_bytes_per_pixel;

                    u32 in_coarse_y = input_y & ~7;
                    u32 in_stride = config.input_width * src_bytes_per_pixel;

                    src_offset = VideoCore::GetMortonOffset(input_x, input_y, src_bytes_per_pixel) +
                                 in_coarse_y * in_stride;
                    dst_offset = VideoCore::GetMortonOffset(x, output_y, dst_bytes_per_pixel) +
                                 out_coarse_y * out_stride;
                }
            }

            const u8* src_pixel = src_pointer + src_offset;
            src_color = DecodePixel(config.input_format, src_pixel);
            if (config.scaling == config.ScaleX) {
                Common::Vec4<u8> pixel =
                    DecodePixel(config.input_format, src_pixel + src_bytes_per_pixel);
                src_color = ((src_color + pixel) / 2).Cast<u8>();
            } else if (config.scaling == config.ScaleXY) {
                Common::Vec4<u8> pixel1 =
                    DecodePixel(config.input_format, src_pixel + 1 * src_bytes_per_pixel);
                Common::Vec4<u8> pixel2 =
                    DecodePixel(config.input_format, src_pixel + 2 * src_bytes_per_pixel);
                Common::Vec4<u8> pixel3 =
                    DecodePixel(config.input_format, src_pixel + 3 * src_bytes_per_pixel);
                src_color = (((src_color + pixel1) + (pixel2 + pixel3)) / 4).Cast<u8>();
            }

            u8* dst_pixel = dst_pointer + dst_offset;
            switch (config.output_format) {
            case GPU::Regs::PixelFormat::RGBA8:
                Color::EncodeRGBA8(src_color, dst_pixel);
                break;

            case GPU::Regs::PixelFormat::RGB8:
                Color::EncodeRGB8(src_color, dst_pixel);
                break;

            case GPU::Regs::PixelFormat::RGB565:
                Color::EncodeRGB565(src_color, dst_pixel);
                break;

            case GPU::Regs::PixelFormat::RGB5A1:
                Color::EncodeRGB5A1(src_color, dst_pixel);
                break;

            case GPU::Regs::PixelFormat::RGBA4:
                Color::EncodeRGBA4(src_color, dst_pixel);
                break;

            default:
                LOG_ERROR(HW_GPU, "Unknown destination framebuffer format {:x}",
                          static_cast<u32>(config.output_format.Value()));
                break;
            }
        }
    }
}

static void TextureCopy(const GPU::Regs::DisplayTransferConfig& config) {
    MICROPROFILE_SCOPE(GPU_TextureCopy);
    const PAddr src_addr = config.GetPhysicalInputAddress();
    const PAddr dst_addr = config.GetPhysicalOutputAddress();

    // TODO: do hwtest with invalid addresses
    if (!VideoCore::g_memory->IsValidPhysicalAddress(src_addr)) {
        LOG_CRITICAL(HW_GPU, "invalid input address {:#010X}", src_addr);
        return;
    }

    if (!VideoCore::g_memory->IsValidPhysicalAddress(dst_addr)) {
        LOG_CRITICAL(HW_GPU, "invalid output address {:#010X}", dst_addr);
        return;
    }

    if (VideoCore::g_renderer->Rasterizer()->AccelerateTextureCopy(config))
        return;

    u8* src_pointer = VideoCore::g_memory->GetPhysicalPointer(src_addr);
    u8* dst_pointer = VideoCore::g_memory->GetPhysicalPointer(dst_addr);

    u32 remaining_size = Common::AlignDown(config.texture_copy.size, 16);

    if (remaining_size == 0) {
        LOG_CRITICAL(HW_GPU, "zero size. Real hardware freezes on this.");
        return;
    }

    u32 input_gap = config.texture_copy.input_gap * 16;
    u32 output_gap = config.texture_copy.output_gap * 16;

    // Zero gap means contiguous input/output even if width = 0. To avoid infinite loop below, width
    // is assigned with the total size if gap = 0.
    u32 input_width = input_gap == 0 ? remaining_size : config.texture_copy.input_width * 16;
    u32 output_width = output_gap == 0 ? remaining_size : config.texture_copy.output_width * 16;

    if (input_width == 0) {
        LOG_CRITICAL(HW_GPU, "zero input width. Real hardware freezes on this.");
        return;
    }

    if (output_width == 0) {
        LOG_CRITICAL(HW_GPU, "zero output width. Real hardware freezes on this.");
        return;
    }

    std::size_t contiguous_input_size =
        config.texture_copy.size / input_width * (input_width + input_gap);
    Memory::RasterizerFlushRegion(config.GetPhysicalInputAddress(),
                                  static_cast<u32>(contiguous_input_size));

    std::size_t contiguous_output_size =
        config.texture_copy.size / output_width * (output_width + output_gap);
    // Only need to flush output if it has a gap
    // const auto FlushInvalidate_fn = (output_gap != 0) ?
    // &VideoCore::g_renderer->Rasterizer()->FlushAndInvalidateRegion
    //                                                  :
    //                                                  &VideoCore::g_renderer->Rasterizer()->InvalidateRegion;
    if (output_gap != 0) {
        VideoCore::g_renderer->Rasterizer()->FlushAndInvalidateRegion(
            config.GetPhysicalOutputAddress(), static_cast<u32>(contiguous_output_size));
    } else {
        VideoCore::g_renderer->Rasterizer()->InvalidateRegion(
            config.GetPhysicalOutputAddress(), static_cast<u32>(contiguous_output_size));
    }

    u32 remaining_input = input_width;
    u32 remaining_output = output_width;
    while (remaining_size > 0) {
        u32 copy_size = std::min({remaining_input, remaining_output, remaining_size});

        std::memcpy(dst_pointer, src_pointer, copy_size);
        src_pointer += copy_size;
        dst_pointer += copy_size;

        remaining_input -= copy_size;
        remaining_output -= copy_size;
        remaining_size -= copy_size;

        if (remaining_input == 0) {
            remaining_input = input_width;
            src_pointer += input_gap;
        }
        if (remaining_output == 0) {
            remaining_output = output_width;
            dst_pointer += output_gap;
        }
    }
}

void ProcessDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) {
    if (config.is_texture_copy) {
        TextureCopy(config);
        LOG_TRACE(HW_GPU,
                  "TextureCopy: {:#X} bytes from {:#010X}({}+{})-> "
                  "{:#010X}({}+{}), flags {:#010X}",
                  config.texture_copy.size, config.GetPhysicalInputAddress(),
                  config.texture_copy.input_width * 16, config.texture_copy.input_gap * 16,
                  config.GetPhysicalOutputAddress(), config.texture_copy.output_width * 16,
                  config.texture_copy.output_gap * 16, config.flags);
    } else {
        DisplayTransfer(config);
        LOG_TRACE(HW_GPU,
                  "DisplayTransfer: {:#010X}({}x{})-> "
                  "{:#010X}({}x{}), dst format {:x}, flags {:#010X}",
                  config.GetPhysicalInputAddress(), config.input_width.Value(),
                  config.input_height.Value(), config.GetPhysicalOutputAddress(),
                  config.output_width.Value(), config.output_height.Value(),
                  static_cast<u32>(config.output_format.Value()), config.flags);
    }
}

void AfterCommandList() {
    Service::GSP::SignalInterrupt(Service::GSP::InterruptId::P3D);
    GPU::g_regs.command_processor_config.trigger = 0;
}

void AfterDisplayTransfer() {
    GPU::g_regs.display_transfer_config.trigger = 0;
    Service::GSP::SignalInterrupt(Service::GSP::InterruptId::PPF);
}

void AfterMemoryFill(bool is_second_filler) {
    const auto& config = GPU::g_regs.memory_fill_config[is_second_filler];
    // Reset "trigger" flag and set the "finish" flag
    // NOTE: This was confirmed to happen on hardware even if "address_start" is zero.
    GPU::g_regs.memory_fill_config[is_second_filler ? 1 : 0].trigger.Assign(0);
    GPU::g_regs.memory_fill_config[is_second_filler ? 1 : 0].finished.Assign(1);
    // It seems that it won't signal interrupt if "address_start" is zero.
    // TODO: hwtest this
    if (config.GetStartAddress() != 0) {
        if (!is_second_filler) {
            Service::GSP::SignalInterrupt(Service::GSP::InterruptId::PSC0);
        } else {
            Service::GSP::SignalInterrupt(Service::GSP::InterruptId::PSC1);
        }
    }
}

void AfterSwapBuffers() {
    // Signal to GSP that GPU interrupt has occurred
    // TODO(yuriks): hwtest to determine if PDC0 is for the Top screen and PDC1 for the Sub
    // screen, or if both use the same interrupts and these two instead determine the
    // beginning and end of the VBlank period. If needed, split the interrupt firing into
    // two different intervals.
    Service::GSP::SignalInterrupt(Service::GSP::InterruptId::PDC0);
    Service::GSP::SignalInterrupt(Service::GSP::InterruptId::PDC1);
}

} // namespace Pica::CommandProcessor
