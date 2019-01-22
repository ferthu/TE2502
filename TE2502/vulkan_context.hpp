#pragma once

#include <vector>
#include <vulkan/vulkan.h>

// Class for handling Vulkan instance
class VulkanContext
{
public:
	VulkanContext();
	~VulkanContext();
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

	// Returns true if the specified layer is available in m_instance_layer_properties
	bool instance_layer_available(const char* layer_name);

	// Returns true if the specified extension is available in m_instance_extension_properties
	bool instance_extension_available(const char* extension_name);

	// Returns true if the specified extension is available in m_device_extension_properties
	bool device_extension_available(const char* extension_name);

	// Selects an appropriate physical Vulkan device and returns it
	VkPhysicalDevice select_physical_device();

	VkInstance m_instance;
	VkDevice m_device;
	VkPhysicalDeviceProperties m_device_properties;
	VkPhysicalDeviceFeatures m_device_features;

	std::vector<VkLayerProperties> m_instance_layer_properties;

	std::vector<VkExtensionProperties> m_instance_extension_properties;
	std::vector<VkExtensionProperties> m_device_extension_properties;

	VkAllocationCallbacks* m_allocation_callbacks;

#ifdef _DEBUG
	VkDebugReportCallbackEXT m_error_callback;
	VkDebugReportCallbackEXT m_warning_callback;
#endif
};