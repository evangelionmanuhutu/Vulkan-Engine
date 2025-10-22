// Copyright (c) 2025 Evangelion Manuhutu

#include "buffers.hpp"
#include "vulkan_wrapper.hpp"

#include "vulkan_context.hpp"

VulkanBuffer::VulkanBuffer(const VkDeviceSize size, VkBufferUsageFlags usage)
{
    const VkDevice device = VulkanContext::get()->get_device();
   
    m_BufferSize = size;
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .size = m_BufferSize,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const VkResult result = vkCreateBuffer(device, &create_info, VK_NULL_HANDLE, &m_Buffer);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to create buffer");

    allocate_memory();
}

VulkanBuffer::VulkanBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage)
{
    const VkDevice device = VulkanContext::get()->get_device();
   
    m_BufferSize = size;
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .size = m_BufferSize,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(device, &create_info, VK_NULL_HANDLE, &m_Buffer);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to create buffer");

    allocate_memory();

    bind_memory();
    VulkanBuffer::set_data(data, size);
}

void VulkanBuffer::bind_memory(const VkDeviceSize offset) const
{
    const VkDevice device = VulkanContext::get()->get_device();
    vkBindBufferMemory(device, m_Buffer, m_Memory, offset);
}

void VulkanBuffer::set_data(const void *data, VkDeviceSize size, VkDeviceSize offset)
{
    const VkDevice device = VulkanContext::get()->get_device();
    copy_data_to_buffer(device, m_Memory, data, size, offset);
}

void VulkanBuffer::destroy()
{
    const VkDevice device = VulkanContext::get()->get_device();
    vkDestroyBuffer(device, m_Buffer, VK_NULL_HANDLE);
    vkFreeMemory(device, m_Memory, VK_NULL_HANDLE);
}

void VulkanBuffer::allocate_memory()
{
    const VkDevice device = VulkanContext::get()->get_device();
    const VkPhysicalDevice physical_device = VulkanContext::get()->get_physical_device();

    // memory requirements
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, m_Buffer, &mem_requirements);
    
    // allocate memory
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, mem_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkResult result = vkAllocateMemory(device, &alloc_info, VK_NULL_HANDLE, &m_Memory);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to allocate vertex buffer memory");
}

// ====== VERTEX BUFFER ======
VertexBuffer::VertexBuffer(const void *data, VkDeviceSize size)
    : VulkanBuffer(data, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
{
}

VertexBuffer::~VertexBuffer()
{
}

Ref<VertexBuffer> VertexBuffer::create(void *data, VkDeviceSize size)
{
    return CreateRef<VertexBuffer>(data, size);
}


// ====== INDEX BUFFER ======
IndexBuffer::IndexBuffer(const std::vector<uint32_t> &indices)
    : m_Count(static_cast<uint32_t>(indices.size()))
    , VulkanBuffer(static_cast<const void*>(indices.data()),
        indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
{
}

IndexBuffer::~IndexBuffer()
{
}

Ref<IndexBuffer> IndexBuffer::create(const std::vector<uint32_t> &indices)
{
    return CreateRef<IndexBuffer>(indices);
}

UniformBuffer::UniformBuffer(VkDeviceSize size)
    : VulkanBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
{
    
}

UniformBuffer::~UniformBuffer()
{
}

Ref<UniformBuffer> UniformBuffer::create(VkDeviceSize size)
{
    return CreateRef<UniformBuffer>(size);
}

void UniformBuffer::destroy()
{
    const VkDevice device = VulkanContext::get()->get_device();
    vkDestroyBuffer(device, m_Buffer, VK_NULL_HANDLE);
    vkFreeMemory(device, m_Memory, VK_NULL_HANDLE);
}
