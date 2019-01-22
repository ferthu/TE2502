#include <vector>
#include <iostream>
#include <assert.h>
#include <intrin.h>

#include "vulkan_context.hpp"

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_error_callback(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char*                 pLayerPrefix,
	const char*                 pMessage,
	void*                       pUserData)
{
	std::cerr << "Vulkan: [" << pLayerPrefix << "] "<< pMessage << "\n";

#ifdef _DEBUG
	__debugbreak();
#endif

	return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_warning_callback(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char*                 pLayerPrefix,
	const char*                 pMessage,
	void*                       pUserData)
{
	std::cerr << "Vulkan: [" << pLayerPrefix << "] " << pMessage << "\n";

	return VK_FALSE;
}

VulkanContext::VulkanContext()
{
	// Not using custom allocation
	m_allocation_callbacks = nullptr;

	init_instance_layer_descriptions();
	init_instance_extension_descriptions();
#ifdef _DEBUG
	print_instance_layers();
	print_instance_extensions();
#endif

	create_instance();

	VkPhysicalDevice selected_physical_device = select_physical_device();
	init_device_extension_descriptions(selected_physical_device);
#ifdef _DEBUG
	print_device_extensions();
#endif
}

VulkanContext::~VulkanContext()
{
#ifdef _DEBUG
	// Destroy debug callbacks
	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
		reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>
		(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT"));

	vkDestroyDebugReportCallbackEXT(m_instance, m_error_callback, m_allocation_callbacks);
	vkDestroyDebugReportCallbackEXT(m_instance, m_warning_callback, m_allocation_callbacks);
#endif

	vkDestroyInstance(m_instance, m_allocation_callbacks);
}

void VulkanContext::create_instance()
{
	const char* appname = "TE2502";
	const char* enginename = "Fritjof Engine";
	uint32_t appversion = 1;
	uint32_t engineversion = 1;

	VkApplicationInfo application_info;
	application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application_info.pNext = nullptr;
	application_info.pApplicationName = appname;
	application_info.applicationVersion = appversion;
	application_info.pEngineName = enginename;
	application_info.engineVersion = engineversion;
	application_info.apiVersion = VK_MAKE_VERSION(1, 1, 0);

	const char* layers = "VK_LAYER_LUNARG_standard_validation";
	VkInstanceCreateInfo instance_info;
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pNext = nullptr;
	instance_info.flags = 0;
	instance_info.pApplicationInfo = &application_info;
#ifdef _DEBUG
	if (instance_layer_available("VK_LAYER_LUNARG_standard_validation"))
	{
		instance_info.enabledLayerCount = 1;
		instance_info.ppEnabledLayerNames = &layers;
	}
	else
	{
		instance_info.enabledLayerCount = 0;
		instance_info.ppEnabledLayerNames = nullptr;
	}
#else
	instance_info.enabledLayerCount = 0;
	instance_info.ppEnabledLayerNames = nullptr;
#endif

	assert(instance_extension_available("VK_KHR_surface") && instance_extension_available("VK_KHR_win32_surface"));

	std::vector<const char*> extensions;
	extensions.push_back("VK_KHR_surface");
	extensions.push_back("VK_KHR_win32_surface");
#ifdef _DEBUG
	extensions.push_back("VK_EXT_debug_report");
#endif

	instance_info.enabledExtensionCount = extensions.size();
	instance_info.ppEnabledExtensionNames = extensions.data();

	VkResult result = vkCreateInstance(&instance_info, m_allocation_callbacks, &m_instance);

	assert(result == VK_SUCCESS);

#ifdef _DEBUG

	// Register validation layer callbacks

	// Get addresses of VK_EXT_debug_report functions
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
		reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>
		(vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT"));
	PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT =
		reinterpret_cast<PFN_vkDebugReportMessageEXT>
		(vkGetInstanceProcAddr(m_instance, "vkDebugReportMessageEXT"));

	// Setup error callback creation information
	VkDebugReportCallbackCreateInfoEXT error_callback_create_info;
	error_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	error_callback_create_info.pNext = nullptr;
	error_callback_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
	error_callback_create_info.pfnCallback = &vulkan_debug_error_callback;
	error_callback_create_info.pUserData = nullptr;

	// Register the callback
	result = vkCreateDebugReportCallbackEXT(m_instance, &error_callback_create_info, m_allocation_callbacks, &m_error_callback);
	assert(result == VK_SUCCESS);

	// Setup warning callback creation information
	VkDebugReportCallbackCreateInfoEXT warning_callback_create_info;
	warning_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	warning_callback_create_info.pNext = nullptr;
	warning_callback_create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
	warning_callback_create_info.pfnCallback = &vulkan_debug_warning_callback;
	warning_callback_create_info.pUserData = nullptr;

	// Register the callback
	result = vkCreateDebugReportCallbackEXT(m_instance, &warning_callback_create_info, m_allocation_callbacks, &m_warning_callback);
	assert(result == VK_SUCCESS);

	vkDebugReportMessageEXT(m_instance, VK_DEBUG_REPORT_INFORMATION_BIT_EXT, 
		VkDebugReportObjectTypeEXT::VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT, 0, 0, 0, 
		"manual validation test", 
		">>> Initializing Vulkan validation layer callbacks <<<");
#endif
}

VkPhysicalDevice VulkanContext::select_physical_device()
{
	VkResult result;

	uint32_t physical_device_count = 0;
	result = vkEnumeratePhysicalDevices(m_instance, &physical_device_count, nullptr);
	assert(result == VK_SUCCESS);

	assert(physical_device_count > 0);

	std::vector<VkPhysicalDevice> physical_devices;
	physical_devices.resize(physical_device_count);
	result = vkEnumeratePhysicalDevices(m_instance, &physical_device_count, physical_devices.data());
	assert(result == VK_SUCCESS);

	// Assert that the right number of devices were written
	assert(physical_device_count == physical_devices.size());

	std::vector<VkPhysicalDeviceProperties> physical_device_properties(physical_device_count);
	std::vector<VkPhysicalDeviceFeatures> physical_device_features(physical_device_count);

	// Fill device properties vector with values
	for (size_t i = 0; i < physical_device_count; i++)
	{
		vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_properties[i]);
		vkGetPhysicalDeviceFeatures(physical_devices[i], &physical_device_features[i]);
	}

	for (size_t i = 0; i < physical_device_count; i++)
	{
		if (physical_device_properties[i].deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
#ifdef _DEBUG
			std::cout << "Selected '" << physical_device_properties[i].deviceName << "'\n\n";
#endif
			m_device_properties = physical_device_properties[i];
			m_device_features = physical_device_features[i];

			return physical_devices[i];
		}
	}

	// Default to first device
#ifdef _DEBUG
	std::cout << "No discrete GPU found, defaulting to '" << physical_device_properties[0].deviceName << "'\n\n";
#endif
	m_device_properties = physical_device_properties[0];
	m_device_features = physical_device_features[0];

	return physical_devices[0];
}

void VulkanContext::init_instance_layer_descriptions()
{
	VkResult result;

	uint32_t num_layers = 0;
	result = vkEnumerateInstanceLayerProperties(&num_layers, nullptr);

	m_instance_layer_properties.resize(num_layers);

	if (num_layers > 0)
	{
		result = vkEnumerateInstanceLayerProperties(&num_layers, m_instance_layer_properties.data());
	}
}

void VulkanContext::print_instance_layers()
{
	std::cout << "Instance Layers:\n";

	for (auto& lp : m_instance_layer_properties) {
		std::cout << lp.layerName << " (sv" << lp.specVersion << ", iv" << lp.implementationVersion << ")\n";
		std::cout << "\t" << lp.description << "\n";
	}

	std::cout << "\n";
}

void VulkanContext::init_instance_extension_descriptions()
{
	uint32_t num_extensions = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, nullptr);

	m_instance_extension_properties.resize(num_extensions);

	if (num_extensions > 0)
	{
		vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, m_instance_extension_properties.data());
	}
}

