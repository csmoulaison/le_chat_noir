struct vk_image_view_config
{
	VkImageView*       view;
	VkImage            image;
	VkFormat           format;
	VkImageAspectFlags aspect_mask;
};

void vk_create_image_view(VkDevice device, struct vk_image_view_config config)
{
	VkImageViewCreateInfo info = {};
	info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image                           = config.image;
	info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
	info.format                          = config.format;
	info.subresourceRange.aspectMask     = config.aspect_mask;
	info.subresourceRange.baseMipLevel   = 0;
	info.subresourceRange.levelCount     = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount     = 1;
	VK_VERIFY(vkCreateImageView(device, &info, 0, config.view));
}
