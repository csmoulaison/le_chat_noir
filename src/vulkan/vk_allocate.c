void vk_allocate_memory(
	VkDevice             device,
	VkPhysicalDevice     physical_device,
	VkDeviceMemory*      memory,
	VkMemoryRequirements requirements,
	uint32_t             type_mask)
{
	VkPhysicalDeviceMemoryProperties mem_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

	uint32_t memory_type_index;
	bool memory_found = false;
	for(uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
	{
		if((requirements.memoryTypeBits & (1 << i)) 
			&& 
			(mem_properties.memoryTypes[i].propertyFlags & type_mask) == type_mask)
		{
			memory_type_index = i;
			memory_found = true;
		}
	}
	if(!memory_found)
	{
		printf("Failed to find suitable memory type for buffer.\n");
		PANIC();
	}

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize  = requirements.size;
	alloc_info.memoryTypeIndex = memory_type_index;

	VK_VERIFY(vkAllocateMemory(device, &alloc_info, 0, memory));
}

void vk_allocate_buffer(
	VkDevice              device,
	VkPhysicalDevice      physical_device,
	VkBuffer*             buffer,
	VkDeviceMemory*       memory,
	VkDeviceSize          size, 
	VkBufferUsageFlags    usage, 
	VkMemoryPropertyFlags properties)
{
	VkBufferCreateInfo buf_info = {};
	buf_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_info.size        = size;
	buf_info.usage       = usage;
	buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_VERIFY(vkCreateBuffer(device, &buf_info, 0, buffer));

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, *buffer, &requirements);

	vk_allocate_memory(
		device,
		physical_device, 
		memory,
		requirements, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_VERIFY(vkBindBufferMemory(device, *buffer, *memory, 0));
}

void vk_allocate_image(
	VkDevice          device,
	VkPhysicalDevice  physical_device,
	struct vk_image_allocation* alloc)
{
	VkImageCreateInfo image_info = {};
	image_info.sType 		 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType     = VK_IMAGE_TYPE_2D;
	image_info.extent.depth  = 1;
	image_info.format        = alloc->format;
	image_info.mipLevels     = 1;
	image_info.arrayLayers   = 1;
	image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.extent.width  = alloc->width;
	image_info.extent.height = alloc->height;
	image_info.samples       = alloc->sample_count;
	image_info.usage         = alloc->usage_mask;
	VK_VERIFY(vkCreateImage(device, &image_info, 0, alloc->image));

	VkMemoryRequirements requirements = {};
	vkGetImageMemoryRequirements(device, *alloc->image, &requirements);

	vk_allocate_memory(
		device,
		physical_device, 
		alloc->memory,
		requirements, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_VERIFY(vkBindImageMemory(device, *alloc->image, *alloc->memory, 0));
}

// LATER - Currently unused, will be part of texture mapping implementation.
void vk_allocate_texture(
	VkDevice         device,
	VkPhysicalDevice physical_device,
	VkImage*         image,
	VkDeviceMemory*  memory,
	char*            fname)
{
	int32_t tex_w;
	int32_t tex_h;
	int32_t tex_channels;
	stbi_uc* pixels = stbi_load(fname, &tex_w, &tex_h, &tex_channels, STBI_rgb_alpha);
	if(!pixels)
	{
		printf("Failed to load image file: %s.\n", fname);
		PANIC();
	}

	VkDeviceSize img_size = tex_w * tex_h * 4;
	VkBuffer staging_buf;
	VkDeviceMemory staging_mem;

	vk_allocate_buffer(
		device, 
		physical_device,
		&staging_buf, 
		&staging_mem,
		img_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* data;
	vkMapMemory(device, staging_mem, 0, img_size, 0, &data);
	memcpy(data, pixels, (size_t)img_size);
	vkUnmapMemory(device, staging_mem);

	stbi_image_free(pixels);

	struct vk_image_allocation alloc =
	{
		.image        = image,
		.memory       = memory,
		.width        = tex_w,
		.height       = tex_h,
		.format       = VK_FORMAT_R8G8B8A8_SRGB,
		.sample_count = VK_SAMPLE_COUNT_1_BIT,
		.usage_mask   = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
	};

	vk_allocate_image(device, physical_device, &alloc);
}
