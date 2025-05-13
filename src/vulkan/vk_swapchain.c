void vk_init_swapchain(struct vk_context* vk, bool recreate)
{
	if(recreate) 
	{
		vkDeviceWaitIdle(vk->device);
		for(uint32_t i = 0; i < vk->swap_images_len; i++)
		{
			vkDestroyImageView(vk->device, vk->swap_views[i], 0);
		}
		vkDestroySwapchainKHR(vk->device, vk->swapchain, 0);

		vkDestroyImage(vk->device, vk->render_image, 0);
		vkDestroyImageView(vk->device, vk->render_view, 0);
		vkFreeMemory(vk->device, vk->render_image_memory, 0);

		vkDestroySemaphore(vk->device, vk->semaphore_image_available, 0);
		vkDestroySemaphore(vk->device, vk->semaphore_render_finished, 0);
	}

	// Query surface capabilities.
	uint32_t image_count = 0;
	VkSurfaceTransformFlagBitsKHR pre_transform;
	{
		VkSurfaceCapabilitiesKHR abilities;

		VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_device, vk->surface, &abilities));

		// XXX - See what we can do about this, or whether it's necessary to do
		// anything different.
		vk->swap_extent.width = abilities.maxImageExtent.width;
		vk->swap_extent.height = abilities.maxImageExtent.height;

		if(abilities.minImageExtent.width  > vk->swap_extent.width
		|| abilities.maxImageExtent.width  < vk->swap_extent.width
		|| abilities.minImageExtent.height > vk->swap_extent.height
		|| abilities.maxImageExtent.height < vk->swap_extent.height) 
		{
			printf("Surface KHR extents are not compatible with configured surface sizes.\n");
			PANIC();
		}

		image_count = abilities.minImageCount + 1;
		if(abilities.maxImageCount > 0 && image_count > abilities.maxImageCount) 
		{
			image_count = abilities.maxImageCount;
		}

		pre_transform = abilities.currentTransform;
	}

	// Choose surface format.
	{
		uint32_t formats_len;
		vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface, &formats_len, 0);
		if(formats_len == 0) 
		{
			printf("Physical device doesn't support any formats?\n");
			PANIC();
		}

		VkSurfaceFormatKHR formats[formats_len];
		vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface, &formats_len, formats);

		vk->surface_format = formats[0];
		for(int i = 0; i < formats_len; i++) 
		{
			if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
			{
				vk->surface_format = formats[i];
				break;
			}
		}
	}

	// Choose presentation mode.
	// 
	// Default to VK_PRESENT_MODE_FIFO_KHR, as this is the only mode required to
	// be supported by the spec.
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	{
		uint32_t modes_len;
		VK_VERIFY(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physical_device, vk->surface, &modes_len, 0));

		VkPresentModeKHR modes[modes_len];
		VK_VERIFY(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physical_device, vk->surface, &modes_len, modes));

		for(int i = 0; i < modes_len; i++) 
		{
			if(modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) 
			{
				present_mode = modes[i];
				break;
			}
		}
	}

	VkSwapchainCreateInfoKHR info = {};
	info.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.pNext                 = 0;
	info.flags                 = 0; // TODO mutable format or any other flags?
	info.surface               = vk->surface;
	info.minImageCount         = image_count; // TODO get this value.
	info.imageFormat           = vk->surface_format.format;
	info.imageColorSpace       = vk->surface_format.colorSpace;
	info.imageExtent           = vk->swap_extent;
	info.imageArrayLayers      = 1;
	info.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // TODO probably right, but we'll see.
	info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE; // TODO needs to be CONCURRENT if compute is in different family from present
	info.queueFamilyIndexCount = 0; // Not used in exclusive mode. Need to check for concurrent.
	info.pQueueFamilyIndices   = 0; // Also not used in exclusive mode, see above.
	info.preTransform          = pre_transform;
	info.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode           = present_mode;
	info.clipped               = VK_TRUE;
	info.oldSwapchain          = VK_NULL_HANDLE;

#if VK_IMMEDIATE
		info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
#endif

	VK_VERIFY(vkCreateSwapchainKHR(vk->device, &info, 0, &vk->swapchain));
	VK_VERIFY(vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->swap_images_len, 0));
	VK_VERIFY(vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->swap_images_len, vk->swap_images));

	// Allocate resources for render and depth images
#define IMAGE_ALLOCATIONS_LEN 2
	struct vk_image_allocation allocs[IMAGE_ALLOCATIONS_LEN] = 
	{
		{
			.image        = &vk->render_image,
			.memory       = &vk->render_image_memory,
			.width        = vk->swap_extent.width,
			.height       = vk->swap_extent.height,
			.format       = vk->surface_format.format,
			.sample_count = vk->render_sample_count,
			.usage_mask   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		},
		{
			.image        = &vk->depth_image,
			.memory       = &vk->depth_image_memory,
			.width        = vk->swap_extent.width,
			.height       = vk->swap_extent.height,
			.format       = DEPTH_ATTACHMENT_FORMAT,
			.sample_count = vk->render_sample_count,
			.usage_mask   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		}
	};

	for(uint32_t i = 0; i < IMAGE_ALLOCATIONS_LEN; i++)
	{
		vk_allocate_image(vk->device, vk->physical_device, &allocs[i]);
	}


	// Create image views.
	uint8_t view_configs_len = IMAGE_ALLOCATIONS_LEN + vk->swap_images_len;
	struct view_config 
	{
		VkImageView*       view;
		VkImage            image;
		VkFormat           format;
		VkImageAspectFlags aspect_mask;
	} view_configs[view_configs_len] = {};

	view_configs[0].view        = &vk->render_view;
	view_configs[0].image       = vk->render_image;
	view_configs[0].format      = vk->surface_format.format;
	view_configs[0].aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_configs[1].view        = &vk->depth_view;
	view_configs[1].image       = vk->depth_image;
	view_configs[1].format      = DEPTH_ATTACHMENT_FORMAT;
	view_configs[1].aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
	for(int i = 0; i < vk->swap_images_len; i++) 
	{
		view_configs[IMAGE_ALLOCATIONS_LEN + i] = (struct view_config)
		{
			.view        = &vk->swap_views[i],
			.image       = vk->swap_images[i],
			.format      = vk->surface_format.format,
			.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
		};
	}

	for(uint32_t i = 0; i < view_configs_len; i++)
	{
		VkImageViewCreateInfo view_info = {};
		view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image                           = view_configs[i].image;
		view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format                          = view_configs[i].format;
		view_info.subresourceRange.aspectMask     = view_configs[i].aspect_mask;
		view_info.subresourceRange.baseMipLevel   = 0;
		view_info.subresourceRange.levelCount     = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount     = 1;
		VK_VERIFY(vkCreateImageView(vk->device, &view_info, 0, view_configs[i].view));
	}

	// Create synchronization primitives
	{
		VkSemaphoreCreateInfo semaphore_info = {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VK_VERIFY(vkCreateSemaphore(vk->device, &semaphore_info, 0, &vk->semaphore_image_available));
		VK_VERIFY(vkCreateSemaphore(vk->device, &semaphore_info, 0, &vk->semaphore_render_finished));
	}
}
