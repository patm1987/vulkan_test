#include "stdafx.h"

#include "VulkanRenderer.h"

#define STRINGIFY(s) STR(s)
#define STR(s) #s
#define PRINT_VK_RESULT(result) case result: printf( STRINGIFY(result) "\n"); break;

void PrintResult(VkResult result) {
	printf("Vulkan Result: ");
	switch (result) {
		PRINT_VK_RESULT(VK_SUCCESS);
		PRINT_VK_RESULT(VK_NOT_READY);
		PRINT_VK_RESULT(VK_TIMEOUT);
		PRINT_VK_RESULT(VK_EVENT_SET);
		PRINT_VK_RESULT(VK_EVENT_RESET);
		PRINT_VK_RESULT(VK_INCOMPLETE);
		PRINT_VK_RESULT(VK_SUBOPTIMAL_KHR);
		PRINT_VK_RESULT(VK_ERROR_OUT_OF_HOST_MEMORY);
		PRINT_VK_RESULT(VK_ERROR_OUT_OF_DEVICE_MEMORY);
		PRINT_VK_RESULT(VK_ERROR_INITIALIZATION_FAILED);
		PRINT_VK_RESULT(VK_ERROR_MEMORY_MAP_FAILED);
		PRINT_VK_RESULT(VK_ERROR_DEVICE_LOST);
		PRINT_VK_RESULT(VK_ERROR_EXTENSION_NOT_PRESENT);
		PRINT_VK_RESULT(VK_ERROR_FEATURE_NOT_PRESENT);
		PRINT_VK_RESULT(VK_ERROR_LAYER_NOT_PRESENT);
		PRINT_VK_RESULT(VK_ERROR_INCOMPATIBLE_DRIVER);
		PRINT_VK_RESULT(VK_ERROR_TOO_MANY_OBJECTS);
		PRINT_VK_RESULT(VK_ERROR_FORMAT_NOT_SUPPORTED);
		PRINT_VK_RESULT(VK_ERROR_SURFACE_LOST_KHR);
		PRINT_VK_RESULT(VK_ERROR_OUT_OF_DATE_KHR);
		PRINT_VK_RESULT(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
		PRINT_VK_RESULT(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
		PRINT_VK_RESULT(VK_ERROR_VALIDATION_FAILED_EXT);
	default:
		printf("Unknown Result %d\n", result);
	}
}

struct vulkan_renderer_t {
	VkInstance instance;
};

VulkanRenderer* VulkanRenderer_Create() {
	VulkanRenderer* pVulkanRenderer = (VulkanRenderer*)malloc(sizeof(VulkanRenderer*));

	VkResult result;

	// initialize vulkan
	VkInstanceCreateInfo createInfo = { 0 };
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.pApplicationInfo = NULL;
	createInfo.enabledLayerCount = 0;
	createInfo.ppEnabledLayerNames = NULL;
	createInfo.enabledExtensionCount = 0;
	createInfo.ppEnabledExtensionNames = NULL;
	result = vkCreateInstance(&createInfo, NULL, &pVulkanRenderer->instance);
	if (result != VK_SUCCESS) {
		PrintResult(result);
		exit(1);
	}

	// get all the physical devices
	// find one we like
	// make a logical device

	return pVulkanRenderer;
}

void VulkanRenderer_Render(VulkanRenderer* pThis) {
	assert(pThis);
}

void VulkanRenderer_Destroy(VulkanRenderer* pThis) {
	assert(pThis);

	vkDestroyInstance(pThis->instance, NULL);
	free(pThis);
}
