// Copyright (c) 2025, Evangelion Manuhutu

#include "texture.hpp"

#include <cstring>
#include <stdexcept>

#include "buffers.hpp"
#include "core/logger.hpp"
#include "vulkan_context.hpp"
#include "vulkan_wrapper.hpp"
#include "stb_image.h"

namespace
{
    constexpr VkImageUsageFlags kTextureUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
}

Texture2D::Texture2D() = default;

Texture2D::Texture2D(const std::string &filepath)
{
    load_from_file(filepath);
}

Texture2D::~Texture2D()
{
    destroy();
}

void Texture2D::set_data(void* pixel, uint32_t width, uint32_t height)
{
    ASSERT(pixel != nullptr, "Texture data pointer is null");
    if (pixel == nullptr || width == 0 || height == 0)
    {
        LOG_WARN("Skipping texture upload; invalid arguments (data={}, width={}, height={})", pixel, width, height);
        return;
    }

    const VkDeviceSize image_size = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
    upload_pixels(pixel, image_size, width, height, VK_FORMAT_R8G8B8A8_SRGB);
}

Ref<Texture2D> Texture2D::create()
{
    return CreateRef<Texture2D>();
}

Ref<Texture2D> Texture2D::create(const std::string &filepath)
{
    return CreateRef<Texture2D>(filepath);
}

void Texture2D::load_from_file(const std::string &filepath)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(1);

    const bool is_hdr = stbi_is_hdr(filepath.c_str()) == 1;
    if (is_hdr)
    {
        float *pixels = stbi_loadf(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels)
        {
            LOG_ERROR("Failed to load HDR texture '{}': {}", filepath, stbi_failure_reason());
            throw std::runtime_error("Failed to load HDR texture");
        }

        const VkDeviceSize image_size = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4ull * sizeof(float);
        upload_pixels(pixels, image_size, static_cast<uint32_t>(width), static_cast<uint32_t>(height), VK_FORMAT_R32G32B32A32_SFLOAT);
        stbi_image_free(pixels);
    }
    else
    {
        stbi_uc *pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels)
        {
            LOG_ERROR("Failed to load texture '{}': {}", filepath, stbi_failure_reason());
            throw std::runtime_error("Failed to load texture");
        }

        const VkDeviceSize image_size = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4ull;
        upload_pixels(pixels, image_size, static_cast<uint32_t>(width), static_cast<uint32_t>(height), VK_FORMAT_R8G8B8A8_SRGB);
        stbi_image_free(pixels);
    }
}

