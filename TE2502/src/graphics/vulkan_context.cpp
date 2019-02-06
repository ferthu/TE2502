#include <vector>
#include <iostream>
#include <assert.h>
#include <intrin.h>
#include <fstream>

#include "utilities.hpp"
#include "vulkan_context.hpp"
#include "utilities.hpp"
#include "window.hpp"
#include "pipeline.hpp"
#include "render_pass.hpp"

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

	create_descriptor_pool();
}

VulkanContext::~VulkanContext()
{
	vkDestroyDescriptorPool(m_device, m_descriptor_pool, m_allocation_callbacks);

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

	// Select memory types to use for allocation

	// Select device memory type
	VkMemoryPropertyFlagBits flags = (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (!find_memory_type(m_device_memory_type, flags))
	{
#ifdef _DEBUG
		__debugbreak();
#endif

		print("Could not find device local memory!\n");
		exit(1);
	}

	// Select host memory type. First look for a cached type
	flags = (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
	if (!find_memory_type(m_host_memory_type, flags))
	{
		// If a cached memory type could not be found, just try to find a coherent one
		flags = (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (!find_memory_type(m_host_memory_type, flags))
		{
#ifdef _DEBUG
			__debugbreak();
#endif

			print("Could not find coherent host visible memory!\n");
			exit(1);
		}
	}
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

	float prios[32];
	for (size_t i = 0; i < 32; i++)
	{
		prios[i] = 1.0f;
	}

	// Add graphics queue
	{
		VkDeviceQueueCreateInfo graphics_queue;
		graphics_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		graphics_queue.pNext = nullptr;
		graphics_queue.flags = 0;
		graphics_queue.queueFamilyIndex = m_graphics_queue_family.family_index;
		graphics_queue.queueCount = m_graphics_queue_family.queue_count;
		graphics_queue.pQueuePriorities = prios;

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
		compute_queue.pQueuePriorities = prios;

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
		transfer_queue.pQueuePriorities = prios;

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

bool VulkanContext::find_memory_type(uint32_t& output, VkMemoryPropertyFlagBits flags)
{
	for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; i++)
	{
		if ((m_memory_properties.memoryTypes[i].propertyFlags & flags) == flags)
		{
			output = i;
			return true;
		}
		
	}

	return false;
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
	assert(m_device_features.multiDrawIndirect);
	features.multiDrawIndirect = VK_TRUE;
}

static std::vector<char> read_file(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to open file: " + filename);
		exit(1);
#endif
	}
	size_t file_size = (size_t)file.tellg();
	std::vector<char> buffer(file_size);
	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();

	return buffer;
}

std::unique_ptr<Pipeline> VulkanContext::create_compute_pipeline(const std::string& shader_name, PipelineLayout& layout, SpecializationInfo* compute_shader_specialization)
{
	auto shader_code = read_file("shaders/compiled/" + shader_name + ".comp.glsl.spv");

	VkShaderModule shader_module = create_shader_module(shader_code);

	VkPipelineShaderStageCreateInfo shader_stage_info = {};
	shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage_info.pNext = nullptr;
	shader_stage_info.flags = 0;
	shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shader_stage_info.module = shader_module;
	shader_stage_info.pName = "main";

	if (compute_shader_specialization)
		shader_stage_info.pSpecializationInfo = &compute_shader_specialization->get_info();

	VkComputePipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipeline_info.pNext = nullptr;
	pipeline_info.flags = 0;
	pipeline_info.stage = shader_stage_info;
	pipeline_info.layout = layout.get_pipeline_layout();
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;

	VkPipeline pipeline;

	if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, m_allocation_callbacks, &pipeline) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to create compute pipeline!");
		exit(1);
#endif
	}

	vkDestroyShaderModule(m_device, shader_module, nullptr);

	return std::make_unique<Pipeline>(pipeline, layout, m_device);
}

std::unique_ptr<Pipeline> VulkanContext::create_graphics_pipeline(
	const std::string& shader_name,
	const glm::vec2 window_size,
	PipelineLayout& layout,
	VertexAttributes& vertex_attributes,
	RenderPass& render_pass,
	bool enable_depth,
	SpecializationInfo* vertex_shader_specialization,
	SpecializationInfo* fragment_shader_specialization,
	VkPrimitiveTopology topology)
{
	auto vert_shader_code = read_file("shaders/compiled/" + shader_name + ".vert.glsl.spv");
	auto frag_shader_code = read_file("shaders/compiled/" + shader_name + ".frag.glsl.spv");

	VkShaderModule vert_shader_module = create_shader_module(vert_shader_code);
	VkShaderModule frag_shader_module = create_shader_module(frag_shader_code);

	VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
	vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_stage_info.module = vert_shader_module;
	vert_shader_stage_info.pName = "main";

	if (vertex_shader_specialization)
		vert_shader_stage_info.pSpecializationInfo = &vertex_shader_specialization->get_info();

	VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
	frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_stage_info.module = frag_shader_module;
	frag_shader_stage_info.pName = "main";

	if (fragment_shader_specialization)
		frag_shader_stage_info.pSpecializationInfo = &fragment_shader_specialization->get_info();

	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };	

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = vertex_attributes.get_num_bindings();
	vertex_input_info.pVertexBindingDescriptions = vertex_attributes.get_bindings();
	vertex_input_info.vertexAttributeDescriptionCount = vertex_attributes.get_num_attributes();
	vertex_input_info.pVertexAttributeDescriptions = vertex_attributes.get_attributes();

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = topology;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = window_size.x;
	viewport.height = window_size.y;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = VkExtent2D({ (uint32_t)window_size.x, (uint32_t)window_size.y });

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo color_blending = {};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_blend_attachment;
	color_blending.blendConstants[0] = 0.0f; // Optional
	color_blending.blendConstants[1] = 0.0f; // Optional
	color_blending.blendConstants[2] = 0.0f; // Optional
	color_blending.blendConstants[3] = 0.0f; // Optional

	VkPipelineDepthStencilStateCreateInfo depth_stencil;
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.pNext = nullptr;
	depth_stencil.flags = 0;
	depth_stencil.depthTestEnable = enable_depth ? VK_TRUE : VK_FALSE;
	depth_stencil.depthWriteEnable = enable_depth ? VK_TRUE : VK_FALSE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;

	VkStencilOpState stencil_state; 
	stencil_state.failOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
	stencil_state.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
	stencil_state.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
	stencil_state.compareOp = VK_COMPARE_OP_ALWAYS;
	stencil_state.compareMask = 1;
	stencil_state.writeMask = 1;
	stencil_state.reference = 1;

	depth_stencil.front = stencil_state;
	depth_stencil.back = stencil_state;
	depth_stencil.minDepthBounds = 0.0f;
	depth_stencil.maxDepthBounds = 1.0f;

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;
	pipeline_info.pVertexInputState = &vertex_input_info;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pDepthStencilState = &depth_stencil;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.pDynamicState = nullptr; // Optional
	pipeline_info.layout = layout.get_pipeline_layout();
	pipeline_info.renderPass = render_pass.get_render_pass();
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipeline_info.basePipelineIndex = -1; // Optional

	VkPipeline pipeline;

	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, m_allocation_callbacks, &pipeline) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to create graphics pipeline!");
		exit(1);
