#include <assert.h>

#include "utilities.hpp"
#include "descriptor_set_layout.hpp"



DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other)
{
	move_from(std::move(other));
}


DescriptorSetLayout& DescriptorSetLayout::operator=(DescriptorSetLayout&& other)
{
	if (this != &other)
	{
		move_from(std::move(other));
	}

	return *this;
}

DescriptorSetLayout::DescriptorSetLayout(VulkanContext& vulkan_context) : m_is_created(false), m_context(&vulkan_context)
{
}


DescriptorSetLayout::~DescriptorSetLayout()
{
	destroy();
}

void DescriptorSetLayout::create()
{
	assert(m_bindings.size() > 0);

	m_is_created = true;

	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {};
	descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_layout_info.pNext = nullptr;
	descriptor_set_layout_info.flags = 0;
	descriptor_set_layout_info.bindingCount = static_cast<uint32_t>(m_bindings.size());
	descriptor_set_layout_info.pBindings = m_bindings.data();

	if (vkCreateDescriptorSetLayout(m_context->get_device(), &descriptor_set_layout_info, nullptr, &m_descriptor_set_layout) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to create descriptor set layout!");
		exit(1);
#endif
	}
}

VkDescriptorSetLayout DescriptorSetLayout::get_descriptor_set_layout()
{
	if (!m_is_created)
	{
#ifdef  _DEBUG
		__debugbreak();
#else
		println("Attempted to get a descriptor set layout that has not been created!");
		exit(1);
#endif //  _DEBUG

	}

	return m_descriptor_set_layout;
}

void DescriptorSetLayout::add_sampler(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_SAMPLER);
}

void DescriptorSetLayout::add_sampled_image(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
}

void DescriptorSetLayout::add_combined_image_sampler(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

void DescriptorSetLayout::add_storage_image(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}

void DescriptorSetLayout::add_uniform_texel_buffer(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
}

void DescriptorSetLayout::add_storage_texel_buffer(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
}

void DescriptorSetLayout::add_uniform_buffer(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void DescriptorSetLayout::add_storage_buffer(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void DescriptorSetLayout::add_input_attachment(VkShaderStageFlags stage_flags)
{
	create_binding(stage_flags, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
}

void DescriptorSetLayout::move_from(DescriptorSetLayout&& other)
{
	destroy();

	m_context = other.m_context;

	m_descriptor_set_layout = other.m_descriptor_set_layout;
	other.m_descriptor_set_layout = VK_NULL_HANDLE;

	m_bindings = std::move(other.m_bindings);

	m_is_created = other.m_is_created;
	other.m_is_created = false;
}

void DescriptorSetLayout::destroy()
{
	if (m_is_created)
	{
		vkDestroyDescriptorSetLayout(m_context->get_device(), m_descriptor_set_layout, m_context->get_allocation_callbacks());
		m_descriptor_set_layout = VK_NULL_HANDLE;
		m_is_created = false;
	}
}

void DescriptorSetLayout::create_binding(VkShaderStageFlags stage_flags, VkDescriptorType type)
{
	VkDescriptorSetLayoutBinding binding;
	binding.binding = static_cast<uint32_t>(m_bindings.size());
	binding.descriptorType = type;
	binding.descriptorCount = 1;
	binding.stageFlags = stage_flags;
	binding.pImmutableSamplers = nullptr;

	m_bindings.push_back(binding);
}
