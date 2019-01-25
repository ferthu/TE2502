#pragma once

#include <vulkan/vulkan.h>

#include "pipeline_layout.hpp"

class PipelineLayout;

class Pipeline
{
public:
	Pipeline(VkPipeline pipeline, PipelineLayout& pipeline_layout, VkDevice& device);
	~Pipeline();

	VkPipeline m_pipeline;
	PipelineLayout& m_pipeline_layout;

private:
	VkDevice& m_device;
};