#endif
	}

	vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
	vkDestroyShaderModule(m_device, frag_shader_module, nullptr);

	return std::make_unique<Pipeline>(pipeline, layout, m_device);
}

VkDescriptorSet VulkanContext::allocate_descriptor_set(DescriptorSetLayout& layout)
{
	VkDescriptorSet descriptor_set;
	VkDescriptorSetLayout descriptor_set_layout = layout.get_descriptor_set_layout();

	VkDescriptorSetAllocateInfo allocate_info;
	allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocate_info.pNext = nullptr;
	allocate_info.descriptorPool = m_descriptor_pool;
	allocate_info.descriptorSetCount = 1;
	allocate_info.pSetLayouts = &descriptor_set_layout;

	if (vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#else
		println("Failed to allocate descriptor set!");
		exit(1);
#endif
	}

	return descriptor_set;
}

void VulkanContext::free_descriptor_set(VkDescriptorSet descriptor_set)
{
	if (vkFreeDescriptorSets(m_device, m_descriptor_pool, 1, &descriptor_set) != VK_SUCCESS)
	{
#ifdef _DEBUG
		__debugbreak();
#endif

		print("Failed when freeing descriptor set!\n");
		exit(1);
	}
}

//std::vector<char> VulkanContext::compile_from_file(const std::string& file_name, shaderc_shader_kind kind) 
//{
//	shaderc::Compiler compiler;
//	shaderc::CompileOptions options;
//
//	//if (optimize) 
//	options.SetOptimizationLevel(shaderc_optimization_level_performance);
//
//	std::ifstream file(file_name, std::ios::ate);
//
//	if (!file.is_open()) 
//	{
//#ifdef _DEBUG
//		__debugbreak();
//#else
//		println("Failed to open file: " + file_name);
//		exit(1);
//#endif
//	}
//
//	size_t file_size = (size_t)file.tellg();
//	std::vector<char> buffer(file_size);
//	file.seekg(0);
//	file.read(buffer.data(), file_size);
//	file.close();
//
//	shaderc::SpvCompilationResult module =
//		compiler.CompileGlslToSpv(buffer.data(), kind, file_name.c_str(), options);
//
//	if (module.GetCompilationStatus() != shaderc_compilation_status_success)
//	{
//#ifdef _DEBUG
//		println("Failed to compile shader \"" + file_name + "\" with error: " + module.GetErrorMessage());
//		__debugbreak();
//#else
//		println("Failed to compile shader \"" + file_name + "\" with error: " + module.GetErrorMessage());
//		exit(1);
//#endif // _DEBUG
//	}
//
//	return buffer;
//}

