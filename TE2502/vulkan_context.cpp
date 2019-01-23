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

	m_physical_device = select_physical_device();
	init_device_extension_descriptions(m_physical_device);
	get_memory_properties(m_physical_device);
	get_queue_family_properties(m_physical_device);

#ifdef _DEBUG
	print_device_extensions();
	print_memory_properties();
	print_queue_family_properties();
#endif

	create_device(m_physical_device);

	get_queues();

	create_command_pools();
}

VulkanContext::~VulkanContext()
{
	vkDeviceWaitIdle(m_device);

	vkDestroyCommandPool(m_device, m_graphics_command_pool, m_allocation_callbacks);
	vkDestroyCommandPool(m_device, m_compute_command_pool, m_allocation_callbacks);
	vkDestroyCommandPool(m_device, m_transfer_command_pool, m_allocation_callbacks);

	vkDestroyDevice(m_device, m_allocation_callbacks);

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

	instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
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
		"Timmie", 
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

void VulkanContext::get_memory_properties(VkPhysicalDevice physical_device)
{
	vkGetPhysicalDeviceMemoryProperties(physical_device, &m_memory_properties);
}

void VulkanContext::print_memory_properties()
{
	std::cout << "Memory Heaps:\n";

	for (uint32_t i = 0; i < m_memory_properties.memoryHeapCount; i++)
	{
		std::cout << i << ": " << m_memory_properties.memoryHeaps[i].size / 1024 / 1024 << "MB\n";
		std::cout << "\tFlags: ";
		if (m_memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) std::cout << "VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ";
		if (m_memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) std::cout << "VK_MEMORY_HEAP_MULTI_INSTANCE_BIT ";
		std::cout << "\n";
	}

	std::cout << "\nMemory Types:\n";

	for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; i++)
	{
		std::cout << i << ": Heap " << m_memory_properties.memoryTypes[i].heapIndex << "\n";
		std::cout << "\tFlags: ";
		if (m_memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) std::cout << "VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ";
		if (m_memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) std::cout << "VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ";
		if (m_memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) std::cout << "VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ";
		if (m_memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) std::cout << "VK_MEMORY_PROPERTY_HOST_CACHED_BIT ";
		if (m_memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) std::cout << "VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT ";
		if (m_memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) std::cout << "VK_MEMORY_PROPERTY_PROTECTED_BIT ";
		std::cout << "\n";
	}
	std::cout << "\n";

}

void VulkanContext::get_queue_family_properties(VkPhysicalDevice physical_device)
{
	uint32_t queue_family_count = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

	assert(queue_family_count > 0);

	m_queue_family_properties.resize(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, m_queue_family_properties.data());

	assert(queue_family_count == m_queue_family_properties.size());
}

void VulkanContext::print_queue_family_properties()
{
	std::cout << "Queue Family Properties:\n";

	for (size_t i = 0; i < m_queue_family_properties.size(); i++)
	{
		std::cout << i << ": " << m_queue_family_properties[i].queueCount << " queues, timestampValidBits: " << m_queue_family_properties[i].timestampValidBits << ", flags: ";
		if (m_queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) std::cout << "VK_QUEUE_GRAPHICS_BIT ";
		if (m_queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) std::cout << "VK_QUEUE_COMPUTE_BIT ";
		if (m_queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) std::cout << "VK_QUEUE_TRANSFER_BIT ";
		if (m_queue_family_properties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) std::cout << "VK_QUEUE_SPARSE_BINDING_BIT ";
		if (m_queue_family_properties[i].queueFlags & VK_QUEUE_PROTECTED_BIT) std::cout << "VK_QUEUE_PROTECTED_BIT ";
		std::cout << "\n";
	}
}

