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
	VkImageCreateInfo image_info = 
	{
		.sType 		           = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext                 = 0,
		.flags                 = 0,
		.imageType             = VK_IMAGE_TYPE_2D,
		.format                = alloc->format,
		.extent                = (VkExtent3D){alloc->width, alloc->height, 1},
		.mipLevels             = 1,
		.arrayLayers           = 1,
		.samples               = alloc->sample_count,
		.tiling                = VK_IMAGE_TILING_OPTIMAL,
		.usage                 = alloc->usage_mask,
		.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
		.pQueueFamilyIndices   = 0,
		.queueFamilyIndexCount = 0,
		.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
	};
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
