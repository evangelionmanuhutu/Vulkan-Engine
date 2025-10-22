// Copyright (c) 2025, Evangelion Manuhutu

#ifndef VULKAN_GRAPHICS_PIPELINE_HPP
#define VULKAN_GRAPHICS_PIPELINE_HPP

#include "shader.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

struct GraphicsPipelineInfo
{
    // Store actual data, not pointers
    VkVertexInputBindingDescription binding_description;
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    VkExtent2D extent;
    VkRenderPass render_pass;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
    VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE;
    VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkColorComponentFlags color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkBlendFactor src_color_blend_factor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dst_color_blend_factor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp color_blend_op = VK_BLEND_OP_ADD;
    VkBlendFactor src_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dst_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp alpha_blend_op = VK_BLEND_OP_ADD;
    float line_width = 1.0f;
    bool depth_test = true;
    bool depth_write = false;
    bool depth_bias = false;
    bool blending = false;
    bool stencil_test = false;
};

class GraphicsPipeline
{
public:
    GraphicsPipeline();
    ~GraphicsPipeline();

    GraphicsPipeline &set_shaders(const std::vector<Ref<Shader>> &shaders);
    void build(const GraphicsPipelineInfo &info);

    void destroy();

    const std::vector<VkDescriptorSetLayout> &get_descriptor_set_layouts() const { return m_DescriptorSetLayouts; }
    VkPipeline get_handle() const { return m_Handle; }
    VkPipelineLayout get_layout() const { return m_Layout; }

private:
    VkPipeline m_Handle;
    VkPipelineLayout m_Layout;
    std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
    std::vector<Ref<Shader>> m_Shaders;
};

struct DrawArguments
{
    uint32_t vertex_count = 0;
    uint32_t instance_count = 1;
    uint32_t first_vertex = 0;
    uint32_t first_instance = 0;
    int32_t vertex_offset = 0;
};

struct GraphicsState
{
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkViewport viewport;
    VkRect2D scissor;
    VkClearValue clear_value;

    struct
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        uint32_t offset = 0;
        VkIndexType index_type = VK_INDEX_TYPE_UINT32;
    } index_buffer;

    std::vector<VkDescriptorSet> descriptor_sets;
    std::vector<VkBuffer> vertex_buffers;
};

#endif