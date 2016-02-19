#include "stdafx.h"

#include "VulkanRenderer.h"

#define STRINGIFY(s) STR(s)
#define STR(s) #s
#define PRINT_VK_RESULT(result) case result: printf( STRINGIFY(result) "\n"); break;

struct vulkan_renderer_t {
	VkInstance instance;
};

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

// order the devices by the kind we'd prefer to run on
VkPhysicalDeviceType aDevicePrecidents[] = {
	VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
	VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
	VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
	VK_PHYSICAL_DEVICE_TYPE_CPU,
	VK_PHYSICAL_DEVICE_TYPE_OTHER,
};

/*!
 * \brief	Determine if the \a newType of device is better than the \a oldType
 * \param	newType the new type of device
 * \param	oldType the old type of device
 * \return	TRUE if we think that \a newType is better than or equal to \a oldType
 */
BOOL DeviceTypeIsSuperior(VkPhysicalDeviceType newType, VkPhysicalDeviceType oldType) {
	for (int i = 0; i < sizeof(aDevicePrecidents) / sizeof(VkPhysicalDevice); i++) {
		VkPhysicalDeviceType currentPrecident = aDevicePrecidents[i];
		if (currentPrecident == newType) {
			return TRUE;
		}
		if (currentPrecident == oldType) {
			return FALSE;
		}
	}
	return TRUE;
}

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

	// first find the numer of devices
	uint32_t physicalDeviceCount;
	result = vkEnumeratePhysicalDevices(
		pVulkanRenderer->instance,
		&physicalDeviceCount,
		NULL);
	if (result != VK_SUCCESS) {
		PrintResult(result);
		exit(1);
	}

	// then get the devices
	VkPhysicalDevice* paPhysicalDevices = (VkPhysicalDevice*)malloc(
		sizeof(VkPhysicalDevice) * physicalDeviceCount);
	result = vkEnumeratePhysicalDevices(
		pVulkanRenderer->instance,
		&physicalDeviceCount,
		paPhysicalDevices);

	// find one we like
	VkPhysicalDevice chosenDevice = NULL;
	VkPhysicalDeviceProperties chosenDeviceProperties;
	for (uint32_t deviceIndex = 0; deviceIndex < physicalDeviceCount; deviceIndex++) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(paPhysicalDevices[deviceIndex], &deviceProperties);

		uint32_t queueFamilyPropertyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(
			paPhysicalDevices[deviceIndex],
			&queueFamilyPropertyCount,
			NULL);
		VkQueueFamilyProperties* paQueueFamilyProperties
			= (VkQueueFamilyProperties*)malloc(
				sizeof(VkQueueFamilyProperties) * queueFamilyPropertyCount);

		BOOL queueValid = FALSE;
		for (uint32_t queueIndex = 0; queueIndex < queueFamilyPropertyCount; queueIndex++) {
			if (paQueueFamilyProperties[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				queueValid = TRUE;
				break;
			}
		}

		if (!chosenDevice) {
			chosenDevice = paPhysicalDevices[deviceIndex];
			chosenDeviceProperties = deviceProperties;
		}
		else {
			if (DeviceTypeIsSuperior(
				deviceProperties.deviceType,
				chosenDeviceProperties.deviceType)) {

				chosenDevice = paPhysicalDevices[deviceIndex];
				chosenDeviceProperties = deviceProperties;
			}
		}

		// if we found a discreet GPU - that's probably ideal
		if (chosenDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			// we've struck gold!
			break;
		}

		free(paQueueFamilyProperties);
	}

	// make a logical device

	free(paPhysicalDevices);

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
