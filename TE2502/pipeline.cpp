#include "pipeline.h"

Pipeline::Pipeline(VkPipeline pipeline, VkPipelineLayout pipeline_layout, VkDevice& device) 
	: pipeline(pipeline)
	, m_pipeline_layout(pipeline_layout)
	, m_device(device)
{
}


Pipeline::~Pipeline()
{
	vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
	vkDestroyPipeline(m_device, pipeline, nullptr);
}
