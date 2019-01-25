#pragma once

#include <vector>

#include "utilities.hpp"
#include "vulkan_context.hpp"
#include "descriptor_set_layout.hpp"
#include "gpu_image.hpp"
#include "gpu_buffer.hpp"
#include "sampler.hpp"

// Contains a VkDescriptorSet
// Usage: add descriptors with add*() functions, then bind them to the descriptor set with bind()
// Clear descriptor storage with clear(). This does not unbind resources from the set
class DescriptorSet
{
public:
	DescriptorSet(VulkanContext& vulkan_context, DescriptorSetLayout& layout);
	~DescriptorSet();

	VkDescriptorSet get_descriptor_set() { return m_descriptor_set; }

	// Bind descriptors added with add*() functions to the descriptor set
	void bind();

	// Clear the descriptor cache
	void clear();

	// Adds a sampler to the set
	void add_sampler(Sampler& sampler);

	// Adds a sampled image to the set
	void add_sampled_image(ImageView& image_view, VkImageLayout layout);

	// Adds a combined image-sampler image to the set
	void add_combined_image_sampler(ImageView& image_view, VkImageLayout layout, Sampler& sampler);

	// Adds a storage (non-sampled) image to the set
	void add_storage_image(ImageView& image_view, VkImageLayout layout);

	// Adds a uniform texel buffer image to the set
	void add_uniform_texel_buffer(BufferView& buffer_view);

	// Adds a storage texel buffer to the set
	void add_storage_texel_buffer(BufferView& buffer_view);

	// Adds a uniform buffer to the set
	void add_uniform_buffer(GPUBuffer& buffer);

	// Adds a storage buffer to the set
	void add_storage_buffer(GPUBuffer& buffer);

	// Adds an input attachment to the set
	void add_input_attachment(ImageView& image_view, VkImageLayout layout);

private:
	VkDescriptorSet m_descriptor_set;

	// Fills a VkWriteDescriptorSet in m_descriptors[index] with common values
	void fill_write_descriptor_set(size_t index);

	// Calls push_back on m_descriptors, m_image_desc and m_buffer_desc
	void push_back();

	VulkanContext& m_context;
	DescriptorSetLayout& m_layout;

	// Contains the descriptors to bind to the descriptor set
	std::vector<VkWriteDescriptorSet> m_descriptors;
	std::vector<VkDescriptorImageInfo> m_image_desc;
	std::vector<VkDescriptorBufferInfo> m_buffer_desc;
};