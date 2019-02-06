#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class SpecializationInfo
{
public:
	SpecializationInfo() {}
	SpecializationInfo(SpecializationInfo&& other);
	SpecializationInfo& operator=(SpecializationInfo&& other);

	~SpecializationInfo();

	// Add a specialization constant, specifying the next 'byte_size' bytes in data
	void add_entry(size_t byte_size);

	// Set pointer to the initialization values
	void set_data(void* data);

	// Return Vulkan struct
	VkSpecializationInfo& get_info();

private:
	// Move other into this
	void move_from(SpecializationInfo&& other);

	// Destroys object
	void destroy();

	std::vector<VkSpecializationMapEntry> m_entries;
	VkSpecializationInfo m_info;
	void* m_data = nullptr;
	size_t m_size = 0;
};

