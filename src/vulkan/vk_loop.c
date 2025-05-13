void insert_image_memory_barrier(
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

void vk_loop(struct vk_context* vk, struct render_group* render_group)
{
	// Translate game memory to uniform buffer object memory.
	struct vk_host_memory mem = {};
	{
		mem.global.world.clear_color = render_group->clear_color;
		mem.global.world.max_draw_distance_z = render_group->max_draw_distance_z;

		glm_lookat(
    		render_group->camera_position.data, 
    		render_group->camera_target.data,
    		(vec3){0, 1, 0}, 
    		mem.global.world.view);
		glm_perspective(radians(75), (float)vk->swap_extent.width / (float)vk->swap_extent.height, .1, 100, mem.global.world.projection);
		mem.global.world.projection[1][1] *= -1;

		mem.global.reticle.pos = render_group->reticle_offset;
		mem.global.reticle.scale_y = (float)vk->swap_extent.width / (float)vk->swap_extent.height;

		for(uint32_t i = 0; i < CUBES_LEN; i++)
		{
			memcpy(mem.instance.models[i], render_group->cube_transforms[i].data, sizeof(struct m4));
			//glm_translate(mem.instance.models[i], (vec3){(float)i * 2, 0, 0});
		}
	}
	memcpy(vk->host_visible_mapped, &mem, sizeof(mem));


	uint32_t image_idx;
	VkResult res = vkAcquireNextImageKHR(
		vk->device, 
		vk->swapchain, 
		UINT64_MAX, 
		vk->semaphore_image_available, 
		VK_NULL_HANDLE, 
		&image_idx);
	if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		vk_init_swapchain(vk, true);
		return;
	}

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(vk->command_buffer, &begin_info);
	{
		// Render image transfer
		insert_image_memory_barrier(
			vk->command_buffer, 
			vk->swap_images[image_idx], 
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			//VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			//VK_PIPELINE_STAGE_TRANSFER_BIT);
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		// Depth image transfer
		insert_image_memory_barrier(
			vk->command_buffer, 
			vk->depth_image, 
			VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			0,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		VkRenderingAttachmentInfo color_attachment = {};
		color_attachment.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		color_attachment.loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp            = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_attachment.imageView          = vk->render_view;
		color_attachment.resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
		color_attachment.resolveImageView   = vk->swap_views[image_idx];
		color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
		color_attachment.clearValue.color   = 
			(VkClearColorValue)
			{{
				render_group->clear_color.r, 
				render_group->clear_color.g, 
				render_group->clear_color.b, 
				1.0f
			}};

		VkRenderingAttachmentInfo depth_attachment = {};
		depth_attachment.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depth_attachment.imageView               = vk->depth_view;
		depth_attachment.resolveMode             = VK_RESOLVE_MODE_NONE;
		depth_attachment.clearValue.depthStencil = 
			(VkClearDepthStencilValue)
			{
				1.0f,
				0
			};

		VkRenderingInfo render_info = {};
		render_info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
		render_info.renderArea           = (VkRect2D){{0, 0}, vk->swap_extent};
		render_info.layerCount           = 1;
		render_info.colorAttachmentCount = 1;
		render_info.pColorAttachments    = &color_attachment;
		render_info.pDepthAttachment     = &depth_attachment;
		render_info.pStencilAttachment   = 0;

		vkCmdBeginRendering(vk->command_buffer, &render_info);
		{
			VkViewport viewport = {};
			viewport.x        = 0;
			viewport.y        = 0;
			viewport.width    = (float)vk->swap_extent.width;
			viewport.height   = (float)vk->swap_extent.height;
			viewport.minDepth = 0;
			viewport.maxDepth = 1;
			vkCmdSetViewport(vk->command_buffer, 0, 1, &viewport);

			VkRect2D scissor = {};
			scissor.offset = (VkOffset2D){0, 0};
			scissor.extent = vk->swap_extent;
			vkCmdSetScissor(vk->command_buffer, 0, 1, &scissor);

			{
				vkCmdBindPipeline(
					vk->command_buffer, 
					VK_PIPELINE_BIND_POINT_GRAPHICS, 
					vk->pipeline_resources_world.pipeline);

				VkDeviceSize offsets[] = {vk->mesh_data_cube.buffer_offset_vertex};
				vkCmdBindVertexBuffers(
					vk->command_buffer, 
					0, 
					1, 
					&vk->device_local_buffer,
					offsets);
				vkCmdBindIndexBuffer(
					vk->command_buffer, 
					vk->device_local_buffer, 
					vk->mesh_data_cube.buffer_offset_index, 
					VK_INDEX_TYPE_UINT16);

				for(uint16_t i = 0; i < MAX_INSTANCES; i++) {
					uint32_t dyn_off = i * sizeof(mat4);
					vkCmdBindDescriptorSets(
						vk->command_buffer, 
						VK_PIPELINE_BIND_POINT_GRAPHICS, 
						vk->pipeline_resources_world.pipeline_layout, 
						0, 
						1, 
						&vk->pipeline_resources_world.descriptor_set,
						1,
						&dyn_off);

					vkCmdDrawIndexed(vk->command_buffer, vk->mesh_data_cube.indices_len, 1, 0, 0, 0);
				}
			}

			{
				vkCmdBindPipeline(
					vk->command_buffer, 
					VK_PIPELINE_BIND_POINT_GRAPHICS, 
					vk->pipeline_resources_reticle.pipeline);

				VkDeviceSize offset = {vk->mesh_data_reticle.buffer_offset_vertex};
				vkCmdBindVertexBuffers(
					vk->command_buffer, 
					0, 
					1, 
					&vk->device_local_buffer,
					&offset);
				vkCmdBindIndexBuffer(
					vk->command_buffer, 
					vk->device_local_buffer, 
					vk->mesh_data_reticle.buffer_offset_index, 
					VK_INDEX_TYPE_UINT16);

				vkCmdBindDescriptorSets(
					vk->command_buffer, 
					VK_PIPELINE_BIND_POINT_GRAPHICS, 
					vk->pipeline_resources_reticle.pipeline_layout, 
					0, 
					1, 
					&vk->pipeline_resources_reticle.descriptor_set,
					0,
					0);

				vkCmdDrawIndexed(vk->command_buffer, vk->mesh_data_reticle.indices_len, 1, 0, 0, 0);
			}
		}
		vkCmdEndRendering(vk->command_buffer);

		insert_image_memory_barrier(
			vk->command_buffer, 
			vk->swap_images[image_idx], 
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}
	vkEndCommandBuffer(vk->command_buffer);

	VkPipelineStageFlags wait_stages[] = 
	{
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	// We wait to submit until that images is available from before. We did all
	// this prior stuff in the meantime, in theory.
	VkSubmitInfo submit_info = {};
	submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount   = 1;
	submit_info.pWaitSemaphores      = &vk->semaphore_image_available;
	submit_info.pWaitDstStageMask    = wait_stages;
	submit_info.commandBufferCount   = 1;
	submit_info.pCommandBuffers      = &vk->command_buffer;
	submit_info.pSignalSemaphores    = &vk->semaphore_render_finished;
	submit_info.signalSemaphoreCount = 1;
	vkQueueSubmit(vk->queue_graphics, 1, &submit_info, VK_NULL_HANDLE);

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk->semaphore_render_finished;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk->swapchain;
	present_info.pImageIndices = &image_idx;

	// CONSIDER - See if tracking the present queue and using that here would speed
	// things up.
	res = vkQueuePresentKHR(vk->queue_graphics, &present_info); 
	if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		vk_init_swapchain(vk, true);
		return;
	}

	vkDeviceWaitIdle(vk->device);
}
