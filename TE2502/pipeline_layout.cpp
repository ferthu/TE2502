#include <assert.h>

#include "pipeline_layout.hpp"
#include "utilities.hpp"

PipelineLayout::PipelineLayout(VulkanContext& vulkan_context) : m_context(&vulkan_context), m_is_created(false)
{
}

PipelineLayout::~PipelineLayout()
{
	if (m_is_created)
		vkDestroyPipelineLayout(m_context->get_device(), m_pipeline_layout, m_context->get_allocation_callbacks());
}

PipelineLayout::PipelineLayout(PipelineLayout&& other)
{
	move_from(std::move(other));
}

PipelineLayout& PipelineLayout::operator=(PipelineLayout&& other)
{
	if (this != &other)
	{
		move_from(std::move(other));
	}

	return *this;
}

void PipelineLayout::add_descriptor_set_layout(DescriptorSetLayout& descriptor_set_layout)
{
	m_descriptor_set_layouts.push_back(descriptor_set_layout.get_descriptor_set_layout());
}

void PipelineLayout::create(VkPushConstantRange* push_constant_range)
{
	m_is_created = true;

	VkPipelineLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.pNext = nullptr;
	layout_info.flags = 0;
	layout_info.setLayoutCount = static_cast<uint32_t>(m_descriptor_set_layouts.size());
	layout_info.pSetLayouts = m_descriptor_set_layouts.size() > 0 ? m_descriptor_set_layouts.data() : nullptr;
	if (push_constant_range)
	{
		layout_info.pushConstantRangeCount = 1;
		layout_info.pPushConstantRanges = push_constant_range;
	}
	else
	{
		layout_info.pushConstantRangeCount = 0;
		layout_info.pPushConstantRanges = nullptr;
	}

	VK_CHECK(vkCreatePipelineLayout(m_context->get_device(), &layout_info, m_context->get_allocation_callbacks(), &m_pipeline_layout), "Failed to create pipeline layout!");
}

VkPipelineLayout PipelineLayout::get_pipeline_layout()
{
	CHECK(m_is_created, "Attempted to get a pipeline layout that has not been created!");

	return m_pipeline_layout;
}

void PipelineLayout::move_from(PipelineLayout&& other)
{
	m_context = other.m_context;
	m_pipeline_layout = other.m_pipeline_layout;
	other.m_pipeline_layout = VK_NULL_HANDLE;
	m_descriptor_set_layouts = std::move(other.m_descriptor_set_layouts);
	m_is_created = other.m_is_created;
	other.m_is_created = false;
}