void VulkanContext::init_device_extension_descriptions(VkPhysicalDevice physical_device)
{
	uint32_t num_extensions = 0;
	vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &num_extensions, nullptr);

	m_device_extension_properties.resize(num_extensions);

	if (num_extensions > 0)
	{
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &num_extensions, m_device_extension_properties.data());
	}
}

void VulkanContext::print_instance_extensions()
{
	std::cout << "Instance Extensions:\n";

	for (auto& ep : m_instance_extension_properties) {
		std::cout << ep.extensionName << " (sv" << ep.specVersion <<  ")\n";
	}

	std::cout << "\n";
}

void VulkanContext::print_device_extensions()
{
	std::cout << "Device Extensions:\n";

	for (auto& ep : m_device_extension_properties) {
		std::cout << ep.extensionName << " (sv" << ep.specVersion << ")\n";
	}

	std::cout << "\n";
}

bool VulkanContext::instance_layer_available(const char* layer_name)
{
	for (auto& props : m_instance_layer_properties)
	{
		if (strcmp(layer_name, props.layerName) == 0) 
			return true;
	}

	return false;
}

bool VulkanContext::instance_extension_available(const char* extension_name)
{
	for (auto& props : m_instance_extension_properties)
	{
		if (strcmp(extension_name, props.extensionName) == 0)
			return true;
	}

	return false;
}

bool VulkanContext::device_extension_available(const char* extension_name)
{
	for (auto& props : m_device_extension_properties)
	{
		if (strcmp(extension_name, props.extensionName) == 0)
			return true;
	}

	return false;
}