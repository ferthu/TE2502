#include "graphics_queue.hpp"

GraphicsQueue::GraphicsQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue) : Queue(context, command_pool, queue)
{
}


GraphicsQueue::~GraphicsQueue()
{
}
