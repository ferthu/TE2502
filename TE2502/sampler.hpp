#pragma once

#include <vulkan/vulkan.h>

#include "vulkan_context.hpp"

// Wrapper around VkSampler
class Sampler
{
public:
	Sampler() {};
	Sampler(VulkanContext& vulkan_context);
	~Sampler();

	Sampler(Sampler&& other);
	Sampler& operator=(Sampler&& other);

	VkSampler get_sampler() { return m_sampler; }

private:
	// Move other into this
	void move_from(Sampler&& other);
	
	// Destroys object
	void destroy();

	VulkanContext* m_context;

	VkSampler m_sampler = VK_NULL_HANDLE;
};

