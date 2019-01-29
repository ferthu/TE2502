#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "vulkan_context.hpp"
#include "descriptor_set_layout.hpp"

class DescriptorSetLayout;

// Describes the layout of a pipeline's resource access 
// Usage: add descriptor set layouts with add(), then create Vulkan object with create() function
class PipelineLayout
{
public:
	PipelineLayout() {};
	PipelineLayout(VulkanContext& vulkan_context);
	~PipelineLayout();

	PipelineLayout(PipelineLayout&& other);
	PipelineLayout& operator=(PipelineLayout&& other);

	// Add a descriptor set layout
	void add_descriptor_set_layout(DescriptorSetLayout& descriptor_set_layout);

	// Create VkPipelineLayout from added descriptor set layouts. Pass nullptr if push constant range is not used
	void create(VkPushConstantRange* push_constant_range);

	VkPipelineLayout get_pipeline_layout();

private:
	// Moves other into this
	void move_from(PipelineLayout&& other);
	
	// Destroys object
	void destroy();

	VulkanContext* m_context;

	VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

	std::vector<VkDescriptorSetLayout> m_descriptor_set_layouts;

	bool m_is_created;
};

