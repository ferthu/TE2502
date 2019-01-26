#include "render_pass.hpp"
#include "utilities.hpp"


RenderPass::RenderPass()
{
}

RenderPass::RenderPass(VulkanContext& context, VkFormat color_format, VkImageLayout color_initial_layout, VkImageLayout color_final_layout) : m_context(&context)
{
	VkAttachmentDescription attachment_desc;
	attachment_desc.flags = 0;
	attachment_desc.format = color_format;
	attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment_desc.initialLayout = color_initial_layout;
	attachment_desc.finalLayout = color_final_layout;

	VkAttachmentReference attachment_ref;
	attachment_ref.attachment = 0;
	attachment_ref.layout = color_initial_layout;

	VkSubpassDescription subpass_desc;
	subpass_desc.flags = 0;
	subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_desc.inputAttachmentCount = 0;
	subpass_desc.pInputAttachments = nullptr;
	subpass_desc.colorAttachmentCount = 1;
	subpass_desc.pColorAttachments = &attachment_ref;
	subpass_desc.pResolveAttachments = nullptr;
	subpass_desc.pDepthStencilAttachment = nullptr;
	subpass_desc.preserveAttachmentCount = 0;
	subpass_desc.pPreserveAttachments = nullptr;

	VkRenderPassCreateInfo render_pass_info;
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.pNext = nullptr;
	render_pass_info.flags = 0;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &attachment_desc;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass_desc;
	render_pass_info.dependencyCount = 0;
	render_pass_info.pDependencies = nullptr;

	VK_CHECK(vkCreateRenderPass(m_context->get_device(), &render_pass_info, m_context->get_allocation_callbacks(), &m_render_pass), 
		"Render pass creation failed!");
}


RenderPass::~RenderPass()
{
	destroy();
}

RenderPass::RenderPass(RenderPass&& other)
{
	move_from(std::move(other));
}

RenderPass& RenderPass::operator=(RenderPass&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

VkRenderPass RenderPass::get_render_pass()
{
	return m_render_pass;
}

VkFormat RenderPass::get_color_format()
{
	return m_color_format;
}

VkImageLayout RenderPass::get_color_initial_layout()
{
	return m_color_initial_layout;
}

VkImageLayout RenderPass::get_color_final_layout()
{
	return m_color_final_layout;
}

void RenderPass::move_from(RenderPass&& other)
{
	destroy();

	m_context = other.m_context;
	m_render_pass = other.m_render_pass;
	other.m_render_pass = VK_NULL_HANDLE;

	m_color_format = other.m_color_format;
	m_color_initial_layout = other.m_color_initial_layout;
	m_color_final_layout = other.m_color_final_layout;
}

void RenderPass::destroy()
{
	if (m_render_pass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(m_context->get_device(), m_render_pass, m_context->get_allocation_callbacks());
		m_render_pass = VK_NULL_HANDLE;
	}
}
