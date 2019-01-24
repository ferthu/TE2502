#pragma once

#include <vulkan/vulkan.h>

class Pipeline
{
public:
	Pipeline(VkPipeline pipeline, VkPipelineLayout pipeline_layout, VkDevice& device);
	~Pipeline();

	VkPipeline pipeline;

private:
	VkDevice& m_device;
	VkPipelineLayout m_pipeline_layout;
};

