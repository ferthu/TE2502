#pragma once

#include <vulkan/vulkan.h>

#include "vulkan_context.hpp"

// Wrapper around VkSampler
class Sampler
{
public:
	Sampler(VulkanContext& vulkan_context);
	~Sampler();

	VkSampler get_sampler() { return m_sampler; }

private:
	VulkanContext& m_context;

	VkSampler m_sampler;
};

