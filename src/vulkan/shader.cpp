// Copyright (c) 2025 Evangelion Manuhutu

#include "core/assert.hpp"
#include "core/logger.hpp"

#include "shader.hpp"
#include "vulkan_context.hpp"

#include "vulkan_wrapper.hpp"

#include <fstream>

static const char *get_cached_directory()
{
    return "res/cache/shaders";
}

static void create_cached_directory_if_needed()
{
    const std::string cached_directory = get_cached_directory();
    if (!std::filesystem::exists(cached_directory))
    {
        std::filesystem::create_directories(cached_directory);
    }
}

static shaderc_shader_kind vulkan_shader_to_shaderc_kind(const VkShaderStageFlagBits stage)
{
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT: return shaderc_glsl_vertex_shader;
    case VK_SHADER_STAGE_FRAGMENT_BIT: return shaderc_glsl_fragment_shader;
    case VK_SHADER_STAGE_GEOMETRY_BIT: return shaderc_glsl_geometry_shader;
    case VK_SHADER_STAGE_COMPUTE_BIT: return shaderc_glsl_compute_shader;
    default: return static_cast<shaderc_shader_kind>(0);
    }
}

static std::string vulkan_shader_stage_str(const VkShaderStageFlagBits stage)
{
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT: return "Vertex";
    case VK_SHADER_STAGE_FRAGMENT_BIT: return "Fragment";
    case VK_SHADER_STAGE_COMPUTE_BIT: return "Compute";
    case VK_SHADER_STAGE_GEOMETRY_BIT: return "Geometry";
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return "Tessellation";
    default: return "Invalid";
    }
}

static std::string vulkan_shader_stage_extension(const VkShaderStageFlagBits stage)
{
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT: return ".vert.spv";
    case VK_SHADER_STAGE_FRAGMENT_BIT: return ".frag.spv";
    case VK_SHADER_STAGE_COMPUTE_BIT: return ".comp.spv";
    case VK_SHADER_STAGE_GEOMETRY_BIT: return ".geom.spv";
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return ".tes.spv";
    default: return "Invalid";
    }
}

Shader::Shader(const std::filesystem::path& filepath, VkShaderStageFlagBits stage)
{
    if (!std::filesystem::exists(filepath))
    {
        ASSERT(false, "[Shader] Shader is not exists");
    }

    create_cached_directory_if_needed();
    const std::string shader_source = read_file(filepath);

    std::vector<uint32_t> byte_code = compile_or_get_vulkan_binaries(shader_source, filepath.string().c_str(), stage);

    // Reflect to gather info for pipeline creation
    reflect(stage, byte_code);

    const VkDevice device = VulkanContext::get()->get_device();
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = byte_code.size() * sizeof(u32);
    create_info.pCode = byte_code.data();

    VK_ERROR_CHECK(vkCreateShaderModule(device, &create_info, VK_NULL_HANDLE, &m_Module), "[Shader] Could not create shader module");

    m_StageCreateInfo = {};
    m_StageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    m_StageCreateInfo.stage = stage;
    m_StageCreateInfo.module = m_Module;
    m_StageCreateInfo.pName = "main";
    m_StageCreateInfo.pSpecializationInfo = VK_NULL_HANDLE;
    m_StageCreateInfo.pNext = VK_NULL_HANDLE;
}

Shader::~Shader()
{
    const VkDevice device = VulkanContext::get()->get_device();
    vkDestroyShaderModule(device, m_Module, VK_NULL_HANDLE);

    m_Module = VK_NULL_HANDLE;
}

