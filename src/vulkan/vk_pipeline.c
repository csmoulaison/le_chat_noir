void vk_create_graphics_pipeline(
	struct vk_context*               vk,
	struct vk_pipeline_resources*    resources,
	struct vk_descriptor_info*       descriptor_infos,
	uint8_t                          descriptors_len,
	struct vk_attribute_description* attribute_descriptions,
	uint8_t                          attribute_descriptions_len,
	size_t                           vertex_stride,
	VkShaderModule                   shader_vert,
	VkShaderModule                   shader_frag)
{
	VkDescriptorSetLayoutBinding ubo_bindings[descriptors_len] = {};
	for(uint8_t i = 0; i < descriptors_len; i++)
	{
		ubo_bindings[i].binding         = i;
		ubo_bindings[i].descriptorType  = descriptor_infos[i].type;
		ubo_bindings[i].descriptorCount = 1;
		ubo_bindings[i].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
	}

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = descriptors_len;
	layout_info.pBindings    = ubo_bindings;
	VK_VERIFY(vkCreateDescriptorSetLayout(vk->device, &layout_info, 0, &resources->descriptor_layout));

	VkDescriptorPoolSize pool_sizes[descriptors_len] = {};
	for(uint8_t i = 0; i < descriptors_len; i++)
	{
		pool_sizes[i].type            = descriptor_infos[i].type;
		pool_sizes[i].descriptorCount = 1;
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = descriptors_len;
	pool_info.pPoolSizes    = pool_sizes;
	pool_info.maxSets       = 1;
	VK_VERIFY(vkCreateDescriptorPool(vk->device, &pool_info, 0, &resources->descriptor_pool));

	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool     = resources->descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts        = &resources->descriptor_layout;
	VK_VERIFY(vkAllocateDescriptorSets(vk->device, &alloc_info, &resources->descriptor_set));

	VkDescriptorBufferInfo buf_infos[descriptors_len] = {};
	VkWriteDescriptorSet write_descriptors[descriptors_len] = {};
	for(uint8_t i = 0; i < descriptors_len; i++)
	{
		buf_infos[i].buffer = vk->host_visible_buffer;
		buf_infos[i].offset = descriptor_infos[i].offset_in_buffer;
		buf_infos[i].range  = descriptor_infos[i].range_in_buffer;

		write_descriptors[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptors[i].dstSet          = resources->descriptor_set;
		write_descriptors[i].dstBinding      = i;
		write_descriptors[i].dstArrayElement = 0;
		write_descriptors[i].descriptorType  = descriptor_infos[i].type;
		write_descriptors[i].descriptorCount = 1;
		write_descriptors[i].pBufferInfo     = &buf_infos[i];
	}

	vkUpdateDescriptorSets(vk->device, descriptors_len, write_descriptors, 0, 0);

	// Shaders
	uint8_t shader_infos_len = 2;
	VkPipelineShaderStageCreateInfo shader_infos[shader_infos_len] = {};

	shader_infos[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_infos[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	shader_infos[0].module = shader_vert;
	shader_infos[0].pName  = "main";

	shader_infos[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_infos[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_infos[1].module = shader_frag;
	shader_infos[1].pName  = "main";

	uint8_t bind_descriptions_len = 1;
	VkVertexInputBindingDescription bind_descriptions[bind_descriptions_len] = {};
	bind_descriptions[0].binding   = 0;
	bind_descriptions[0].stride    = vertex_stride;
	bind_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attr_descriptions[attribute_descriptions_len] = {};

	for(uint8_t i = 0; i < attribute_descriptions_len; i++)
	{
		attr_descriptions[i].binding  = 0;
		attr_descriptions[i].location = i;
		attr_descriptions[i].format   = attribute_descriptions[i].format;
		attr_descriptions[i].offset   = attribute_descriptions[i].offset;
	}

	VkPipelineVertexInputStateCreateInfo vert_input_info = {};
	vert_input_info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vert_input_info.vertexBindingDescriptionCount   = bind_descriptions_len;
	vert_input_info.pVertexBindingDescriptions      = bind_descriptions;
	vert_input_info.vertexAttributeDescriptionCount = attribute_descriptions_len;
	vert_input_info.pVertexAttributeDescriptions    = attr_descriptions;

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
	input_assembly_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	// VOLATILE - Length must match dynamic_info.dynamicStateCount.
	VkDynamicState dyn_states[2] = 
	{
		VK_DYNAMIC_STATE_VIEWPORT, 
		VK_DYNAMIC_STATE_SCISSOR 
	};
	VkPipelineDynamicStateCreateInfo dynamic_info = {};
	dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_info.dynamicStateCount = 2;
	dynamic_info.pDynamicStates = dyn_states;

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
	multisample_info.rasterizationSamples  = vk->render_sample_count;

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

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &resources->descriptor_layout;
	VK_VERIFY(vkCreatePipelineLayout(vk->device, &pipeline_layout_info, 0, &resources->pipeline_layout));

	VkPipelineRenderingCreateInfoKHR pipeline_render_info = {};
	pipeline_render_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR; 
	pipeline_render_info.pNext                   = VK_NULL_HANDLE; 
	pipeline_render_info.colorAttachmentCount    = 1; 
	pipeline_render_info.pColorAttachmentFormats = &vk->surface_format.format;
	pipeline_render_info.depthAttachmentFormat   = DEPTH_ATTACHMENT_FORMAT;
	pipeline_render_info.stencilAttachmentFormat = 0;

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
	pipeline_info.layout              = resources->pipeline_layout;
	VK_VERIFY(vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE, 1, &pipeline_info, 0, &resources->pipeline));

	vkDestroyShaderModule(vk->device, shader_vert, 0);
	vkDestroyShaderModule(vk->device, shader_frag, 0);
}