void VulkanContext::create_device(VkPhysicalDevice physical_device)
{
	// Select queues to use for different operations
	const uint32_t not_found = 999999;
	m_graphics_queue_family.family_index = not_found;

	// Try to find exclusive graphics family
	find_queue_family(m_graphics_queue_family.family_index, VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT);

	// If no exclusive family was found, find any graphics family
	if (m_graphics_queue_family.family_index == not_found)
	{
		find_queue_family(m_graphics_queue_family.family_index, VK_QUEUE_GRAPHICS_BIT, (VkQueueFlagBits)0);
	}

	assert(m_graphics_queue_family.family_index != not_found);
	m_graphics_queue_family.queue_count = m_queue_family_properties[m_graphics_queue_family.family_index].queueCount;
	m_graphics_queue_family.supports_presentation = vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, m_graphics_queue_family.family_index);
	assert(m_graphics_queue_family.supports_presentation == VK_TRUE);

	m_compute_queue_family.family_index = not_found;

	// Try to find exclusive compute family
	find_queue_family(m_compute_queue_family.family_index, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT);

	// If no exclusive family was found, find any compute family
	if (m_compute_queue_family.family_index == not_found)
	{
		find_queue_family(m_compute_queue_family.family_index, VK_QUEUE_COMPUTE_BIT, (VkQueueFlagBits)0);
	}

	assert(m_compute_queue_family.family_index != not_found);
	m_compute_queue_family.queue_count = m_queue_family_properties[m_compute_queue_family.family_index].queueCount;
	m_compute_queue_family.supports_presentation = vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, m_compute_queue_family.family_index);


	m_transfer_queue_family.family_index = not_found;

	// Try to find exclusive transfer family
	find_queue_family(m_transfer_queue_family.family_index, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT);

	// If no exclusive family was found, find any transfer family
	if (m_transfer_queue_family.family_index == not_found)
	{
		find_queue_family(m_transfer_queue_family.family_index, VK_QUEUE_TRANSFER_BIT, (VkQueueFlagBits)0);
	}

	assert(m_transfer_queue_family.family_index != not_found);
	m_transfer_queue_family.queue_count = m_queue_family_properties[m_transfer_queue_family.family_index].queueCount;
	m_transfer_queue_family.supports_presentation = vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, m_transfer_queue_family.family_index);


	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

	// Add graphics queue
	{
		VkDeviceQueueCreateInfo graphics_queue;
		graphics_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		graphics_queue.pNext = nullptr;
		graphics_queue.flags = 0;
		graphics_queue.queueFamilyIndex = m_graphics_queue_family.family_index;
		graphics_queue.queueCount = m_graphics_queue_family.queue_count;
		graphics_queue.pQueuePriorities = nullptr;

		queue_create_infos.push_back(graphics_queue);
	}

	// Add compute queue
	if (m_compute_queue_family.family_index != m_graphics_queue_family.family_index)
	{
		VkDeviceQueueCreateInfo compute_queue;
		compute_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		compute_queue.pNext = nullptr;
		compute_queue.flags = 0;
		compute_queue.queueFamilyIndex = m_compute_queue_family.family_index;
		compute_queue.queueCount = m_compute_queue_family.queue_count;
		compute_queue.pQueuePriorities = nullptr;

		queue_create_infos.push_back(compute_queue);
	}

	// Add transfer queue
	if (m_transfer_queue_family.family_index != m_graphics_queue_family.family_index && 
		m_transfer_queue_family.family_index != m_compute_queue_family.family_index)
	{
		VkDeviceQueueCreateInfo transfer_queue;
		transfer_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		transfer_queue.pNext = nullptr;
		transfer_queue.flags = 0;
		transfer_queue.queueFamilyIndex = m_transfer_queue_family.family_index;
		transfer_queue.queueCount = m_transfer_queue_family.queue_count;
		transfer_queue.pQueuePriorities = nullptr;

		queue_create_infos.push_back(transfer_queue);
	}

#ifdef _DEBUG
	std::cout << "\nSelected Queues:\nGraphics:\t" << m_graphics_queue_family.family_index << ", " << m_graphics_queue_family.queue_count << " queues, can present: " << m_graphics_queue_family.supports_presentation << "\n";
	std::cout << "Compute:\t" << m_compute_queue_family.family_index << ", " << m_compute_queue_family.queue_count << " queues, can present: " << m_compute_queue_family.supports_presentation << "\n";
	std::cout << "Transfer:\t" << m_transfer_queue_family.family_index << ", " << m_transfer_queue_family.queue_count << " queues, can present: " << m_transfer_queue_family.supports_presentation << "\n\n";
#endif

	// Select device extensions
	assert(device_extension_available("VK_KHR_swapchain"));

	const char* extensions[] = { "VK_KHR_swapchain" };

	// Select device features
	VkPhysicalDeviceFeatures device_features;
	write_required_features(device_features);

	VkDeviceCreateInfo device_create_info;
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.pNext = nullptr;
	device_create_info.flags = 0;
	device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	device_create_info.pQueueCreateInfos = queue_create_infos.data();
	device_create_info.enabledLayerCount = 0;
	device_create_info.ppEnabledLayerNames = nullptr;
	device_create_info.enabledExtensionCount = 1;
	device_create_info.ppEnabledExtensionNames = extensions;
	device_create_info.pEnabledFeatures = &device_features;

	VkResult result = vkCreateDevice(physical_device, &device_create_info, m_allocation_callbacks, &m_device);
	assert(result == VK_SUCCESS);
}

void VulkanContext::find_queue_family(uint32_t& output, VkQueueFlagBits required, VkQueueFlagBits not_allowed)
{
	for (size_t i = 0; i < m_queue_family_properties.size(); i++)
	{
		if (((m_queue_family_properties[i].queueFlags & required) == required) && ((m_queue_family_properties[i].queueFlags & not_allowed) == 0))
		{
			output = static_cast<uint32_t>(i);
			return;
		}
	}
}

