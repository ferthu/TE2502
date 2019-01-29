#include "descriptor_set.hpp"



DescriptorSet::DescriptorSet(VulkanContext& vulkan_context, DescriptorSetLayout& layout) : m_context(&vulkan_context), m_layout(&layout)
{
	m_descriptor_set = m_context->allocate_descriptor_set(layout);

	m_descriptors.resize(10);
	m_image_desc.resize(10);
	m_buffer_desc.resize(10);
}


DescriptorSet::~DescriptorSet()
{
	destroy();
}

DescriptorSet::DescriptorSet(DescriptorSet&& other)
{
	move_from(std::move(other));
}

DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other)
{
	if (this != &other)
	{
		move_from(std::move(other));
	}

	return *this;
}

void DescriptorSet::bind()
{
	vkUpdateDescriptorSets(m_context->get_device(), static_cast<uint32_t>(m_descriptors.size()), m_descriptors.data(), 0, nullptr);
}

void DescriptorSet::clear()
{
	m_descriptors.clear();
	m_image_desc.clear();
	m_buffer_desc.clear();
}

void DescriptorSet::add_sampler(Sampler& sampler)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);

	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	m_descriptors[index].pImageInfo = &m_image_desc[index];
	m_image_desc[index].sampler = sampler.get_sampler();
}

void DescriptorSet::add_sampled_image(ImageView& image_view, VkImageLayout layout)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);
	
	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	m_descriptors[index].pImageInfo = &m_image_desc[index];
	m_image_desc[index].imageView = image_view.get_view();
	m_image_desc[index].imageLayout = layout;
}

void DescriptorSet::add_combined_image_sampler(ImageView& image_view, VkImageLayout layout, Sampler& sampler)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);

	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	m_descriptors[index].pImageInfo = &m_image_desc[index];
	m_image_desc[index].imageView = image_view.get_view();
	m_image_desc[index].imageLayout = layout;
	m_image_desc[index].sampler = sampler.get_sampler();
}

void DescriptorSet::add_storage_image(ImageView& image_view, VkImageLayout layout)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);

	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	m_descriptors[index].pImageInfo = &m_image_desc[index];
	m_image_desc[index].imageView = image_view.get_view();
	m_image_desc[index].imageLayout = layout;
}

void DescriptorSet::add_uniform_texel_buffer(BufferView& buffer_view)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);

	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

	VkBufferView view = buffer_view.get_view();
	m_descriptors[index].pTexelBufferView = &view;
}

void DescriptorSet::add_storage_texel_buffer(BufferView& buffer_view)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);

	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

	VkBufferView view = buffer_view.get_view();
	m_descriptors[index].pTexelBufferView = &view;
}

void DescriptorSet::add_uniform_buffer(GPUBuffer& buffer)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);

	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	m_descriptors[index].pBufferInfo = &m_buffer_desc[index];
	m_buffer_desc[index].buffer = buffer.get_buffer();
	m_buffer_desc[index].offset = 0;
	m_buffer_desc[index].range = VK_WHOLE_SIZE;
}

void DescriptorSet::add_storage_buffer(GPUBuffer& buffer)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);

	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	m_descriptors[index].pBufferInfo = &m_buffer_desc[index];
	m_buffer_desc[index].buffer = buffer.get_buffer();
	m_buffer_desc[index].offset = 0;
	m_buffer_desc[index].range = VK_WHOLE_SIZE;
}

void DescriptorSet::add_input_attachment(ImageView& image_view, VkImageLayout layout)
{
	size_t index = m_descriptors.size();
	push_back();
	fill_write_descriptor_set(index);

	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	m_descriptors[index].pImageInfo = &m_image_desc[index];
	m_image_desc[index].imageView = image_view.get_view();
	m_image_desc[index].imageLayout = layout;
}

void DescriptorSet::move_from(DescriptorSet&& other)
{
	destroy();

	m_descriptor_set = other.m_descriptor_set;
	other.m_descriptor_set = VK_NULL_HANDLE;

	m_context = other.m_context;
	m_layout = other.m_layout;

	m_descriptors = std::move(other.m_descriptors);
	m_image_desc = std::move(other.m_image_desc);
	m_buffer_desc = std::move(other.m_buffer_desc);
}

void DescriptorSet::destroy()
{
	if (m_descriptor_set != VK_NULL_HANDLE)
	{
		m_context->free_descriptor_set(m_descriptor_set);
		m_descriptor_set = VK_NULL_HANDLE;
	}
}

void DescriptorSet::fill_write_descriptor_set(size_t index)
{
	m_descriptors[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	m_descriptors[index].pNext = nullptr;
	m_descriptors[index].dstSet = m_descriptor_set;
	m_descriptors[index].dstBinding = static_cast<uint32_t>(m_descriptors.size() - 1);
	m_descriptors[index].dstArrayElement = 0;
	m_descriptors[index].descriptorCount = 1;
	
	// These should be filled in later by the caller
	m_descriptors[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; 
	m_descriptors[index].pImageInfo = nullptr;
	m_descriptors[index].pBufferInfo = nullptr;
	m_descriptors[index].pTexelBufferView = nullptr;
}

void DescriptorSet::push_back()
{
	m_descriptors.push_back(VkWriteDescriptorSet());
	m_image_desc.push_back(VkDescriptorImageInfo());
	m_buffer_desc.push_back(VkDescriptorBufferInfo());
}
