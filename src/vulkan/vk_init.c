struct vk_context vk_init(struct vk_platform* platform)
{
	struct vk_context vk;

	// Create instance.
	VkApplicationInfo app = {};
	app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pNext              = 0;
	app.pApplicationName   = PROGRAM_NAME;
	app.applicationVersion = 1;
	app.pEngineName        = 0;
	app.engineVersion      = 0;
	app.apiVersion         = VK_API_VERSION_1_3;

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

	VkInstanceCreateInfo inst_info = {};
	inst_info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pNext            = 0;
	inst_info.flags            = 0;
	inst_info.pApplicationInfo = &app;

	uint32_t layers_len = 0;

	#ifdef VK_DEBUG
		layers_len++;
	#endif

	const char* layers[layers_len];

#if VK_DEBUG
		layers[layers_len - 1] = "VK_LAYER_KHRONOS_validation";
#endif

	inst_info.enabledLayerCount = layers_len;
	inst_info.ppEnabledLayerNames = layers;

	inst_info.enabledExtensionCount = exts_len;
	inst_info.ppEnabledExtensionNames = exts;

	VK_VERIFY(vkCreateInstance(&inst_info, 0, &vk.instance));

	// Create surface.
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

		vk.physical_device = devices[i];

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(vk.physical_device, &properties);
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
	VkDeviceQueueCreateInfo queue = {};
	queue.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue.pNext            = 0;
	queue.flags            = 0;
	queue.queueFamilyIndex = graphics_family_idx;
	queue.queueCount       = 1; // 1 because only 1 queue, right?
	float priority         = 1.0f;
 	queue.pQueuePriorities = &priority;

 	VkPhysicalDeviceDynamicRenderingFeatures dynamic_features = {};
	dynamic_features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	dynamic_features.pNext            = 0;
	dynamic_features.dynamicRendering = VK_TRUE;


	VkPhysicalDeviceFeatures2 features = {};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext = &dynamic_features;
	vkGetPhysicalDeviceFeatures2(vk.physical_device, &features);
	
	VkDeviceCreateInfo device_info = {};
	device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
 	device_info.pNext                   = &features;
 	device_info.flags                   = 0;
 	device_info.queueCreateInfoCount    = 1;
 	device_info.pQueueCreateInfos       = &queue;
 	const char* device_exts[2] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
 	device_info.enabledExtensionCount   = 2;
 	device_info.ppEnabledExtensionNames = device_exts;
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
#define WORLD_DESCRIPTORS_COUNT 2
	struct vk_descriptor_info world_descriptors[WORLD_DESCRIPTORS_COUNT] = 
	{
		{
			.type             = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.offset_in_buffer = offsetof(struct vk_host_memory, global),
			.range_in_buffer  = sizeof(struct vk_ubo_global_world)
		},
		{
			.type             = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.offset_in_buffer = offsetof(struct vk_host_memory, instance),
			.range_in_buffer  = sizeof(mat4)
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

#define RETICLE_DESCRIPTORS_COUNT 1
	struct vk_descriptor_info reticle_descriptors[RETICLE_DESCRIPTORS_COUNT] = 
	{
		{
			.type             = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.offset_in_buffer = 
				offsetof(struct vk_host_memory, global) 
				+ offsetof(struct vk_ubo_global, reticle),
			.range_in_buffer  = sizeof(struct vk_ubo_global_reticle)
		}
	};

#define RETICLE_ATTRIBUTES_COUNT 1
	struct vk_attribute_description reticle_attributes[RETICLE_ATTRIBUTES_COUNT] =
	{
		{
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(struct vk_reticle_vertex, pos)
		}
	};

#define PIPELINES_COUNT 2

	struct pipeline_init_data
	{
		struct vk_pipeline_resources*    resources;
		size_t                           vertex_stride;
		
		VkShaderModule                   vertex_shader;
		VkShaderModule                   fragment_shader;

		struct vk_descriptor_info*       descriptors;
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

			.descriptors     = reticle_descriptors,
			.descriptors_len = RETICLE_DESCRIPTORS_COUNT,

			.attributes      = reticle_attributes,
			.attributes_len  = RETICLE_ATTRIBUTES_COUNT
		}
	};

	// The following create infos aren't specific to an individual pipeline, so
	// have been hoisted out of the below pipeline creation loop.

	// VOLATILE - Length must match dynamic_info.dynamicStateCount.
	VkDynamicState dyn_states[2] = 
	{
		VK_DYNAMIC_STATE_VIEWPORT, 
		VK_DYNAMIC_STATE_SCISSOR 
	};

	VkPipelineRenderingCreateInfoKHR pipeline_render_info = {};
	pipeline_render_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR; 
	pipeline_render_info.pNext                   = VK_NULL_HANDLE; 
	pipeline_render_info.colorAttachmentCount    = 1; 
	pipeline_render_info.pColorAttachmentFormats = &vk.surface_format.format;
	pipeline_render_info.depthAttachmentFormat   = DEPTH_ATTACHMENT_FORMAT;
	pipeline_render_info.stencilAttachmentFormat = 0;

	VkPipelineDynamicStateCreateInfo dynamic_info = {};
	dynamic_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_info.dynamicStateCount = 2;
	dynamic_info.pDynamicStates    = dyn_states;

	VkPipelineViewportStateCreateInfo viewport_info = {};
	viewport_info.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_info.viewportCount = 1;
	viewport_info.scissorCount  = 1;
	// pViewports and pScissors null because they're set later during rendering.
	viewport_info.pViewports    = 0; 
	viewport_info.pScissors     = 0;

	VkPipelineRasterizationStateCreateInfo raster_info = {};
	raster_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster_info.depthClampEnable        = VK_FALSE;
	raster_info.rasterizerDiscardEnable = VK_FALSE;
	raster_info.polygonMode             = VK_POLYGON_MODE_FILL;
	raster_info.lineWidth               = 1.0f;
	raster_info.cullMode                = VK_CULL_MODE_NONE;
	raster_info.frontFace               = VK_FRONT_FACE_CLOCKWISE;
	raster_info.depthBiasEnable         = VK_FALSE;
	raster_info.depthBiasConstantFactor = 0.0f;
	raster_info.depthBiasClamp          = 0.0f;
	raster_info.depthBiasSlopeFactor    = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisample_info = {};
	multisample_info.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_info.sampleShadingEnable   = VK_FALSE;
	multisample_info.rasterizationSamples  = vk.render_sample_count;

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

	// Pipeline creation loop
	for(uint8_t pipe_idx = 0; pipe_idx < PIPELINES_COUNT; pipe_idx++)
	{
		struct pipeline_init_data* pipe = &pipeline_init_datas[pipe_idx];
		
		VkDescriptorSetLayoutBinding descriptor_bindings[pipe->descriptors_len] = {};
		for(uint8_t i = 0; i < pipe->descriptors_len; i++)
		{
			descriptor_bindings[i].binding         = i;
			descriptor_bindings[i].descriptorType  = pipe->descriptors[i].type;
			// CONSIDER - Multiple descriptors in a given binding?
			descriptor_bindings[i].descriptorCount = 1; 
			descriptor_bindings[i].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
		}

		VkDescriptorSetLayoutCreateInfo layout_info = {};
		layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = pipe->descriptors_len;
		layout_info.pBindings    = descriptor_bindings;
		VK_VERIFY(vkCreateDescriptorSetLayout(vk.device, &layout_info, 0, &pipe->resources->descriptor_layout));

		VkDescriptorPoolSize pool_sizes[pipe->descriptors_len] = {};
		for(uint8_t i = 0; i < pipe->descriptors_len; i++)
		{
			pool_sizes[i].type            = pipe->descriptors[i].type;
			pool_sizes[i].descriptorCount = 1;
		}

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

		VkDescriptorBufferInfo buf_infos[pipe->descriptors_len] = {};
		VkWriteDescriptorSet write_descriptors[pipe->descriptors_len] = {};
		for(uint8_t i = 0; i < pipe->descriptors_len; i++)
		{
			buf_infos[i].buffer = vk.host_visible_buffer;
			buf_infos[i].offset = pipe->descriptors[i].offset_in_buffer;
			buf_infos[i].range  = pipe->descriptors[i].range_in_buffer;

			write_descriptors[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptors[i].dstSet          = pipe->resources->descriptor_set;
			write_descriptors[i].dstBinding      = i;
			write_descriptors[i].dstArrayElement = 0;
			write_descriptors[i].descriptorType  = pipe->descriptors[i].type;
			write_descriptors[i].descriptorCount = 1;
			write_descriptors[i].pBufferInfo     = &buf_infos[i];
		}

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

	VkBuffer staging_buf;
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

	// NOTE - This will almost need to be abstracted away, but wait until we have
	// a real case.
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool        = vk.command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buf;
	vkAllocateCommandBuffers(vk.device, &alloc_info, &command_buf);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(command_buf, &begin_info);
	{
		VkBufferCopy copy = {};
		copy.size = staging_buf_size;
		vkCmdCopyBuffer(command_buf, staging_buf, vk.device_local_buffer, 1, &copy);
	}
	vkEndCommandBuffer(command_buf);

	VkSubmitInfo submit_info = {};
	submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &command_buf;

	// CONSIDER  - We are using the graphics queue for this presently, but might
	// we want to use a transfer queue for this?
	// I'm not sure if there's any possible performance boost, and I'd rather not
	// do it just for the sake of it.
	// 
	// High level on how this might be done at the following link:
	// https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
	vkQueueSubmit(vk.queue_graphics, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(vk.queue_graphics);

	vkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buf);
	vkDestroyBuffer(vk.device, staging_buf, 0);
	vkFreeMemory(vk.device, staging_buf_mem, 0);

	return vk;
}
