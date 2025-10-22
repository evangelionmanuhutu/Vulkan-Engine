// Copyright (c) 2025, Evangelion Manuhutu

#ifndef VULKAN_TEXTURE_HPP
#define VULKAN_TEXTURE_HPP

#include "core/types.hpp"

#include <vulkan/vulkan.h>
#include <string>
#include <glm/glm.hpp>

class Texture2D
{
public:
	Texture2D();
	Texture2D(const std::string &filepath);
	~Texture2D();

	void set_data(void* pixel, uint32_t width, uint32_t height);
	void destroy();

	static Ref<Texture2D> create();
	static Ref<Texture2D> create(const std::string &filepath);

	VkImage get_image() const { return m_Image; }
	VkSampler get_sampler() const { return m_Sampler; }
	VkImageView get_image_view() const { return m_ImageView; }

private:
	void load_from_file(const std::string &filepath);
	void upload_pixels(const void *data, VkDeviceSize size, uint32_t width, uint32_t height, VkFormat format);

	VkImage m_Image = VK_NULL_HANDLE;
	VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;
	VkSampler m_Sampler = VK_NULL_HANDLE;
	VkImageView m_ImageView = VK_NULL_HANDLE;
	VkFormat m_Format = VK_FORMAT_UNDEFINED;
	uint32_t m_Width = 0;
	uint32_t m_Height = 0;
};

#endif