Shader::SetMap Shader::get_merge_sets(
    const std::vector<Ref<Shader>>& shaders)
{
    Shader::SetMap merged_sets;

    for (const auto& shader : shaders)
    {
        for (const auto& [set, bindings] : shader->get_descriptor_set_layout_bindings())
        {
            auto& dst_vec = merged_sets[set];
            for (const auto& b : bindings)
            {
                bool merged = false;
                for (auto& existing : dst_vec)
                {
                    if (existing.binding == b.binding && existing.descriptorType == b.descriptorType)
                    {
                        existing.stageFlags |= b.stageFlags; // merge stage flags
                        merged = true;
                        break;
                    }
                }
                if (!merged)
                {
                    dst_vec.push_back(b);
                }
            }
        }
    }

    return merged_sets;
}

std::vector<VkPushConstantRange> Shader::get_push_constants(const std::vector<Ref<Shader>>& shaders)
{
    // Merge push constant ranges
    std::vector<VkPushConstantRange> push_ranges;
    for (const auto &shader : shaders)
    {
        for (const auto& pr : shader->get_push_constant_ranges())
        {
            bool merged = false;
            for (auto& ex : push_ranges)
            {
                if (ex.offset == pr.offset && ex.size == pr.size)
                {
                    ex.stageFlags |= pr.stageFlags;
                    merged = true;
                    break;
                }
            }
            if (!merged)
            {
                push_ranges.push_back(pr);
            }
        }
    }
    return push_ranges;
}

std::string Shader::read_file(const std::filesystem::path& file_path)
{
    std::string result;
    std::ifstream shader_in(file_path.string(), std::ios::binary);
    if (shader_in.is_open())
    {
        shader_in.seekg(0, std::ios::end);
        if (size_t size = shader_in.tellg(); size != -1)
        {
            result.resize(size);
            shader_in.seekg(0, std::ios::beg);
            shader_in.read(&result[0], size);
        }
        else
        {
            Logger::get_instance().push_message(LoggingLevel::Error, "[Shader] Could not read from file. {}", file_path.string());
        }

        shader_in.close();
    }
    else
    {
        Logger::get_instance().push_message(LoggingLevel::Error, "[Shader] Could not open file. {}", file_path.string());
    }
    return result;
}

std::vector<u32> Shader::compile_or_get_vulkan_binaries(const std::string& shader_source, const std::string& file_path, VkShaderStageFlagBits stage)
{
    std::vector<u32> code;
    shaderc::CompileOptions options;
    std::filesystem::path cached_directory = get_cached_directory();

    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    std::filesystem::path shader_file_path = cached_directory / file_path;
    std::filesystem::path cached_path = cached_directory / (shader_file_path.stem().string() + vulkan_shader_stage_extension(stage));
    std::ifstream shader_file(cached_path, std::ios::binary);
    if (shader_file.is_open())
    {
        shader_file.seekg(0, std::ios::end);
        auto size = shader_file.tellg();
        shader_file.seekg(0, std::ios::beg);
        auto &data = code;
        data.resize(size / sizeof(u32));
        shader_file.read(reinterpret_cast<char*>(data.data()), size);

        shader_file.close();
    }
    else
    {
        shaderc::Compiler compiler;
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(shader_source, vulkan_shader_to_shaderc_kind(stage), file_path.c_str());
        bool success = module.GetCompilationStatus() == shaderc_compilation_status_success;
        std::string error_message = module.GetErrorMessage();
        ASSERT(success, std::format("[Shader] Compilation failed {}", error_message));

        code = std::vector<u32>(module.cbegin(), module.cend());

        std::ofstream out_file(cached_path, std::ios::binary);
        if (out_file.is_open())
        {
            auto &data = code;
            out_file.write(reinterpret_cast<char *>(data.data()), data.size() * sizeof(u32));
            out_file.flush();
            out_file.close();
        }
    }
    
    return code;
}

