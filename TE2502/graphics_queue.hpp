#pragma once

#include "queue.hpp"

class VulkanContext;

// Represents a hardware queue used for graphics commands
class GraphicsQueue : public Queue
{
public:
	GraphicsQueue(VulkanContext& context, VkCommandPool command_pool, VkQueue queue);
	virtual ~GraphicsQueue();

private:

};

