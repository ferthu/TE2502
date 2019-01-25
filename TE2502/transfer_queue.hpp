#pragma once

#include "queue.hpp"

class VulkanContext;

// Represents a hardware queue used for transfer commands
class TransferQueue : public Queue
{
public:
	TransferQueue() {};
	TransferQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~TransferQueue();

	TransferQueue(TransferQueue&& other);
	TransferQueue& operator=(TransferQueue&& other);

private:
	// Move other into this
	void move_from(TransferQueue&& other);
};

