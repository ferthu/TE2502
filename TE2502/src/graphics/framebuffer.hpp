#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "vulkan_context.hpp"
#include "gpu_image.hpp"

// Wrapper around a VkFramebuffer
// Usage: add attachments with add_attachment(), then create framebuffer with create()
class Framebuffer
{
public:
	Framebuffer();
	Framebuffer(VulkanContext& context);
	~Framebuffer();

	Framebuffer(Framebuffer&& other);
	Framebuffer& operator=(Framebuffer&& other);

	// Adds an image attachment to the framebuffer
	void add_attachment(ImageView& image_view);

	// Create the framebuffer object
	void create(VkRenderPass render_pass, uint32_t width, uint32_t height);

	VkFramebuffer get_framebuffer();

private:
	// Move other into this
	void move_from(Framebuffer&& other);

	// Destroy object
	void destroy();

	// Pointers to image views to be used as attachments
	std::vector<VkImageView> m_attachments;

	VulkanContext* m_context;

	VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
};

