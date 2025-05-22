VkCommandBuffer vk_start_transient_commands(
	VkDevice device, 
	VkCommandPool command_pool)
{
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool        = command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer buffer;
	vkAllocateCommandBuffers(device, &alloc_info, &buffer);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(buffer, &begin_info);

	return buffer;
}

void vk_end_transient_commands(
	VkCommandBuffer buffer,
	VkQueue         submit_queue,
	VkDevice        device,
	VkCommandPool   pool)
{
	vkEndCommandBuffer(buffer);

	VkSubmitInfo submit_info = {};
	submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &buffer;

	// CONSIDER  - We are using the graphics queue for this presently, but might
	// we want to use a transfer queue for this?
	// I'm not sure if there's any possible performance boost, and I'd rather not
	// do it just for the sake of it.
	// 
	// High level on how this might be done at the following link:
	// https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
	vkQueueSubmit(submit_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(submit_queue);

	vkFreeCommandBuffers(device, pool, 1, &buffer);
}
