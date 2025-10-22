// Copyright (c) 2025 Evangelion Manuhutu

#ifndef VULKAN_SHADER_HPP
#define VULKAN_SHADER_HPP

#include "core/types.hpp"

#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>

#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_hlsl.hpp>
#include <shaderc/shaderc.hpp>

#include <vulkan/vulkan.h>

static VkFormat map_spirv_type_to_vk_format(const spirv_cross::SPIRType& type)
{
    using spirv_cross::SPIRType;
    if (type.basetype == SPIRType::Float && type.columns == 1)
    {
        switch (type.vecsize)
        {
        case 1: return VK_FORMAT_R32_SFLOAT;
        case 2: return VK_FORMAT_R32G32_SFLOAT;
        case 3: return VK_FORMAT_R32G32B32_SFLOAT;
        case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        default: break;
        }
    }
    if (type.basetype == SPIRType::Int && type.columns == 1)
    {
        switch (type.vecsize)
        {
        case 1: return VK_FORMAT_R32_SINT;
        case 2: return VK_FORMAT_R32G32_SINT;
        case 3: return VK_FORMAT_R32G32B32_SINT;
        case 4: return VK_FORMAT_R32G32B32A32_SINT;
        default: break;
        }
    }
    if (type.basetype == spirv_cross::SPIRType::UInt && type.columns == 1)
    {
        switch (type.vecsize)
        {
        case 1: return VK_FORMAT_R32_UINT;
        case 2: return VK_FORMAT_R32G32_UINT;
        case 3: return VK_FORMAT_R32G32B32_UINT;
        case 4: return VK_FORMAT_R32G32B32A32_UINT;
        default: break;
        }
    }
    return VK_FORMAT_UNDEFINED;
}



class Shader {
public:
    using SetMap = std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>;

    Shader(const std::filesystem::path& filepath, VkShaderStageFlagBits stage);
    ~Shader();

    const VkPipelineShaderStageCreateInfo &get_stage() const { return m_StageCreateInfo; }
    VkShaderModule get_module() const { return m_Module; }

    static SetMap get_merge_sets(const std::vector<Ref<Shader>> &shaders);
    static std::vector<VkPushConstantRange> get_push_constants(const std::vector<Ref<Shader>> &shaders);

    // Reflection getters
    // Vertex input (only meaningful for vertex stage)
    const std::vector<VkVertexInputAttributeDescription>& get_vertex_attributes() const { return m_VertexAttributes; }
    u32 get_vertex_stride() const { return m_VertexStride; }

    // Descriptor set layout bindings grouped by set index
    const std::unordered_map<u32, std::vector<VkDescriptorSetLayoutBinding>>& get_descriptor_set_layout_bindings() const { return m_SetBindings; }

    // Push constant ranges gathered from this shader
    const std::vector<VkPushConstantRange>& get_push_constant_ranges() const { return m_PushConstantRanges; }

private:
    [[nodiscard]] static std::string read_file(const std::filesystem::path& file_path) ;
    static std::vector<u32> compile_or_get_vulkan_binaries(const std::string &shader_source, const std::string &file_path, VkShaderStageFlagBits stage) ;
    void reflect(VkShaderStageFlagBits shader_stage, const std::vector<u32> &code);

    // Reflection data
    std::vector<VkVertexInputAttributeDescription> m_VertexAttributes;
    u32 m_VertexStride = 0;
    std::unordered_map<u32, std::vector<VkDescriptorSetLayoutBinding>> m_SetBindings;
    std::vector<VkPushConstantRange> m_PushConstantRanges;

    VkShaderModule m_Module;
    VkPipelineShaderStageCreateInfo m_StageCreateInfo;
};

#endif //VULKAN_SHADER_H
