#include "specialization_info.hpp"

SpecializationInfo::SpecializationInfo(SpecializationInfo&& other)
{
	move_from(std::move(other));
}

SpecializationInfo& SpecializationInfo::operator=(SpecializationInfo&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

SpecializationInfo::~SpecializationInfo()
{
	destroy();
}

void SpecializationInfo::add_entry(size_t byte_size)
{
	VkSpecializationMapEntry entry;
	entry.constantID = static_cast<uint32_t>(m_entries.size());
	entry.offset = static_cast<uint32_t>(m_size);
	entry.size = byte_size;

	m_size += byte_size;

	m_entries.push_back(entry);
}

void SpecializationInfo::set_data(void* data)
{
	m_data = data;
}

VkSpecializationInfo& SpecializationInfo::get_info()
{
	m_info.mapEntryCount = static_cast<uint32_t>(m_entries.size());
	m_info.pMapEntries = m_entries.size() > 0 ? m_entries.data() : nullptr;
	m_info.dataSize = m_size;
	m_info.pData = m_data;

	return m_info;
}

void SpecializationInfo::move_from(SpecializationInfo&& other)
{
	destroy();

	m_entries = std::move(other.m_entries);
	m_data = other.m_data;
	m_size = other.m_size;
}

void SpecializationInfo::destroy()
{

}