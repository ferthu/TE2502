#pragma once

#include "queue.hpp"

class VulkanContext;

class TransferQueue : public Queue
{
public:
	TransferQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~TransferQueue();
};

