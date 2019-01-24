#pragma once

#include <vector>
#include <memory>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
//#include <shaderc/shaderc.hpp>
#include <glm/glm.hpp>

#include "pipeline.h"

class Window;

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

	// Creates the render pass 
	void create_render_pass(const Window* window);

	// Create a compute pipeline
	std::unique_ptr<Pipeline> create_compute_pipeline();

	// Create a graphics pipeline
	std::unique_ptr<Pipeline> create_graphics_pipeline(const glm::vec2 window_size);

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

	// Writes the required features into a VkPhysicalDeviceFeatures struct
	void write_required_features(VkPhysicalDeviceFeatures& features);

	// Compiles a shader to a SPIR-V binary. Returns the binary as a vector of 32-bit words.
	//std::vector<char> compile_from_file(const std::string& file_name, shaderc_shader_kind kind);

	// Return a VkShaderModule using the given byte code
	VkShaderModule create_shader_module(const std::vector<char>& code);

	VkRenderPass m_render_pass;

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
		VkBool32 supports_presentation;
	};

	// Index of queue families for different capabilities
	QueueFamily m_graphics_queue_family;
	QueueFamily m_compute_queue_family;
	QueueFamily m_transfer_queue_family;

#ifdef _DEBUG
	VkDebugReportCallbackEXT m_error_callback;
	VkDebugReportCallbackEXT m_warning_callback;
#endif
};