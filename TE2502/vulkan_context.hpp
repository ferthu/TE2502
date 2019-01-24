#pragma once

#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "graphics_queue.hpp"
#include "compute_queue.hpp"
#include "transfer_queue.hpp"

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

	// Gets queues from device and places them in m_graphics_queue_family etc.
	void get_queues();

	// Creates command pools for queue families
	void create_command_pools();

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

#ifdef _DEBUG
	VkDebugReportCallbackEXT m_error_callback;
	VkDebugReportCallbackEXT m_warning_callback;
#endif
};