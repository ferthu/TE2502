#include <utility>

#include "framebuffer.hpp"
#include "utilities.hpp"

Framebuffer::Framebuffer()
{
}

Framebuffer::Framebuffer(VulkanContext& context) : m_context(&context)
{
}


Framebuffer::~Framebuffer()
{
	destroy();
}

Framebuffer::Framebuffer(Framebuffer&& other)
{
	move_from(std::move(other));
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

void Framebuffer::add_attachment(ImageView& image_view)
{
	m_attachments.push_back(image_view.get_view());
}

void Framebuffer::create(VkRenderPass render_pass, uint32_t width, uint32_t height)
{
	assert(m_framebuffer == VK_NULL_HANDLE);
	assert(m_attachments.size());

	VkFramebufferCreateInfo framebuffer_info;
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.pNext = nullptr;
	framebuffer_info.flags = 0;
	framebuffer_info.renderPass = render_pass;
	framebuffer_info.attachmentCount = static_cast<uint32_t>(m_attachments.size());
	framebuffer_info.pAttachments = m_attachments.data();
	framebuffer_info.width = width;
	framebuffer_info.height = height;
	framebuffer_info.layers = 1;

	VK_CHECK(vkCreateFramebuffer(m_context->get_device(), &framebuffer_info, m_context->get_allocation_callbacks(), &m_framebuffer), 
		"Failed to create frame buffer!");
}

VkFramebuffer Framebuffer::get_framebuffer()
{
	return m_framebuffer;
}

void Framebuffer::move_from(Framebuffer&& other)
{
	destroy();

	m_context = other.m_context;

	m_framebuffer = other.m_framebuffer;
	other.m_framebuffer = VK_NULL_HANDLE;

	m_attachments = std::move(other.m_attachments);
}

void Framebuffer::destroy()
{
	if (m_framebuffer != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(m_context->get_device(), m_framebuffer, m_context->get_allocation_callbacks());
		m_framebuffer = VK_NULL_HANDLE;
	}
}
