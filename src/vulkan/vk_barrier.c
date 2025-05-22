void vk_image_memory_barrier(
	VkCommandBuffer         command_buffer, 
	VkImage                 image, 
	VkImageAspectFlags      aspect_mask,
	VkImageLayout           layout_old, 
	VkImageLayout           layout_new, 
	VkAccessFlags           src_access_mask,
	VkAccessFlags           dst_access_mask,
	VkPipelineStageFlagBits stage_src, 
	VkPipelineStageFlagBits stage_dst) 
{
    VkImageSubresourceRange subresource_range = {};
    subresource_range.aspectMask     = aspect_mask;
    subresource_range.baseMipLevel   = 0;
    subresource_range.levelCount     = 1;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount     = 1;

    VkImageMemoryBarrier barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask       = src_access_mask;
    barrier.dstAccessMask       = dst_access_mask;
    barrier.oldLayout           = layout_old;
    barrier.newLayout           = layout_new;
    barrier.srcQueueFamilyIndex = 0;
    barrier.dstQueueFamilyIndex = 0;
    barrier.image               = image;
    barrier.subresourceRange    = subresource_range;

    vkCmdPipelineBarrier(command_buffer, stage_src, stage_dst, 0, 0, 0, 0, 0, 1, &barrier);
}
