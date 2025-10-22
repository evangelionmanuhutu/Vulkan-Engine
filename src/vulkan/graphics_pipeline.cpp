// Copyright (c) 2025 Evangelion Manuhutu

#include "graphics_pipeline.hpp"
#include "vulkan_wrapper.hpp"

#include <algorithm>

#include "vulkan_context.hpp"

GraphicsPipeline::GraphicsPipeline()
    : m_Handle(VK_NULL_HANDLE), m_Layout(VK_NULL_HANDLE)
{
}

void GraphicsPipeline::destroy()
{
    auto device = VulkanContext::get()->get_device();

    for (const auto layout : m_DescriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(device, layout, VK_NULL_HANDLE);
    }
    
    if (m_Handle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_Handle, VK_NULL_HANDLE);
        m_Handle = VK_NULL_HANDLE;
    }

    if (m_Layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, m_Layout, VK_NULL_HANDLE);
        m_Layout = VK_NULL_HANDLE;
    }

    m_Shaders.clear();
}

GraphicsPipeline::~GraphicsPipeline()
{
    ASSERT(m_Handle == VK_NULL_HANDLE, "Forgeting to call destroy()");
}

GraphicsPipeline &GraphicsPipeline::set_shaders(const std::vector<Ref<Shader>> &shaders)
{
    auto const device = VulkanContext::get()->get_device();

    m_Shaders = shaders;

    // Merge descriptor set layouts from both shaders
    Shader::SetMap merged_sets = Shader::get_merge_sets(shaders);
    auto push_ranges = Shader::get_push_constants(shaders);

    // Create VkDescriptorSetLayout(s)
    std::vector<std::pair<u32, VkDescriptorSetLayout>> set_layout_pairs;
    set_layout_pairs.reserve(merged_sets.size());
    for (auto& [set_index, bindings] : merged_sets)
    {
        VkDescriptorSetLayoutCreateInfo set_info { };
        set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        set_info.bindingCount = static_cast<u32>(bindings.size());
        set_info.pBindings = bindings.data();
        VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
        const VkResult res = vkCreateDescriptorSetLayout(device, &set_info, nullptr, &set_layout);
        VK_ERROR_CHECK(res, "[Vulkan] Failed to create descriptor set layout");
        set_layout_pairs.emplace_back(set_index, set_layout);
    }

    std::ranges::sort(set_layout_pairs, [](auto& a, auto& b){ return a.first < b.first; });

    m_DescriptorSetLayouts.clear();
    m_DescriptorSetLayouts.reserve(set_layout_pairs.size());
    for (auto& p : set_layout_pairs)
    {
        m_DescriptorSetLayouts.push_back(p.second);
    }

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<u32>(m_DescriptorSetLayouts.size()),
        .pSetLayouts = m_DescriptorSetLayouts.empty() ? nullptr : m_DescriptorSetLayouts.data(),
        .pushConstantRangeCount = static_cast<u32>(push_ranges.size()),
        .pPushConstantRanges = push_ranges.empty() ? nullptr : push_ranges.data(),
    };

    // create pipeline layout
    VkResult result = vkCreatePipelineLayout(device, &layout_create_info, VK_NULL_HANDLE, &m_Layout);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to create pipeline layout");

    return *this;
}

void GraphicsPipeline::build(const GraphicsPipelineInfo& info)
{
    auto device = VulkanContext::get()->get_device();

    VkPipelineRasterizationStateCreateInfo rasterization_info = {};
    rasterization_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.depthClampEnable        = VK_FALSE;
    rasterization_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_info.polygonMode             = info.polygon_mode;
    rasterization_info.lineWidth               = info.line_width;
    rasterization_info.cullMode                = info.cull_mode;
    rasterization_info.frontFace               = info.front_face;
    rasterization_info.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisample_info = {};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.sampleShadingEnable  = VK_FALSE;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = info.color_write_mask;
    color_blend_attachment.blendEnable    = info.blending;

    VkPipelineColorBlendStateCreateInfo color_blend_info = {};
    color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_info.logicOpEnable = VK_FALSE;
    color_blend_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments = &color_blend_attachment;
    color_blend_info.blendConstants[0] = 0.0f;
    color_blend_info.blendConstants[1] = 0.0f;
    color_blend_info.blendConstants[2] = 0.0f;
    color_blend_info.blendConstants[3] = 0.0f;

    // Viewport state with dynamic viewport and scissor (no static values needed)
    VkPipelineViewportStateCreateInfo viewport_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = nullptr,  // Will be set dynamically
        .scissorCount = 1,
        .pScissors = nullptr    // Will be set dynamically
    };

    // Dynamic states
    VkDynamicState dynamic_states[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(std::size(dynamic_states)),
        .pDynamicStates = dynamic_states
    };

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages {};
    for (auto& shader : m_Shaders)
    {
        shader_stages.push_back(shader->get_stage());
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = info.topology,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Build vertex input state from stored data
    VkPipelineVertexInputStateCreateInfo vertex_input_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &info.binding_description,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(info.attribute_descriptions.size()),
        .pVertexAttributeDescriptions = info.attribute_descriptions.data()
    };

    // Graphics pipeline creation info
    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_create_info,
        .pRasterizationState = &rasterization_info,
        .pMultisampleState = &multisample_info,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = m_Layout,
        .renderPass = info.render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    // Create the new pipeline
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, VK_NULL_HANDLE, &m_Handle);
    VK_ERROR_CHECK(result,"[Vulkan] Failed to recreate graphics pipeline");
}
