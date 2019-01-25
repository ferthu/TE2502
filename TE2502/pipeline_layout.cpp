#include <assert.h>

#include "pipeline_layout.hpp"
#include "utilities.hpp"

PipelineLayout::PipelineLayout(VulkanContext& vulkan_context) : m_context(vulkan_context), m_is_created(false)
{
}

PipelineLayout::~PipelineLayout()
{
	if (m_is_created)
	{
		vkDestroyPipelineLayout(m_context.get_device(), m_pipeline_layout, m_context.get_allocation_callbacks());
	}
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

	if (vkCreatePipelineLayout(m_context.get_device(), &layout_info, m_context.get_allocation_callbacks(), &m_pipeline_layout) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to create pipeline layout!");
		exit(1);
#endif	// _DEBUG
	}
}

VkPipelineLayout PipelineLayout::get_pipeline_layout()
{
	if (!m_is_created)
	{
#ifdef  _DEBUG
		__debugbreak();
#else
		println("Attempted to get a pipeline layout that has not been created!");
		exit(1);
#endif // _DEBUG
	}

	return m_pipeline_layout;
}
