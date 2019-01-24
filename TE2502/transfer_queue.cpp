#include "transfer_queue.hpp"



TransferQueue::TransferQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) : Queue(context, command_pool, queue)
{
}


TransferQueue::~TransferQueue()
{
}
