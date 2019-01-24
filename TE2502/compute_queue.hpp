#pragma once

#include "queue.hpp"

class ComputeQueue : public Queue
{
public:
	ComputeQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~ComputeQueue();
};

