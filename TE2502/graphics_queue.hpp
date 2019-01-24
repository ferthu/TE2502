#pragma once

#include "queue.hpp"

class GraphicsQueue : public Queue
{
public:
	GraphicsQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~GraphicsQueue();

private:

};