void Shader::reflect(const VkShaderStageFlagBits shader_stage, const std::vector<u32>& code)
{
    spirv_cross::Compiler compiler(code);
    spirv_cross::ShaderResources resources = compiler.get_shader_resources();

    Logger::get_instance().push_message(LoggingLevel::Info, "[Shader] Shader reflect - {}", vulkan_shader_stage_str(shader_stage));
    Logger::get_instance().push_message(LoggingLevel::Info, "[Shader]    {} Uniform buffers", resources.uniform_buffers.size());
    Logger::get_instance().push_message(LoggingLevel::Info, "[Shader]    {} Resources", resources.sampled_images.size());

    // Descriptor sets: Uniform buffers
    for (const auto& uniform_buffer : resources.uniform_buffers)
    {
        const auto &buffer_type = compiler.get_type(uniform_buffer.base_type_id);
        (void)buffer_type; // size can be used by user later
        u32 binding = compiler.get_decoration(uniform_buffer.id, spv::DecorationBinding);
        u32 set     = compiler.get_decoration(uniform_buffer.id, spv::DecorationDescriptorSet);
        VkDescriptorSetLayoutBinding b {};
        b.binding = binding;
        b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags = shader_stage;
        b.pImmutableSamplers = nullptr;
        m_SetBindings[set].push_back(b);
    }

    // Descriptor sets: Sampled images / combined image samplers
    for (const auto& si : resources.sampled_images)
    {
        u32 binding = compiler.get_decoration(si.id, spv::DecorationBinding);
        u32 set     = compiler.get_decoration(si.id, spv::DecorationDescriptorSet);
        VkDescriptorSetLayoutBinding b {};
        b.binding = binding;
        b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = 1;
        b.stageFlags = shader_stage;
        b.pImmutableSamplers = nullptr;
        m_SetBindings[set].push_back(b);
    }

    // Descriptor sets: Storage buffers
    for (const auto& sb : resources.storage_buffers)
    {
        u32 binding = compiler.get_decoration(sb.id, spv::DecorationBinding);
        u32 set     = compiler.get_decoration(sb.id, spv::DecorationDescriptorSet);
        VkDescriptorSetLayoutBinding b {};
        b.binding = binding;
        b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags = shader_stage;
        b.pImmutableSamplers = nullptr;
        m_SetBindings[set].push_back(b);
    }

    // Push constants
    for (const auto& pc : resources.push_constant_buffers)
    {
        const auto &pc_type = compiler.get_type(pc.base_type_id);
        u32 size = compiler.get_declared_struct_size(pc_type);
        VkPushConstantRange range {};
        range.stageFlags = shader_stage;
        range.offset = 0;
        range.size = size;
        m_PushConstantRanges.push_back(range);
    }

    // Vertex inputs (only for vertex stage)
    if (shader_stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        m_VertexAttributes.clear();
        m_VertexStride = 0;
        // Sort inputs by location to compute offsets consistently
        struct InAttr { u32 loc; spirv_cross::ID id; };
        std::vector<InAttr> inputs;
        inputs.reserve(resources.stage_inputs.size());
        for (const auto& in : resources.stage_inputs)
        {
            u32 loc = compiler.get_decoration(in.id, spv::DecorationLocation);
            inputs.push_back({loc, in.id});
        }
        std::sort(inputs.begin(), inputs.end(), [](const InAttr& a, const InAttr& b){ return a.loc < b.loc; });

        u32 offset = 0;
        for (const auto& it : inputs)
        {
            const auto &type = compiler.get_type(compiler.get_type_from_variable(it.id).self);
            VkFormat fmt = map_spirv_type_to_vk_format(type);
            if (fmt == VK_FORMAT_UNDEFINED)
                continue; // skip unsupported types
            VkVertexInputAttributeDescription attr {};
            attr.location = it.loc;
            attr.binding = 0;
            attr.format = fmt;
            attr.offset = offset;

            // size in bytes for simple float/int/uint vectors
            u32 comp_size = 4; // 32-bit
            u32 elem_count = std::max(1u, type.vecsize);
            u32 attr_size = comp_size * elem_count;
            offset += attr_size;
            m_VertexAttributes.push_back(attr);
        }
        m_VertexStride = offset;
    }
}
