#pragma once

#include "queue.hpp"

class VulkanContext;

// Represents a hardware queue used for compute commands
class ComputeQueue : public Queue
{
public:
	ComputeQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~ComputeQueue();
};

