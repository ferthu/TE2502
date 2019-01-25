#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "vulkan_context.hpp"

// Describes the layout of a descriptor set 
// Usage: add descriptor slots with add*() functions, then create Vulkan object with create() function
class DescriptorSetLayout
{
public:
	DescriptorSetLayout(VulkanContext& vulkan_context);
	~DescriptorSetLayout();

	// Creates the Vulkan object from descriptors added with add*() functions
	void create();

	// Returns descriptor set layout. create() must have been called before this
	VkDescriptorSetLayout get_descriptor_set_layout();

	// Adds a sampler to the set
	void add_sampler(VkShaderStageFlags stage_flags);

	// Adds a sampled image to the set
	void add_sampled_image(VkShaderStageFlags stage_flags);

	// Adds a combined image-sampler image to the set
	void add_combined_image_sampler(VkShaderStageFlags stage_flags);

	// Adds a storage (non-sampled) image to the set
	void add_storage_image(VkShaderStageFlags stage_flags);

	// Adds a uniform texel buffer image to the set
	void add_uniform_texel_buffer(VkShaderStageFlags stage_flags);

	// Adds a storage texel buffer to the set
	void add_storage_texel_buffer(VkShaderStageFlags stage_flags);

	// Adds a uniform buffer to the set
	void add_uniform_buffer(VkShaderStageFlags stage_flags);

	// Adds a storage buffer to the set
	void add_storage_buffer(VkShaderStageFlags stage_flags);

	// Adds an input attachment to the set
	void add_input_attachment(VkShaderStageFlags stage_flags);

private:
	void create_binding(VkShaderStageFlags stage_flags, VkDescriptorType type);

	VulkanContext& m_context;

	VkDescriptorSetLayout m_descriptor_set_layout;

	// Bindings that will be used when creating descriptor set
	std::vector<VkDescriptorSetLayoutBinding> m_bindings;

	// True if the descriptor set has been created
	bool m_is_created;
};