VkShaderModule VulkanContext::create_shader_module(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shader_module;
	if (vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
	{
#ifdef _DEBUG
		println("Failed to create shader module!");
		__debugbreak();
#else
		println("Failed to create shader module!");
		exit(1);
#endif // _DEBUG
	}

	return shader_module;
}

void VulkanContext::create_descriptor_pool()
{
	const uint32_t pool_size_count = 9;
	VkDescriptorPoolSize descriptor_counts[pool_size_count] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 100},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100}
	};


	VkDescriptorPoolCreateInfo pool_info;
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.pNext = nullptr;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 100;
	pool_info.poolSizeCount = pool_size_count;
	pool_info.pPoolSizes = descriptor_counts;

	if (vkCreateDescriptorPool(m_device, &pool_info, m_allocation_callbacks, &m_descriptor_pool) != VK_SUCCESS)
	{
		println("Failed to create descriptor pool!");
#ifdef _DEBUG
		__debugbreak();
#else
		exit(1);
#endif // _DEBUG
	}
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
	command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

uint32_t VulkanContext::get_graphics_queue_index()
{
	return m_graphics_queue_family.family_index;
}

uint32_t VulkanContext::get_compute_queue_index()
{
	return m_compute_queue_family.family_index;
}

VkDescriptorPool VulkanContext::get_descriptor_pool()
{
	return m_descriptor_pool;
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

GraphicsQueue VulkanContext::create_graphics_queue()
{
	assert(m_graphics_queue_family.next_free < m_graphics_queue_family.queue_count);

	m_graphics_queue_family.next_free++;

	return GraphicsQueue(*this, m_graphics_command_pool, m_graphics_queue_family.queues[m_graphics_queue_family.next_free - 1]);
}

ComputeQueue VulkanContext::create_compute_queue()
{
	assert(m_compute_queue_family.next_free < m_compute_queue_family.queue_count);

	m_compute_queue_family.next_free++;

	return ComputeQueue(*this, m_compute_command_pool, m_compute_queue_family.queues[m_compute_queue_family.next_free - 1]);
}

TransferQueue VulkanContext::create_transfer_queue()
{
	assert(m_transfer_queue_family.next_free < m_transfer_queue_family.queue_count);

	m_transfer_queue_family.next_free++;

	return TransferQueue(*this, m_transfer_command_pool, m_transfer_queue_family.queues[m_transfer_queue_family.next_free - 1]);
}

GPUMemory VulkanContext::allocate_device_memory(VkDeviceSize byte_size)
{
	return GPUMemory(*this, m_device_memory_type, byte_size);
}

GPUMemory VulkanContext::allocate_host_memory(VkDeviceSize byte_size)
{
	return GPUMemory(*this, m_host_memory_type, byte_size);
}
