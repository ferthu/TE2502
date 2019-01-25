#pragma once

#include "queue.hpp"

class VulkanContext;

// Represents a hardware queue used for transfer commands
class TransferQueue : public Queue
{
public:
	TransferQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~TransferQueue();
};

