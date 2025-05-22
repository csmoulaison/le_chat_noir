struct vk_descriptor_config
{
	VkDescriptorType   type;
	VkShaderStageFlags stage_flags;
	VkDeviceSize       offset_in_buffer;
	VkDeviceSize       range_in_buffer;
};

struct vk_context vk_init(struct vk_platform* platform)
{
	struct vk_context vk;

	// Create instance.
	VkApplicationInfo app = 
	{
		.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext              = 0,
		.pApplicationName   = PROGRAM_NAME,
		.applicationVersion = 1,
		.pEngineName        = 0,
		.engineVersion      = 0,
		.apiVersion         = VK_API_VERSION_1_3
	};

	uint32_t exts_len = platform->window_extensions_len;

#if VK_DEBUG
		exts_len++;
		char* debug_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

	const char* exts[exts_len];
	for(int i = 0; i < platform->window_extensions_len; i++) {
		exts[i] = platform->window_extensions[i];
	}

#if VK_DEBUG
		exts[exts_len - 1] = debug_ext;
#endif

	uint32_t layers_len = 0;

	#ifdef VK_DEBUG
		layers_len++;
	#endif

	const char* layers[layers_len];

#if VK_DEBUG
		layers[layers_len - 1] = "VK_LAYER_KHRONOS_validation";
#endif

	VkInstanceCreateInfo inst_info = 
	{
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext                   = 0,
		.flags                   = 0,
		.pApplicationInfo        = &app,
		.enabledLayerCount       = layers_len,
		.ppEnabledLayerNames     = layers,
		.enabledExtensionCount   = exts_len,
		.ppEnabledExtensionNames = exts
	};
	VK_VERIFY(vkCreateInstance(&inst_info, 0, &vk.instance));

	// Create surface using the platform specific callback function.
	VK_VERIFY(platform->create_surface_callback(&vk, platform->context));

	// Create physical device.
	uint32_t devices_len;
	VK_VERIFY(vkEnumeratePhysicalDevices(vk.instance, &devices_len, 0));
	VkPhysicalDevice devices[devices_len];
	VK_VERIFY(vkEnumeratePhysicalDevices(vk.instance, &devices_len, devices));

