printf "Pre-analyzing code...\n"

# Ensure that the only call to vkAllocateMemory is done by our wrapper function,
# i.e. that it is only called once in our codebase.
if [[ $(grep -r vkAllocateMemory ../src/ | wc -l) != "1" ]]; then
	printf "Code analysis error: vkAllocateMemory called more than once. Use vk_allocate_memory() instead.\n\n"
	grep -r vkAllocateMemory ../src/
	exit 1
fi

exit 0