void Texture2D::upload_pixels(const void *data, VkDeviceSize size, uint32_t width, uint32_t height, VkFormat format)
{
    ASSERT(data != nullptr, "Pixel data pointer is null");
    if (data == nullptr || size == 0 || width == 0 || height == 0)
    {
        LOG_WARN("Skipping texture upload; invalid data (data={}, size={}, width={}, height={})", data, size, width, height);
        return;
    }

    VulkanContext *context = VulkanContext::get();
    ASSERT(context != nullptr, "VulkanContext is null when uploading texture");
    if (context == nullptr)
    {
        LOG_ERROR("Cannot upload texture data without an active Vulkan context");
        return;
    }

    const VkDevice device = context->get_device();
    const VkPhysicalDevice physical_device = context->get_physical_device();

    destroy();

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(device, &buffer_info, VK_NULL_HANDLE, &staging_buffer);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to create staging buffer for texture upload");

    VkMemoryRequirements staging_requirements;
    vkGetBufferMemoryRequirements(device, staging_buffer, &staging_requirements);

    VkMemoryAllocateInfo staging_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .allocationSize = staging_requirements.size,
        .memoryTypeIndex = find_memory_type(physical_device, staging_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    result = vkAllocateMemory(device, &staging_alloc_info, VK_NULL_HANDLE, &staging_memory);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to allocate staging buffer memory");

    result = vkBindBufferMemory(device, staging_buffer, staging_memory, 0);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to bind staging buffer memory");

    void *mapped_data = nullptr;
    result = vkMapMemory(device, staging_memory, 0, size, 0, &mapped_data);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to map staging buffer memory");
    std::memcpy(mapped_data, data, static_cast<size_t>(size));
    vkUnmapMemory(device, staging_memory);

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { width, height, 1u },
        .mipLevels = 1,
        .arrayLayers = 1u,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = kTextureUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    result = vkCreateImage(device, &image_info, VK_NULL_HANDLE, &m_Image);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to create texture image");

    VkMemoryRequirements image_requirements;
    vkGetImageMemoryRequirements(device, m_Image, &image_requirements);

    VkMemoryAllocateInfo image_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .allocationSize = image_requirements.size,
        .memoryTypeIndex = find_memory_type(physical_device, image_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    result = vkAllocateMemory(device, &image_alloc_info, VK_NULL_HANDLE, &m_ImageMemory);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to allocate texture image memory");

    result = vkBindImageMemory(device, m_Image, m_ImageMemory, 0);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to bind texture image memory");

    VkCommandBufferAllocateInfo command_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandPool = context->get_command_pool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1u
    };

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    result = vkAllocateCommandBuffers(device, &command_alloc_info, &command_buffer);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to allocate command buffer for texture upload");

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = VK_NULL_HANDLE
    };

    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to begin texture upload command buffer");

    VkImageMemoryBarrier barrier_to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = VK_NULL_HANDLE,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_Image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, VK_NULL_HANDLE,
        0, VK_NULL_HANDLE,
        1, &barrier_to_transfer);

    VkBufferImageCopy copy_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1u }
    };

    vkCmdCopyBufferToImage(command_buffer, staging_buffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    VkImageMemoryBarrier barrier_to_shader_read = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = VK_NULL_HANDLE,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_Image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, VK_NULL_HANDLE,
        0, VK_NULL_HANDLE,
        1, &barrier_to_shader_read);

    result = vkEndCommandBuffer(command_buffer);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to record texture upload command buffer");

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandBufferCount = 1u,
        .pCommandBuffers = &command_buffer
    };

    VkQueue queue = context->get_queue()->get_handle();
    result = vkQueueSubmit(queue, 1u, &submit_info, VK_NULL_HANDLE);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to submit texture upload command buffer");
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, context->get_command_pool(), 1, &command_buffer);
    vkDestroyBuffer(device, staging_buffer, VK_NULL_HANDLE);
    vkFreeMemory(device, staging_memory, VK_NULL_HANDLE);

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .image = m_Image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    result = vkCreateImageView(device, &view_info, VK_NULL_HANDLE, &m_ImageView);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to create texture image view");

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    result = vkCreateSampler(device, &sampler_info, VK_NULL_HANDLE, &m_Sampler);
    VK_ERROR_CHECK(result, "[Vulkan] Failed to create texture sampler");

    m_Format = format;
    m_Width = width;
    m_Height = height;
}

void Texture2D::destroy()
{
    VulkanContext *context = VulkanContext::get();
    if (!context)
    {
        m_Image = VK_NULL_HANDLE;
        m_ImageMemory = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
        m_Sampler = VK_NULL_HANDLE;
        m_Format = VK_FORMAT_UNDEFINED;
        m_Width = 0;
        m_Height = 0;
        return;
    }

    const VkDevice device = context->get_device();

    if (m_Sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, m_Sampler, VK_NULL_HANDLE);
        m_Sampler = VK_NULL_HANDLE;
    }

    if (m_ImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, m_ImageView, VK_NULL_HANDLE);
        m_ImageView = VK_NULL_HANDLE;
    }

    if (m_Image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, m_Image, VK_NULL_HANDLE);
        m_Image = VK_NULL_HANDLE;
    }

    if (m_ImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_ImageMemory, VK_NULL_HANDLE);
        m_ImageMemory = VK_NULL_HANDLE;
    }

    m_Format = VK_FORMAT_UNDEFINED;
    m_Width = 0;
    m_Height = 0;
}

