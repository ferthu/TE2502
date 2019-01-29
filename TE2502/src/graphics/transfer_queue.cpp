#include <utility>

#include "transfer_queue.hpp"



TransferQueue::TransferQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) : Queue(context, command_pool, queue)
{
}


TransferQueue::~TransferQueue()
{
	destroy();
}

TransferQueue::TransferQueue(TransferQueue&& other)
{
	move_from(std::move(other));
}

TransferQueue& TransferQueue::operator=(TransferQueue&& other)
{
	if (this != &other)
		move_from(std::move(other));

	return *this;
}

void TransferQueue::move_from(TransferQueue&& other)
{
	destroy();
	Queue::move_from(std::move(other));
}

void TransferQueue::destroy()
{
	Queue::destroy();
}
