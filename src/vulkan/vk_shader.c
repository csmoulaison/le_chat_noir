VkShaderModule vk_create_shader_module(VkDevice device, const char* fname)
{
	FILE* file = fopen(fname, "r");
	if(!file)
	{
		printf("Failed to open file: %s\n", fname);
		PANIC();
	}
	fseek(file, 0, SEEK_END);
	uint32_t fsize = ftell(file);
	fseek(file, 0, SEEK_SET);
	char src[fsize];

	char c;
	uint32_t i = 0;
	while((c = fgetc(file)) != EOF)
	{
		src[i] = c;
		i++;
	}
	fclose(file);
	
	VkShaderModuleCreateInfo info = {};
	info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = fsize;
	info.pCode    = (uint32_t*)src;

	VkShaderModule module;
	VK_VERIFY(vkCreateShaderModule(device, &info, 0, &module));
	
	return module;
}
