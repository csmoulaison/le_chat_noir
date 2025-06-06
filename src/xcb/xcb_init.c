#define MEMORY_POOL_BYTES 1073741824

VkResult xcb_create_surface_callback(struct vk_context* vk, void* context)
{
	struct xcb_context* xcb = (struct xcb_context*)context;
	
	VkXcbSurfaceCreateInfoKHR info = {};
	info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	info.pNext      = 0;
	info.flags      = 0;
	info.connection = xcb->connection;
	info.window     = xcb->window;

	return vkCreateXcbSurfaceKHR(vk->instance, &info, 0, &vk->surface);
}

struct xcb_context xcb_init()
{
	struct xcb_context xcb;
	
	xcb.connection = xcb_connect(0, 0);
	// TODO - Handle more than 1 screen?
	xcb.screen = xcb_setup_roots_iterator(xcb_get_setup(xcb.connection)).data;

	// Window event registration
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[1];
	values[0] = 
		XCB_EVENT_MASK_EXPOSURE | 
		XCB_EVENT_MASK_KEY_PRESS | 
		XCB_EVENT_MASK_KEY_RELEASE | 
		XCB_EVENT_MASK_POINTER_MOTION | 
		XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	
	xcb.window = xcb_generate_id(xcb.connection);
	xcb_create_window(
		xcb.connection,
		XCB_COPY_FROM_PARENT,
		xcb.window,
		xcb.screen->root,
		0, 0,
		480, 480,
		1,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		xcb.screen->root_visual,
		mask, values);
	xcb_map_window(xcb.connection, xcb.window);

	//XGrabPointer(x11.display, x11.window, 1, PointerMotionMask, GrabModeAsync, GrabModeAsync, x11.window, None, CurrentTime);
	xcb_grab_pointer(xcb.connection, false, xcb.window, mask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
	xcb_xfixes_query_version(xcb.connection, 4, 0);
	xcb_xfixes_hide_cursor(xcb.connection, xcb.window);

	xcb_flush(xcb.connection);

	// This needs to be initialized in order for keysym lookups to work
	xcb.keysyms = xcb_key_symbols_alloc(xcb.connection);

	// TODO - can we get the width and height post window manager resize to pass
	// to vk_init? Alternatively, don't worry about it and just handle fullscreen.
	/*xcb_get_geometry_reply_t* geometry = xcb_get_geometry_reply(
		xcb.connection, 
		xcb_get_geometry(xcb.connection, xcb.window), 
		0);
	xcb.window_w = geometry->width;
	xcb.window_h = geometry->height;*/

	// VOLATILE - window_extensions_len must equal length of window_exts.
	char* window_exts[2] = 
	{
		"VK_KHR_surface", // TODO - find relevant _EXTENSION_NAME macro?
		VK_KHR_XCB_SURFACE_EXTENSION_NAME
	};

	struct vk_platform xcb_platform;
	xcb_platform.context = &xcb;
	xcb_platform.create_surface_callback = xcb_create_surface_callback;
	xcb_platform.window_extensions_len = 2;
	xcb_platform.window_extensions = window_exts;

	// TODO - doesn't match by the time we are making swapchain, so have to
	// hardcode it here. Whyyyyy?
	// Might have to resort to just waiting until xcb_loop COnfigureNotify to do this.
	xcb.vk = vk_init(&xcb_platform);

	xcb.running = true;

	xcb.input.mouse_x = 0;
	xcb.input.mouse_y = 0;
	xcb.input.mouse_delta_x = 0;
	xcb.input.mouse_delta_y = 0;
	for(uint32_t i = 0; i < INPUT_BUTTONS_LEN; i++) 
	{
		xcb.input.buttons[i].held = 0;
		xcb.input.buttons[i].pressed = 0;
		xcb.input.buttons[i].released = 0;
	}

	xcb.mouse_just_warped = false;
	xcb.mouse_moved_yet = false;

	// TODO - raw memory page allocation
	xcb.memory_pool = malloc(MEMORY_POOL_BYTES);
	xcb.memory_pool_bytes = MEMORY_POOL_BYTES;

	game_init(xcb.memory_pool, xcb.memory_pool_bytes);

    if(clock_gettime(CLOCK_REALTIME, &xcb.time_prev))
    {
        PANIC();
    }
    xcb.time_since_start = 0;

	return xcb;
}
