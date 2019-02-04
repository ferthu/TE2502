#pragma once

#include <vulkan/vulkan.h>

#include "vulkan_context.hpp"

// Describes a VkRenderPass
// Currently only supports one subpass and one color attachment
class RenderPass
{
public:
	RenderPass();
	RenderPass(VulkanContext& context, VkFormat color_format, VkImageLayout color_initial_layout, VkImageLayout color_final_layout, bool clear_color_image, 
		bool use_depth, bool clear_depth = false, VkImageLayout depth_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout depth_final_layout = VK_IMAGE_LAYOUT_UNDEFINED);
	~RenderPass();

	RenderPass(RenderPass&& other);
	RenderPass& operator=(RenderPass&& other);

	VkRenderPass get_render_pass();

	VkFormat get_color_format();

	VkImageLayout get_color_initial_layout();

	VkImageLayout get_color_final_layout();

private:
	// Move other into this
	void move_from(RenderPass&& other);

	// Destroy object
	void destroy();

	VulkanContext* m_context;

	VkRenderPass m_render_pass = VK_NULL_HANDLE;

	VkFormat m_color_format;
	VkImageLayout m_color_initial_layout;
	VkImageLayout m_color_final_layout;
};

