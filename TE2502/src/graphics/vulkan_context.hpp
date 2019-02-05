#pragma once

#include <vector>
#include <memory>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
//#include <shaderc/shaderc.hpp>
#include <glm/glm.hpp>

#include "graphics_queue.hpp"
#include "compute_queue.hpp"
#include "transfer_queue.hpp"
#include "gpu_memory.hpp"
#include "pipeline_layout.hpp"
#include "descriptor_set_layout.hpp"
#include "vertex_attributes.hpp"

#include "pipeline.hpp"

class Window;
class Pipeline;
class PipelineLayout;
class DescriptorSetLayout;
class DebugDrawer;
class VertexAttributes;
class RenderPass;

// Class for handling Vulkan instance
class VulkanContext
{
public:
	VulkanContext();
	~VulkanContext();

	// Returns true if the specified layer is available in m_instance_layer_properties
	bool instance_layer_available(const char* layer_name);

	// Returns true if the specified extension is available in m_instance_extension_properties
	bool instance_extension_available(const char* extension_name);

	// Returns true if the specified extension is available in m_device_extension_properties
	bool device_extension_available(const char* extension_name);

	// Returns Vulkan instance
	VkInstance get_instance();

	// Returns Vulkan device
	VkDevice get_device();

	// Returns Vulkan physical device
	VkPhysicalDevice get_physical_device();

	// Returns Vulkan device properties struct
	const VkPhysicalDeviceProperties& get_device_properties();

	// Returns allocation callbacks
	VkAllocationCallbacks* get_allocation_callbacks();

	// Returns number of graphics queues
	size_t graphics_queues_count();

	// Returns number of compute queues
	size_t compute_queue_count();

	// Returns number of transfer queues
	size_t transfer_queues_count();

	// Get graphics queue with specified index
	VkQueue get_graphics_queue(size_t index);

	// Get compute queue with specified index
	VkQueue get_compute_queue(size_t index);

	// Get transfer queue with specified index
	VkQueue get_transfer_queue(size_t index);

	// Creates and returns a GraphicsQueue object
	// Will fail if there are no more queues available
	GraphicsQueue create_graphics_queue();

	// Creates and returns a ComputeQueue object
	// Will fail if there are no more queues available
	ComputeQueue create_compute_queue();

	// Creates and returns a TransferQueue object
	// Will fail if there are no more queues available
	TransferQueue create_transfer_queue();

	// Returns a memory object allocated from GPU
	GPUMemory allocate_device_memory(VkDeviceSize byte_size);

	// Returns a memory object allocated from host
	GPUMemory allocate_host_memory(VkDeviceSize byte_size);

	// Creates and returns a compute pipeline
	std::unique_ptr<Pipeline> create_compute_pipeline(const std::string& shader_name, PipelineLayout& layout);

	// Creates and returns a graphics pipeline
	std::unique_ptr<Pipeline> create_graphics_pipeline(
		const std::string& shader_name, 
		const glm::vec2 window_size, 
		PipelineLayout& layout, 
		VertexAttributes& vertex_attributes, 
		RenderPass& render_pass, 
		bool enable_depth, 
		bool enable_geometry_shader,
		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	// Allocate a descriptor set from descriptor pool
	VkDescriptorSet allocate_descriptor_set(DescriptorSetLayout& layout);

	// Free a descriptor set allocated from descriptor pool
	void free_descriptor_set(VkDescriptorSet descriptor_set);

	// Returns queue family index of graphics queues
	uint32_t get_graphics_queue_index();

	// Returns queue family index of compute queues
	uint32_t get_compute_queue_index();

	// Returns internal descriptor pool
	VkDescriptorPool get_descriptor_pool();

private:
	// Creates the VkInstance
	void create_instance();

	// Initializes m_instance_layer_properties
	void init_instance_layer_descriptions();

	// Prints the available instance layers
	void print_instance_layers();

	// Initializes m_instance_extension_properties
	void init_instance_extension_descriptions();

	// Initializes m_device_extension_properties
	void init_device_extension_descriptions(VkPhysicalDevice physical_device);

	// Prints the available instance extensions
	void print_instance_extensions();

	// Prints the available device extensions
	void print_device_extensions();

	// Selects an appropriate physical Vulkan device and returns it
	VkPhysicalDevice select_physical_device();

	// Gets and stores memory properties of a physical device
	void get_memory_properties(VkPhysicalDevice physical_device);

	// Prints info on device memory properties from m_memory_properties
	void print_memory_properties();

	// Gets and stores queue family properties of a physical device
	void get_queue_family_properties(VkPhysicalDevice physical_device);

	// Prints info on device queue family properties from m_queue_family_properties
	void print_queue_family_properties();

	// Creates a VkDevice from a VkPhysicalDevice
	void create_device(VkPhysicalDevice physical_device);

	// Attempts to find an appropriate queue family from m_queue_family_properties
	// Does not write to output if not found
	void find_queue_family(uint32_t& output, VkQueueFlagBits required, VkQueueFlagBits not_allowed);

	// Attempts to find memory type with specified flags in m_memory_properties. If found, return true and write index to 'output', else return false and do not write
	bool find_memory_type(uint32_t& output, VkMemoryPropertyFlagBits flags);

	// Writes the required features into a VkPhysicalDeviceFeatures struct
	void write_required_features(VkPhysicalDeviceFeatures& features);

	// Gets queues from device and places them in m_graphics_queue_family etc.
	void get_queues();

	// Creates command pools for queue families
	void create_command_pools();

	// Compiles a shader to a SPIR-V binary. Returns the binary as a vector of 32-bit words.
	//std::vector<char> compile_from_file(const std::string& file_name, shaderc_shader_kind kind);

	// Return a VkShaderModule using the given byte code
	VkShaderModule create_shader_module(const std::vector<char>& code);

	// Initialize the descriptor pool
	void create_descriptor_pool();

	VkInstance m_instance;
	VkPhysicalDevice m_physical_device;
	VkDevice m_device;
	VkPhysicalDeviceProperties m_device_properties;
	VkPhysicalDeviceFeatures m_device_features;
	VkPhysicalDeviceMemoryProperties m_memory_properties;
	std::vector<VkQueueFamilyProperties> m_queue_family_properties;

	std::vector<VkLayerProperties> m_instance_layer_properties;

	std::vector<VkExtensionProperties> m_instance_extension_properties;
	std::vector<VkExtensionProperties> m_device_extension_properties;

	VkAllocationCallbacks* m_allocation_callbacks;

	// Info on a created device queue
	struct QueueFamily
	{
		uint32_t family_index;
		uint32_t queue_count;
		uint32_t next_free = 0; // Index of next unused queue
		VkBool32 supports_presentation;
		std::vector<VkQueue> queues;
	};

	// Index of queue families for different capabilities
	QueueFamily m_graphics_queue_family;
	QueueFamily m_compute_queue_family;
	QueueFamily m_transfer_queue_family;

	VkCommandPool m_graphics_command_pool;
	VkCommandPool m_compute_command_pool;
	VkCommandPool m_transfer_command_pool;

	VkDescriptorPool m_descriptor_pool;

	// Memory type to use when allocating device memory
	uint32_t m_device_memory_type;

	// Memory type to use when allocating host memory
	uint32_t m_host_memory_type;

#ifdef _DEBUG
	VkDebugReportCallbackEXT m_error_callback;
	VkDebugReportCallbackEXT m_warning_callback;
#endif
};