void VulkanContext::write_required_features(VkPhysicalDeviceFeatures& features)
{
	// Set all contents to zero
	memset(&features, 0, sizeof(VkPhysicalDeviceFeatures));

	// Enable required features
	assert(m_device_features.dualSrcBlend);
	features.dualSrcBlend = VK_TRUE;
	assert(m_device_features.logicOp);
	features.logicOp = VK_TRUE;
	assert(m_device_features.fillModeNonSolid);
	features.fillModeNonSolid = VK_TRUE;
	assert(m_device_features.depthBounds);
	features.depthBounds = VK_TRUE;
	assert(m_device_features.wideLines);
	features.wideLines = VK_TRUE;
	assert(m_device_features.largePoints);
	features.largePoints = VK_TRUE;
	assert(m_device_features.samplerAnisotropy);
	features.samplerAnisotropy = VK_TRUE;
	assert(m_device_features.textureCompressionBC);
	features.textureCompressionBC = VK_TRUE;
	assert(m_device_features.vertexPipelineStoresAndAtomics);
	features.vertexPipelineStoresAndAtomics = VK_TRUE;
	assert(m_device_features.shaderImageGatherExtended);
	features.shaderImageGatherExtended = VK_TRUE;
	assert(m_device_features.shaderStorageImageExtendedFormats);
	features.shaderStorageImageExtendedFormats = VK_TRUE;
	assert(m_device_features.shaderUniformBufferArrayDynamicIndexing);
	features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
	assert(m_device_features.shaderSampledImageArrayDynamicIndexing);
	features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
	assert(m_device_features.shaderStorageBufferArrayDynamicIndexing);
	features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
	assert(m_device_features.shaderStorageImageArrayDynamicIndexing);
	features.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
	assert(m_device_features.shaderClipDistance);
	features.shaderClipDistance = VK_TRUE;
	assert(m_device_features.shaderCullDistance);
	features.shaderCullDistance = VK_TRUE;
}

void VulkanContext::get_queues()
{
	// Get graphics queues
	m_graphics_queue_family.queues.resize(m_graphics_queue_family.queue_count);
	for (uint32_t i = 0; i < m_graphics_queue_family.queue_count; i++)
	{
		vkGetDeviceQueue(m_device, m_graphics_queue_family.family_index, i, &m_graphics_queue_family.queues[i]);
	}

	// Get compute queues
	m_compute_queue_family.queues.resize(m_compute_queue_family.queue_count);
	for (uint32_t i = 0; i < m_compute_queue_family.queue_count; i++)
	{
		vkGetDeviceQueue(m_device, m_compute_queue_family.family_index, i, &m_compute_queue_family.queues[i]);
	}

	// Get transfer queues
	m_transfer_queue_family.queues.resize(m_transfer_queue_family.queue_count);
	for (uint32_t i = 0; i < m_transfer_queue_family.queue_count; i++)
	{
		vkGetDeviceQueue(m_device, m_transfer_queue_family.family_index, i, &m_transfer_queue_family.queues[i]);
	}
}

void VulkanContext::create_command_pools()
{
	// Create graphics command pool
	VkCommandPoolCreateInfo command_pool_info;
	command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_info.pNext = nullptr;
	command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	command_pool_info.queueFamilyIndex = m_graphics_queue_family.family_index;

	VkResult result = vkCreateCommandPool(m_device, &command_pool_info, m_allocation_callbacks, &m_graphics_command_pool);
	assert(result == VK_SUCCESS);

	// Create compute command pool
	command_pool_info.queueFamilyIndex = m_compute_queue_family.family_index;
	result = vkCreateCommandPool(m_device, &command_pool_info, m_allocation_callbacks, &m_compute_command_pool);
	assert(result == VK_SUCCESS);

	// Create transfer command pool
	command_pool_info.queueFamilyIndex = m_transfer_queue_family.family_index;
	result = vkCreateCommandPool(m_device, &command_pool_info, m_allocation_callbacks, &m_transfer_command_pool);
	assert(result == VK_SUCCESS);
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

VkInstance VulkanContext::get_instance()
{
	return m_instance;
}

VkDevice VulkanContext::get_device()
{
	return m_device;
}

VkPhysicalDevice VulkanContext::get_physical_device()
{
	return m_physical_device;
}

const VkPhysicalDeviceProperties& VulkanContext::get_device_properties()
{
	return m_device_properties;
}

VkAllocationCallbacks* VulkanContext::get_allocation_callbacks()
{
	return m_allocation_callbacks;
}

size_t VulkanContext::graphics_queues_count()
{
	return m_graphics_queue_family.queue_count;
}

size_t VulkanContext::compute_queue_count()
{
	return m_compute_queue_family.queue_count;
}

size_t VulkanContext::transfer_queues_count()
{
	return m_transfer_queue_family.queue_count;
}

VkQueue VulkanContext::get_graphics_queue(size_t index)
{
	return m_graphics_queue_family.queues[index];
}

VkQueue VulkanContext::get_compute_queue(size_t index)
{
	return m_compute_queue_family.queues[index];
}

VkQueue VulkanContext::get_transfer_queue(size_t index)
{
	return m_transfer_queue_family.queues[index];
}
