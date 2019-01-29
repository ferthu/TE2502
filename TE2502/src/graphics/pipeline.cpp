#include "pipeline.hpp"

Pipeline::Pipeline(VkPipeline pipeline, PipelineLayout& pipeline_layout, VkDevice& device) 
	: m_pipeline(pipeline)
	, m_pipeline_layout(pipeline_layout)
	, m_device(device)
{
}


Pipeline::~Pipeline()
{
	vkDestroyPipeline(m_device, m_pipeline, nullptr);
}
