#include "compute_queue.hpp"



ComputeQueue::ComputeQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) : Queue(context, command_pool, queue)
{
}


ComputeQueue::~ComputeQueue()
{
}
