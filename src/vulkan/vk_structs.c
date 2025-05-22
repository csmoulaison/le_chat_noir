#define MAX_INSTANCES 256

// LATER - Our own m4 struct
struct vk_ubo_global_world
{
	alignas(16) mat4 view;
	alignas(16) mat4 projection;

	alignas(16) struct v3 clear_color;
	float                 max_draw_distance_z;
};

struct vk_ubo_global_reticle
{
	alignas(64) struct v2 pos;
	float                 scale_y;
};

struct vk_ubo_global
{
	alignas(64) struct vk_ubo_global_world   world;
	alignas(64) struct vk_ubo_global_reticle reticle;
};

struct vk_ubo_instance
{
	alignas(16) mat4 models[MAX_INSTANCES];
};

struct vk_host_memory
{
	alignas(64) struct vk_ubo_global global;
	alignas(64) struct vk_ubo_instance instance;
};

// CONSIDER - Only the following are used outside of initializtion, and indeed
// outside of the pipeline creation loop(?):
// - descriptor_set
// - pipeline
// - pipeline_layout
// Accordingly, do either layout and pool need to hang around?
struct vk_pipeline_resources
{
	VkDescriptorSetLayout descriptor_layout;
	VkDescriptorPool      descriptor_pool;
	VkDescriptorSet       descriptor_set;
	VkPipeline            pipeline;
	VkPipelineLayout      pipeline_layout;
};

struct vk_mesh_data
{
	void*    vertex_memory;
	void*    index_memory;

	// In bytes
	uint32_t vertex_stride;

	uint32_t vertices_len;
	uint32_t indices_len;

	uint32_t buffer_offset_vertex;
	uint32_t buffer_offset_index;
};

struct vk_context
{
	VkInstance                   instance;
	VkPhysicalDevice             physical_device;
	VkDevice                     device;
	VkSurfaceKHR                 surface;
	VkSurfaceFormatKHR           surface_format;

	VkQueue                      queue_graphics;
	VkQueue                      queue_present;

	VkImageView                  render_view;
	VkImage                      render_image;
	VkDeviceMemory               render_image_memory;
	VkSampleCountFlagBits        render_sample_count;

	VkImageView                  depth_view;
	VkImage                      depth_image;
	VkDeviceMemory               depth_image_memory;

	VkSwapchainKHR               swapchain;
	VkExtent2D                   swap_extent;
	VkImageView                  swap_views[MAX_SWAP_IMAGES];
	VkImage                      swap_images[MAX_SWAP_IMAGES];
	uint32_t                     swap_images_len;

	struct vk_pipeline_resources pipeline_resources_world;
	struct vk_pipeline_resources pipeline_resources_reticle;

	struct vk_mesh_data          mesh_data_cube;
	struct vk_mesh_data          mesh_data_reticle;

	VkBuffer                     device_local_buffer;
	VkDeviceMemory               device_local_memory;

	VkBuffer                     host_visible_buffer;
	VkDeviceMemory               host_visible_memory;
	void*                        host_visible_mapped;

	VkCommandPool                command_pool;
	VkCommandBuffer              command_buffer;

	VkSemaphore                  semaphore_image_available;
	VkSemaphore                  semaphore_render_finished;

	VkSampler                    texture_sampler;
	float                        max_sampler_anisotropy;

	VkImage                      texture_image;
	VkDeviceMemory               texture_memory;
	VkImageView                  texture_view;
};

struct vk_platform
{
	VkResult(*create_surface_callback)(struct vk_context* vk, void* context);
	void*   context;
	char**  window_extensions;
	uint8_t window_extensions_len;
};

struct vk_cube_vertex
{
	struct v3 pos;
	struct v3 color;
};

struct vk_reticle_vertex
{
	struct v2 pos;
};

struct vk_attribute_description
{
	VkFormat format;
	uint32_t offset;
};

struct vk_image_allocation
{
	VkImage*          image;
	VkDeviceMemory*   memory;
	uint32_t          width;
	uint32_t          height;
	VkFormat          format;
	uint32_t          sample_count;
	VkImageUsageFlags usage_mask;
};