	vk.physical_device = 0;
	uint32_t graphics_family_idx = 0;
	for(int i = 0; i < devices_len; i++) 
	{
		// Check queue families
		uint32_t fams_len;
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &fams_len, 0);
		VkQueueFamilyProperties fams[fams_len];
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &fams_len, fams);

		//bool compute = false;
		bool graphics = false;
		for(int j = 0; j < fams_len; j++) 
		{
			/*
			if(fams[j].queueFlags & VK_QUEUE_COMPUTE_BIT) 
			{
				compute = true;
				compute_family_idx = j;
			}
			*/
			if(fams[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
			{
				graphics = true;
				graphics_family_idx = j;
			}
		}
		if(!graphics) 
		{
			continue;
		}

		// Check device extensions.
		uint32_t exts_len;
		vkEnumerateDeviceExtensionProperties(devices[i], 0, &exts_len, 0);
		VkExtensionProperties exts[exts_len];
		vkEnumerateDeviceExtensionProperties(devices[i], 0, &exts_len, exts);

		bool swapchain = false;
		bool dynamic = false;
		for(int j = 0; j < exts_len; j++) 
		{
			if(strcmp(exts[j].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) 
			{
				swapchain = true;
				continue;
			}
			if(strcmp(exts[j].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) 
			{
				dynamic = true;
				continue;
			}
		}
		if(!swapchain || !dynamic) 
		{
			continue;
		}

		VkPhysicalDeviceFeatures features = {};
		vkGetPhysicalDeviceFeatures(devices[i], &features);
		if(features.samplerAnisotropy != VK_TRUE)
		{
			continue;
		}

		vk.physical_device = devices[i];

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(vk.physical_device, &properties);

		vk.max_sampler_anisotropy = properties.limits.maxSamplerAnisotropy;

		VkSampleCountFlags sample_counts = properties.limits.framebufferColorSampleCounts; //& properties.limits.framebufferDepthSampleCounts;
		if(sample_counts & VK_SAMPLE_COUNT_64_BIT) 
		{ 
			vk.render_sample_count = VK_SAMPLE_COUNT_64_BIT; 
		} 
		else if(sample_counts & VK_SAMPLE_COUNT_32_BIT)
		{
			vk.render_sample_count = VK_SAMPLE_COUNT_32_BIT; 
		}
		else if(sample_counts & VK_SAMPLE_COUNT_16_BIT)
		{
			vk.render_sample_count = VK_SAMPLE_COUNT_16_BIT; 
		}
		else if(sample_counts & VK_SAMPLE_COUNT_8_BIT)
		{
			vk.render_sample_count = VK_SAMPLE_COUNT_8_BIT; 
		}
		else if(sample_counts & VK_SAMPLE_COUNT_4_BIT)
		{
			vk.render_sample_count = VK_SAMPLE_COUNT_4_BIT; 
		}
		else if(sample_counts & VK_SAMPLE_COUNT_2_BIT)
		{
			vk.render_sample_count = VK_SAMPLE_COUNT_2_BIT; 
		}
		else
		{
			vk.render_sample_count = VK_SAMPLE_COUNT_1_BIT;
		}
	}

	// Exit if we haven't found an eligible device.
	if(!vk.physical_device) 
	{
		printf("No suitable physical device.\n");
		PANIC();
	}

	// Create logical device.
	float priority         = 1.0f;
	VkDeviceQueueCreateInfo queue = 
	{
		.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext            = 0,
		.flags            = 0,
		.queueFamilyIndex = graphics_family_idx,
		.queueCount       = 1,
 		.pQueuePriorities = &priority
	};

 	VkPhysicalDeviceDynamicRenderingFeatures dynamic_features = 
 	{
		.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
		.pNext            = 0,
		.dynamicRendering = VK_TRUE
 	};

	// Zero init for this one because there's a lot of features.
	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &dynamic_features;
	vkGetPhysicalDeviceFeatures2(vk.physical_device, &features2);
	
 	const char* device_exts[2] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
	VkDeviceCreateInfo device_info = 
	{
		.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
	 	.pNext                   = &features2,
	 	.flags                   = 0,
	 	.queueCreateInfoCount    = 1,
	 	.pQueueCreateInfos       = &queue,
	 	.enabledLayerCount       = 0, // Deprecated
	 	.ppEnabledLayerNames     = 0, // ^
	 	.enabledExtensionCount   = 2,
	 	.ppEnabledExtensionNames = device_exts,
	 	.pEnabledFeatures        = 0
	};
 	VK_VERIFY(vkCreateDevice(vk.physical_device, &device_info, 0, &vk.device));

	vkGetDeviceQueue(vk.device, graphics_family_idx, 0, &vk.queue_graphics);

	// Create swapchain, images, and image views. This has been abstracted to allow
	// swapchain recreation after initialization in the case of window resize, for
	// example.
	// CONSIDER - If we were being really pedantic, we would also include pipeline
	// recreation as the surface format could theoretically also change at runtime.
	vk_init_swapchain(&vk, false);

	// Allocate host visible memory buffer.
	VkDeviceSize host_buf_size = sizeof(struct vk_host_memory);

	vk_allocate_buffer(
		vk.device,
		vk.physical_device,
		&vk.host_visible_buffer,
		&vk.host_visible_memory,
		host_buf_size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(vk.device, vk.host_visible_memory, 0, host_buf_size, 0, (void*)&vk.host_visible_mapped);

	// Create graphics pipelines
	// The following create infos aren't specific to an individual pipeline, so
	// have been hoisted out of the below pipeline creation loop.

	// VOLATILE - Length must match dynamic_info.dynamicStateCount.
	VkDynamicState dyn_states[2] = 
	{
		VK_DYNAMIC_STATE_VIEWPORT, 
		VK_DYNAMIC_STATE_SCISSOR 
	};

	VkPipelineRenderingCreateInfoKHR pipeline_render_info = 
	{
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		.pNext                   = VK_NULL_HANDLE,
		.viewMask                = 0,
		.colorAttachmentCount    = 1,
		.pColorAttachmentFormats = &vk.surface_format.format,
		.depthAttachmentFormat   = DEPTH_ATTACHMENT_FORMAT,
		.stencilAttachmentFormat = 0
	};

	VkPipelineDynamicStateCreateInfo dynamic_info = 
	{
		.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext             = 0,
		.flags             = 0,
		.dynamicStateCount = 2,
		.pDynamicStates    = dyn_states
	};

	VkPipelineViewportStateCreateInfo viewport_info = 
	{
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext         = 0,
		.flags         = 0,
		.viewportCount = 1,
		.scissorCount  = 1,
		// pViewports and pScissors null because they're set later during rendering.
		.pViewports    = 0,
		.pScissors     = 0
	};

	VkPipelineRasterizationStateCreateInfo raster_info = 
	{
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext                   = 0,
		.flags                   = 0,
		.depthClampEnable        = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode             = VK_POLYGON_MODE_FILL,
		.cullMode                = VK_CULL_MODE_NONE,
		.frontFace               = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable         = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp          = 0.0f,
		.depthBiasSlopeFactor    = 0.0f,
		.lineWidth               = 1.0f
	};

	VkPipelineMultisampleStateCreateInfo multisample_info = 
	{
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext                 = 0,
		.flags                 = 0,
		.rasterizationSamples  = vk.render_sample_count,
		.sampleShadingEnable   = VK_FALSE,
		.minSampleShading      = VK_FALSE,
		.pSampleMask           = 0,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable      = VK_FALSE
	};

	// LATER - Keep up with the switch to curly initialization from here down.
	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT | 
		VK_COLOR_COMPONENT_G_BIT | 
		VK_COLOR_COMPONENT_B_BIT | 
		VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable    = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo color_blend_info = {};
	color_blend_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_info.logicOpEnable     = VK_FALSE;
	color_blend_info.attachmentCount   = 1;
	color_blend_info.pAttachments      = &color_blend_attachment;
	color_blend_info.blendConstants[0] = 0.0f;
	color_blend_info.blendConstants[1] = 0.0f;
	color_blend_info.blendConstants[2] = 0.0f;
	color_blend_info.blendConstants[3] = 0.0f;

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
	depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_info.depthTestEnable = VK_TRUE;
	depth_stencil_info.depthWriteEnable = VK_TRUE;
	depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_info.stencilTestEnable = VK_FALSE;

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
	input_assembly_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	// Configure pipeline-specific information here.
#define WORLD_DESCRIPTORS_COUNT 3
	struct vk_descriptor_config world_descriptors[WORLD_DESCRIPTORS_COUNT] = 
	{
		{
			.type             = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.stage_flags      = VK_SHADER_STAGE_VERTEX_BIT,
			.offset_in_buffer = offsetof(struct vk_host_memory, global),
			.range_in_buffer  = sizeof(struct vk_ubo_global_world)
		},
		{
			.type             = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.stage_flags      = VK_SHADER_STAGE_VERTEX_BIT,
			.offset_in_buffer = offsetof(struct vk_host_memory, instance),
			.range_in_buffer  = sizeof(mat4)
		},
		{
			.type             = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.stage_flags      = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset_in_buffer = 0,
			.range_in_buffer  = 0
		}
	};

#define WORLD_ATTRIBUTES_COUNT 2
	struct vk_attribute_description world_attributes[WORLD_ATTRIBUTES_COUNT] = 
	{
		{
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(struct vk_cube_vertex, pos)
		},
		{
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(struct vk_cube_vertex, color)
		}
	};

	struct vk_descriptor_config reticle_descriptor = 
	{
		.type             = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.stage_flags      = VK_SHADER_STAGE_VERTEX_BIT,
		.offset_in_buffer = offsetof(struct vk_host_memory, global) + offsetof(struct vk_ubo_global, reticle),
		.range_in_buffer  = sizeof(struct vk_ubo_global_reticle)
	};

	struct vk_attribute_description reticle_attribute = 
	{
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(struct vk_reticle_vertex, pos)
	};

#define PIPELINES_COUNT 2

	struct pipeline_init_data 
	{
		struct vk_pipeline_resources*    resources;
		size_t                           vertex_stride;
		
		VkShaderModule                   vertex_shader;
		VkShaderModule                   fragment_shader;

		struct vk_descriptor_config*     descriptors;
		uint8_t                          descriptors_len;

		struct vk_attribute_description* attributes;
		uint8_t                          attributes_len;
	} pipeline_init_datas[PIPELINES_COUNT] = {
		{
			.resources       = &vk.pipeline_resources_world,
			.vertex_stride   = sizeof(struct vk_cube_vertex),

			.vertex_shader   = vk_create_shader_module(vk.device, "shaders/world_vert.spv"),
			.fragment_shader = vk_create_shader_module(vk.device, "shaders/world_frag.spv"),

			.descriptors     = world_descriptors,
			.descriptors_len = WORLD_DESCRIPTORS_COUNT,

			.attributes      = world_attributes,
			.attributes_len  = WORLD_ATTRIBUTES_COUNT
		},
		{
			.resources       = &vk.pipeline_resources_reticle,
			.vertex_stride   = sizeof(struct vk_reticle_vertex),

			.vertex_shader   = vk_create_shader_module(vk.device, "shaders/reticle_vert.spv"),
			.fragment_shader = vk_create_shader_module(vk.device, "shaders/reticle_frag.spv"),

			.descriptors     = &reticle_descriptor,
			.descriptors_len = 1,

			.attributes      = &reticle_attribute,
			.attributes_len  = 1
		} 
	};

	// Pipeline creation loop
	for(uint8_t pipe_idx = 0; pipe_idx < PIPELINES_COUNT; pipe_idx++)
	{
		struct pipeline_init_data* pipe = &pipeline_init_datas[pipe_idx];
		
		VkDescriptorSetLayoutBinding descriptor_bindings[pipe->descriptors_len] = {};
		VkDescriptorPoolSize         pool_sizes         [pipe->descriptors_len] = {};
		VkWriteDescriptorSet         write_descriptors  [pipe->descriptors_len] = {};

		for(uint8_t i = 0; i < pipe->descriptors_len; i++)
		{
			descriptor_bindings[i].binding         = i;
			descriptor_bindings[i].descriptorType  = pipe->descriptors[i].type;
			// CONSIDER - Multiple descriptors in a given binding?
			descriptor_bindings[i].descriptorCount = 1; 
			descriptor_bindings[i].stageFlags      = pipe->descriptors[i].stage_flags;

			pool_sizes[i].type            = pipe->descriptors[i].type;
			pool_sizes[i].descriptorCount = 1;

			write_descriptors[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptors[i].dstSet          = pipe->resources->descriptor_set;
			write_descriptors[i].dstBinding      = i;
			write_descriptors[i].dstArrayElement = 0;
			write_descriptors[i].descriptorType  = pipe->descriptors[i].type;
			write_descriptors[i].descriptorCount = 1;

			if(pipe->descriptors[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
			{
				VkDescriptorBufferInfo buf_info = 
				{
					.buffer = vk.host_visible_buffer,
					.offset = pipe->descriptors[i].offset_in_buffer,
					.range  = pipe->descriptors[i].range_in_buffer 
				};

				write_descriptors[i].pBufferInfo = &buf_info;
			}
			else if(pipe->descriptors[i].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				VkDescriptorImageInfo image_info = 
				{
					.sampler     = vk.texture_sampler,
					.imageView   = vk.texture_view,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};

				write_descriptors[i].pImageInfo = &image_info;
			}
		}

		VkDescriptorSetLayoutCreateInfo layout_info = {};
		layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = pipe->descriptors_len;
		layout_info.pBindings    = descriptor_bindings;
		VK_VERIFY(vkCreateDescriptorSetLayout(vk.device, &layout_info, 0, &pipe->resources->descriptor_layout));

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.poolSizeCount = pipe->descriptors_len;
		pool_info.pPoolSizes    = pool_sizes;
		pool_info.maxSets       = 1;
		VK_VERIFY(vkCreateDescriptorPool(vk.device, &pool_info, 0, &pipe->resources->descriptor_pool));

		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool     = pipe->resources->descriptor_pool;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts        = &pipe->resources->descriptor_layout;
		VK_VERIFY(vkAllocateDescriptorSets(vk.device, &alloc_info, &pipe->resources->descriptor_set));

		vkUpdateDescriptorSets(vk.device, pipe->descriptors_len, write_descriptors, 0, 0);

		// Shaders
		uint8_t shader_infos_len = 2;
		VkPipelineShaderStageCreateInfo shader_infos[shader_infos_len] = {};

		shader_infos[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_infos[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		shader_infos[0].module = pipe->vertex_shader;
		shader_infos[0].pName  = "main";

		shader_infos[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_infos[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_infos[1].module = pipe->fragment_shader;
		shader_infos[1].pName  = "main";

		uint8_t bind_descriptions_len = 1;
		VkVertexInputBindingDescription bind_descriptions[bind_descriptions_len] = {};
		bind_descriptions[0].binding   = 0;
		bind_descriptions[0].stride    = pipe->vertex_stride;
		bind_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription attr_descriptions[pipe->attributes_len] = {};

		for(uint8_t i = 0; i < pipe->attributes_len; i++)
		{
			attr_descriptions[i].binding  = 0;
			attr_descriptions[i].location = i;
			attr_descriptions[i].format   = pipe->attributes[i].format;
			attr_descriptions[i].offset   = pipe->attributes[i].offset;
		}

		VkPipelineVertexInputStateCreateInfo vert_input_info = {};
		vert_input_info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vert_input_info.vertexBindingDescriptionCount   = bind_descriptions_len;
		vert_input_info.pVertexBindingDescriptions      = bind_descriptions;
		vert_input_info.vertexAttributeDescriptionCount = pipe->attributes_len;
		vert_input_info.pVertexAttributeDescriptions    = attr_descriptions;

		VkPipelineLayoutCreateInfo pipeline_layout_info = {};
		pipeline_layout_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = 1;
		pipeline_layout_info.pSetLayouts    = &pipe->resources->descriptor_layout;
		VK_VERIFY(vkCreatePipelineLayout(vk.device, &pipeline_layout_info, 0, &pipe->resources->pipeline_layout));

		VkGraphicsPipelineCreateInfo pipeline_info = {};
		pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.pNext               = &pipeline_render_info;
		pipeline_info.renderPass          = VK_NULL_HANDLE;
		pipeline_info.pInputAssemblyState = &input_assembly_info;
		pipeline_info.pRasterizationState = &raster_info;
		pipeline_info.pColorBlendState    = &color_blend_info;
		pipeline_info.pMultisampleState   = &multisample_info;
		pipeline_info.pViewportState      = &viewport_info;
		pipeline_info.pDepthStencilState  = &depth_stencil_info;
		pipeline_info.pDynamicState       = &dynamic_info;
		pipeline_info.pVertexInputState   = &vert_input_info;
		pipeline_info.stageCount          = shader_infos_len;
		pipeline_info.pStages             = shader_infos;
		pipeline_info.layout              = pipe->resources->pipeline_layout;
		VK_VERIFY(vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipeline_info, 0, &pipe->resources->pipeline));

		vkDestroyShaderModule(vk.device, pipe->vertex_shader,   0);
		vkDestroyShaderModule(vk.device, pipe->fragment_shader, 0);
	}

	// Create command pool and allocate command buffers
	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = graphics_family_idx;
	VK_VERIFY(vkCreateCommandPool(vk.device, &pool_info, 0, &vk.command_pool));

	VkCommandBufferAllocateInfo buf_info = {};
	buf_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	buf_info.commandPool        = vk.command_pool;
	buf_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	buf_info.commandBufferCount = 1;
	VK_VERIFY(vkAllocateCommandBuffers(vk.device, &buf_info, &vk.command_buffer));

	// Create texture sampler.
	VkSamplerCreateInfo sampler_info = 
	{
		.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext                   = 0,
		.flags                   = 0,
		.magFilter               = VK_FILTER_LINEAR,
		.minFilter               = VK_FILTER_LINEAR,
		.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias              = 0.0f,
		.anisotropyEnable        = VK_TRUE,
		.maxAnisotropy           = vk.max_sampler_anisotropy,
		.compareEnable           = VK_FALSE,
		.compareOp               = VK_COMPARE_OP_ALWAYS,
		.minLod                  = 0.0f,
		.maxLod                  = 0.0f,
		.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE
	};

	VK_VERIFY(vkCreateSampler(vk.device, &sampler_info, 0, &vk.texture_sampler));

	// Allocate texture image.
	// Load image from disk.
	int32_t tex_w;
	int32_t tex_h;
	int32_t tex_channels;
	stbi_uc* pixels = stbi_load("stone.bmp", &tex_w, &tex_h, &tex_channels, STBI_rgb_alpha);
	if(!pixels)
	{
		printf("Failed to load image file.\n");
		PANIC();
	}

	// Allocate staging buffer.
	VkDeviceSize img_size = tex_w * tex_h * 4;
	VkBuffer staging_buf;
	VkDeviceMemory staging_mem;

	vk_allocate_buffer(
		vk.device, 
		vk.physical_device,
		&staging_buf, 
		&staging_mem,
		img_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* data;
	vkMapMemory(vk.device, staging_mem, 0, img_size, 0, &data);
	memcpy(data, pixels, (size_t)img_size);
	vkUnmapMemory(vk.device, staging_mem);

	stbi_image_free(pixels);

	// Allocate image memory.
	struct vk_image_allocation alloc =
	{
		.image        = &vk.texture_image,
		.memory       = &vk.texture_memory,
		.width        = tex_w,
		.height       = tex_h,
		.format       = VK_FORMAT_R8G8B8A8_SRGB,
		.sample_count = VK_SAMPLE_COUNT_1_BIT,
		.usage_mask   = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
	};
	vk_allocate_image(vk.device, vk.physical_device, &alloc);

	// Transfer image from staging buffer to 
	VkCommandBuffer cmd = vk_start_transient_commands(vk.device, vk.command_pool);
	{
		vk_image_memory_barrier(
			cmd,
			vk.texture_image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkBufferImageCopy region = 
		{
			.bufferOffset      = 0,
			.bufferRowLength   = 0,
			.bufferImageHeight = 0,
			.imageSubresource  = (VkImageSubresourceLayers)
			{ 
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1
			},
			.imageOffset       = (VkOffset3D){0, 0, 0},
			.imageExtent       = (VkExtent3D){alloc.width, alloc.height, 1}
		};

		vkCmdCopyBufferToImage(
			cmd,
			staging_buf,
			vk.texture_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);
	}
	vk_end_transient_commands(cmd, vk.queue_graphics, vk.device, vk.command_pool);

	vkDestroyBuffer(vk.device, staging_buf, 0);
	vkFreeMemory(vk.device, staging_mem, 0);

	// Create texture image view
	vk_create_image_view(
		vk.device,
		(struct vk_image_view_config)
		{
			.view        = &vk.texture_view,
			.image       = vk.texture_image,
			.format      = VK_FORMAT_R8G8B8A8_SRGB,
			.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
		});

	// Allocate device local memory buffer
#define MESHES_LEN 2
	// LATER - This will be made into a for loop once we have mesh assets.
	vk.mesh_data_cube = (struct vk_mesh_data)
	{
		.vertex_memory = (void*)cube_vertices,
		.index_memory  = (void*)cube_indices,
		.vertex_stride = sizeof(struct vk_cube_vertex),
		.vertices_len  = CUBE_VERTICES_LEN,
		.indices_len   = CUBE_INDICES_LEN
	};

	vk.mesh_data_reticle = (struct vk_mesh_data)
	{
		.vertex_memory = (void*)reticle_vertices,
		.index_memory  = (void*)reticle_indices,
		.vertex_stride = sizeof(struct vk_reticle_vertex),
		.vertices_len  = RETICLE_VERTICES_LEN,
		.indices_len   = RETICLE_INDICES_LEN
	};

	struct vk_mesh_data* mesh_datas[MESHES_LEN] = {&vk.mesh_data_cube, &vk.mesh_data_reticle};
	size_t mesh_vert_buffer_sizes[MESHES_LEN];
	size_t mesh_index_buffer_sizes[MESHES_LEN];

	size_t staging_buf_size = 0;
	for(uint8_t i = 0; i < MESHES_LEN; i++)
	{
		mesh_vert_buffer_sizes[i]  = mesh_datas[i]->vertex_stride * mesh_datas[i]->vertices_len;
		mesh_index_buffer_sizes[i] = sizeof(uint16_t)             * mesh_datas[i]->indices_len;

		mesh_datas[i]->buffer_offset_vertex = staging_buf_size;
		mesh_datas[i]->buffer_offset_index  = staging_buf_size + mesh_vert_buffer_sizes[i];
		
		staging_buf_size += mesh_vert_buffer_sizes[i] + mesh_index_buffer_sizes[i];
	}

	//VkBuffer       staging_buf;
	VkDeviceMemory staging_buf_mem;

	vk_allocate_buffer(
		vk.device,
		vk.physical_device,
		&staging_buf,
		&staging_buf_mem,
		staging_buf_size, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* buf_data;
	vkMapMemory(vk.device, staging_buf_mem, 0, staging_buf_size, 0, &buf_data);
	{
		size_t total_offset = 0;
		for(uint8_t i = 0; i < MESHES_LEN; i++)
		{
			memcpy(buf_data + mesh_datas[i]->buffer_offset_vertex, mesh_datas[i]->vertex_memory, mesh_vert_buffer_sizes[i]);
			memcpy(buf_data + mesh_datas[i]->buffer_offset_index,  mesh_datas[i]->index_memory,  mesh_index_buffer_sizes[i]);
			total_offset += mesh_vert_buffer_sizes[i] + mesh_index_buffer_sizes[i];
		}
	}
	vkUnmapMemory(vk.device, staging_buf_mem);

	vk_allocate_buffer(
		vk.device,
		vk.physical_device,
		&vk.device_local_buffer,
		&vk.device_local_memory,
		staging_buf_size, 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	cmd = vk_start_transient_commands(vk.device, vk.command_pool);
	{
		VkBufferCopy copy = {};
		copy.size = staging_buf_size;
		vkCmdCopyBuffer(cmd, staging_buf, vk.device_local_buffer, 1, &copy);
	}
	vk_end_transient_commands(cmd, vk.queue_graphics, vk.device, vk.command_pool);

	vkDestroyBuffer(vk.device, staging_buf, 0);
	vkFreeMemory(vk.device, staging_buf_mem, 0);

	return vk;
}
