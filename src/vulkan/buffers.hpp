// Copyright (c) 2025 Evangelion Manuhutu

#ifndef VULKAN_BUFFER_HPP
#define VULKAN_BUFFER_HPP

#include <cstring>
#include <vulkan/vulkan.h>
#include "renderer/vertex.hpp"

#include "vulkan_wrapper.hpp"

static u32 find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_prop;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_prop);

    // Iterate over available memory types, not heaps
    for (u32 i = 0; i < mem_prop.memoryTypeCount; ++i)
    {
        if ((type_filter & (1u << i)) && (mem_prop.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
        }
    }
    throw std::runtime_error("[Vulkan] Failed to find suitable memory type!");
}

static void copy_data_to_buffer(VkDevice device, VkDeviceMemory buffer_memory, const void *data, VkDeviceSize size, VkDeviceSize offset)
{
    void *mapped_data;
    vkMapMemory(device, buffer_memory, offset, size, 0, &mapped_data);
    std::memcpy(mapped_data, data, size);
    vkUnmapMemory(device, buffer_memory);
}

class VulkanBuffer
{
public:
    VulkanBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    VulkanBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage);
    virtual ~VulkanBuffer() {};

    void bind_memory(VkDeviceSize offset = 0) const;
    
    virtual void set_data(const void *data, VkDeviceSize size, VkDeviceSize offset = 0);
    VkDeviceMemory get_buffer_memory() const { return m_Memory; }
    VkBuffer get_buffer() const { return m_Buffer; }

    virtual void destroy();
private:
    void allocate_memory();

protected:
    VkDeviceSize m_BufferSize = 0;
    VkBuffer m_Buffer;
    VkDeviceMemory m_Memory;
};

class VertexBuffer : public VulkanBuffer
{
public:
    VertexBuffer(const void *data, VkDeviceSize size);
    ~VertexBuffer() override;
    static Ref<VertexBuffer> create(void *data, VkDeviceSize size);
};

class IndexBuffer : public VulkanBuffer
{
public:
    IndexBuffer(const std::vector<uint32_t> &indices);
    ~IndexBuffer() override;
    
    static Ref<IndexBuffer> create(const std::vector<uint32_t> &indices);
    uint32_t get_count() const { return m_Count; }
private:
    uint32_t m_Count;
};

class UniformBuffer : public VulkanBuffer
{
public:
    UniformBuffer(VkDeviceSize size);
    ~UniformBuffer() override;
    
    static Ref<UniformBuffer> create(VkDeviceSize size);

    void destroy() override;
private:
};

#endif