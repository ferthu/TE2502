#include "sampler.hpp"
#include "utilities.hpp"


Sampler::Sampler(VulkanContext& vulkan_context) : m_context(&vulkan_context)
{
	VkSamplerCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.magFilter = VK_FILTER_LINEAR;
	create_info.minFilter = VK_FILTER_LINEAR;
	create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	create_info.mipLodBias = 0.0f;
	create_info.anisotropyEnable = VK_FALSE;
	create_info.maxAnisotropy = 0.0f;
	create_info.compareEnable = VK_FALSE;
	create_info.compareOp = VK_COMPARE_OP_LESS;
	create_info.minLod = 0.0f;
	create_info.maxLod = 1.0f;
	create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	create_info.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK(vkCreateSampler(m_context->get_device(), &create_info, m_context->get_allocation_callbacks(), &m_sampler), "Failed to create sampler");
}


Sampler::~Sampler()
{
	if (m_sampler != VK_NULL_HANDLE)
		vkDestroySampler(m_context->get_device(), m_sampler, m_context->get_allocation_callbacks());
}

Sampler::Sampler(Sampler&& other)
{
	move_from(std::move(other));
}

Sampler& Sampler::operator=(Sampler&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

void Sampler::move_from(Sampler&& other)
{
	m_context = other.m_context;
	m_sampler = other.m_sampler;
	other.m_sampler = VK_NULL_HANDLE;
}
