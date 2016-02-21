#include "stdafx.h"

#include "VulkanRenderer.h"

#include "MemoryUtils.h"
#include "ShaderManager.h"
#include "Utils.h"

// timeout in nanoseconds
#define FENCE_TIMEOUT 100000000

typedef struct vertex_t {
	float position [3];
	float color [3];
} Vertex;

typedef struct swap_chain_buffer_t {
	VkImage image;
	VkImageView view;
} SwapChainBuffer;

struct vulkan_renderer_t {
	uint32_t width;
	uint32_t height;

	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;

	uint32_t queueFamilyIndex;

	VkCommandPool commandPool;

	VkQueue mainQueue;

	ShaderManager* pShaderManager;
	VkShaderModule vertexShader;
	VkShaderModule fragmentShader;

	VkFormat surfaceFormat;
	VkFormat depthBufferFormat;

	uint32_t swapChainImageCount;
	SwapChainBuffer* paSwapChainBuffers;
	uint32_t currentBuffer;

	// TODO: Windows stuff - abstract out!
	HINSTANCE hInstance;
	HWND hWnd;
};

// creation
void VulkanRenderer_CreateCommandPool(VulkanRenderer* pThis);
void VulkanRenderer_CreateSurface(VulkanRenderer* pThis);
void VulkanRenderer_CacheSurfaceFormats(VulkanRenderer* pThis);
void VulkanRenderer_CreateSwapchain(
	VulkanRenderer* pThis,
	VkCommandBuffer setupCommandBuffer);
void VulkanRenderer_CreateShaders(VulkanRenderer* pThis);
void VulkanRenderer_CreatePipelines(VulkanRenderer* pThis);

// destruction - there should be one for every creation above
void VulkanRenderer_FreeSurface(VulkanRenderer* pThis);
void VulkanRenderer_FreeSwapchain(VulkanRenderer* pThis);

// command buffer management
// TODO: these want to be in a seperate command buffer management "class"
VkCommandBuffer VulkanRenderer_SetupCommandBuffer(VulkanRenderer* pThis);
void VulkanRenderer_DestroyCommandBuffer(
	VulkanRenderer* pThis,
	VkCommandBuffer commandBuffer);
VulkanRenderer_BeginCommandBuffer(VkCommandBuffer commandBuffer);
VulkanRenderer_EndCommandBuffer(VkCommandBuffer commandBuffer);

void VulkanRenderer_ChangeImageLayout(
	VulkanRenderer* pThis,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkCommandBuffer setupCommandBuffer);
VkFormat SelectDepthFormat(VkPhysicalDevice physicalDevice);

BOOL DeviceTypeIsSuperior(VkPhysicalDeviceType newType, VkPhysicalDeviceType oldType);

// order the devices by the kind we'd prefer to run on
VkPhysicalDeviceType aDevicePrecidents[] = {
	VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
	VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
	VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
	VK_PHYSICAL_DEVICE_TYPE_CPU,
	VK_PHYSICAL_DEVICE_TYPE_OTHER,
};

VulkanRenderer* VulkanRenderer_Create(
	uint32_t width,
	uint32_t height,
	HINSTANCE hInstance,
	HWND hWnd) {
	assert(hInstance);
	assert(hWnd);

	VulkanRenderer* pVulkanRenderer = (VulkanRenderer*)malloc(sizeof(VulkanRenderer));
	memset(pVulkanRenderer, 0, sizeof(VulkanRenderer));

	pVulkanRenderer->width = width;
	pVulkanRenderer->height = height;
	pVulkanRenderer->hInstance = hInstance;
	pVulkanRenderer->hWnd = hWnd;

	pVulkanRenderer->pShaderManager = ShaderManager_Create(
		"Resources/Shaders",
		".vert.spv",
		".frag.spv");

	const char* aszInstanceExtensionNames[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,

		// TODO: this is windows code, extract!
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
	};
	uint32_t instanceExtensionCount = sizeof(aszInstanceExtensionNames) / sizeof(const char*);

	// initialize vulkan
	VkInstanceCreateInfo createInfo = { 0 };
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.pApplicationInfo = NULL;
	createInfo.enabledLayerCount = 0;
	createInfo.ppEnabledLayerNames = NULL;
	createInfo.enabledExtensionCount = instanceExtensionCount;
	createInfo.ppEnabledExtensionNames = aszInstanceExtensionNames;
	REQUIRE_VK_SUCCESS(
		vkCreateInstance(&createInfo, NULL, &pVulkanRenderer->instance)
	);

	// get all the physical devices
	// first find the numer of devices
	uint32_t physicalDeviceCount;
	REQUIRE_VK_SUCCESS(
		vkEnumeratePhysicalDevices(
			pVulkanRenderer->instance,
			&physicalDeviceCount,
			NULL)
	);

	// then get the devices
	VkPhysicalDevice* paPhysicalDevices = (VkPhysicalDevice*)malloc(
		sizeof(VkPhysicalDevice) * physicalDeviceCount);
	REQUIRE_VK_SUCCESS(
		vkEnumeratePhysicalDevices(
			pVulkanRenderer->instance,
			&physicalDeviceCount,
			paPhysicalDevices)
	);

	VulkanRenderer_CreateSurface(pVulkanRenderer);

	assert(pVulkanRenderer->surface);

	// find one we like
	VkPhysicalDevice chosenDevice = NULL;
	VkPhysicalDeviceProperties chosenDeviceProperties;
	uint32_t chosenQueueIndex = 0;
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
		uint32_t queueIndex = 0;
		for (; queueIndex < queueFamilyPropertyCount; queueIndex++) {
			VkBool32 supportsPresent;
			REQUIRE_VK_SUCCESS(
				vkGetPhysicalDeviceSurfaceSupportKHR(
					paPhysicalDevices[deviceIndex],
					queueIndex,
					pVulkanRenderer->surface,
					&supportsPresent)
			);

			// TODO: do this check when finding a queue
			assert(supportsPresent);

			if (supportsPresent
				&& (paQueueFamilyProperties[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
				
				// if this queue supports presenting. I'll just pray that it does for now!
				queueValid = TRUE;
				break;
			}
		}

		if (!chosenDevice) {
			chosenDevice = paPhysicalDevices[deviceIndex];
			chosenDeviceProperties = deviceProperties;
			chosenQueueIndex = queueIndex;
		}
		else {
			if (DeviceTypeIsSuperior(
				deviceProperties.deviceType,
				chosenDeviceProperties.deviceType)) {

				chosenDevice = paPhysicalDevices[deviceIndex];
				chosenDeviceProperties = deviceProperties;
				chosenQueueIndex = queueIndex;
			}
		}

		// if we found a discreet GPU - that's probably ideal
		if (chosenDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			// we've struck gold!
			break;
		}

		free(paQueueFamilyProperties);
	}

	pVulkanRenderer->physicalDevice = chosenDevice;
	pVulkanRenderer->depthBufferFormat = SelectDepthFormat(chosenDevice);

	// make a logical device
	float queuePriorities[1] = { 0.5f };
	VkDeviceQueueCreateInfo deviceQueues[1] = {0};
	deviceQueues[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueues[0].pNext = NULL;
	deviceQueues[0].flags = 0;
	deviceQueues[0].queueFamilyIndex = chosenQueueIndex;
	deviceQueues[0].queueCount = 1;
	deviceQueues[0].pQueuePriorities = queuePriorities;

	const char* aszDeviceExtensionNames[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	uint32_t deviceExtensionCount = sizeof(aszDeviceExtensionNames) / sizeof(const char*);

	VkDeviceCreateInfo deviceCreateInfo = { 0 };
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = NULL;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = deviceQueues;
	deviceCreateInfo.enabledLayerCount = 0; // don't turn any layers on for now
	deviceCreateInfo.ppEnabledLayerNames = NULL;
	deviceCreateInfo.enabledExtensionCount = deviceExtensionCount; // no extensions
	deviceCreateInfo.ppEnabledExtensionNames = aszDeviceExtensionNames;
	deviceCreateInfo.pEnabledFeatures = NULL; // no special features for now
	REQUIRE_VK_SUCCESS(
		vkCreateDevice(
			chosenDevice,
			&deviceCreateInfo,
			NULL,
			&pVulkanRenderer->device)
	);

	// grab the queue!
	vkGetDeviceQueue(
		pVulkanRenderer->device,
		chosenQueueIndex,
		0,
		&pVulkanRenderer->mainQueue);
	pVulkanRenderer->queueFamilyIndex = chosenQueueIndex;

	free(paPhysicalDevices);

	VulkanRenderer_CacheSurfaceFormats(pVulkanRenderer);
	VulkanRenderer_CreateCommandPool(pVulkanRenderer);

	// TODO: this is all wrong!
	// I need to begin the command buffer
	// init the swap chain - it looks like the buffer is needed to move the images over
	// "execute_pre_present_barrier" will need it for a vkCmdPipelineBarrier
	// find whatever else needs info.cmd in the drawcube demo

	VkCommandBuffer setupBuffer = VulkanRenderer_SetupCommandBuffer(pVulkanRenderer);
	VulkanRenderer_BeginCommandBuffer(setupBuffer);

	VulkanRenderer_CreateSurface(pVulkanRenderer); // TODO: move out of active cmd buffer
	VulkanRenderer_CreateSwapchain(pVulkanRenderer, setupBuffer);

	VulkanRenderer_CreateShaders(pVulkanRenderer);
	// TODO: see what I have to do to make this NOT crash
	//VulkanRenderer_CreatePipelines(pVulkanRenderer);

	VulkanRenderer_EndCommandBuffer(setupBuffer);

	// setup fence to submit the setup buffer
	VkFenceCreateInfo fenceCreateInfo = { 0 };
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = NULL;
	fenceCreateInfo.flags = 0;
	VkFence setupFence;
	vkCreateFence(pVulkanRenderer->device, &fenceCreateInfo, NULL, &setupFence);

	VkSubmitInfo submitInfo = { 0 };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = NULL;
	submitInfo.pWaitDstStageMask = NULL;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &setupBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = NULL;

	REQUIRE_VK_SUCCESS(
		vkQueueSubmit(pVulkanRenderer->mainQueue, 1, &submitInfo, setupFence)
	);

	VkResult fenceResult;
	do {
		fenceResult = vkWaitForFences(
			pVulkanRenderer->device,
			1,
			&setupFence,
			VK_TRUE,
			FENCE_TIMEOUT);
	} while (fenceResult == VK_TIMEOUT);

	VulkanRenderer_DestroyCommandBuffer(pVulkanRenderer, setupBuffer);

	return pVulkanRenderer;
}

void VulkanRenderer_Render(VulkanRenderer* pThis) {
	assert(pThis);

	VkPresentInfoKHR presentInfo = { 0 };
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.waitSemaphoreCount = 0;
	presentInfo.pWaitSemaphores = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &pThis->swapChain;
	presentInfo.pImageIndices = &pThis->currentBuffer;
	presentInfo.pResults = NULL;

	// TODO: define fences, wait for fences (resource loading, drawing, &c)

	REQUIRE_VK_SUCCESS(
		vkQueuePresentKHR(pThis->mainQueue, &presentInfo)
	);
}

void VulkanRenderer_Destroy(VulkanRenderer* pThis) {
	assert(pThis);

	ShaderManager_Destroy(pThis->pShaderManager);
	pThis->pShaderManager = NULL;

	VulkanRenderer_FreeSurface(pThis);
	vkDestroyCommandPool(pThis->device, pThis->commandPool, NULL);
	vkDeviceWaitIdle(pThis->device);
	vkDestroyDevice(pThis->device, NULL);
	vkDestroyInstance(pThis->instance, NULL);
	free(pThis);
}

// Private Interface!

void VulkanRenderer_CreateCommandPool(VulkanRenderer* pThis) {
	VkCommandPoolCreateInfo createInfo = { 0 };
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	createInfo.queueFamilyIndex = pThis->queueFamilyIndex;

	REQUIRE_VK_SUCCESS(
		vkCreateCommandPool(
			pThis->device,
			&createInfo,
			NULL,
			&pThis->commandPool)
	);
}

void VulkanRenderer_CreateSurface(VulkanRenderer* pThis) {
	assert(pThis);
	assert(pThis->hInstance);
	assert(pThis->hWnd);
	assert(pThis->instance);

	// TODO: Win32 code here! abstract me!
	VkWin32SurfaceCreateInfoKHR createInfo = { 0 };
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.hinstance = pThis->hInstance;
	createInfo.hwnd = pThis->hWnd;
	REQUIRE_VK_SUCCESS(
		vkCreateWin32SurfaceKHR(
			pThis->instance,
			&createInfo,
			NULL,
			&pThis->surface)
	);
}

void VulkanRenderer_CacheSurfaceFormats(VulkanRenderer* pThis) {
	assert(pThis);
	assert(pThis->physicalDevice);
	assert(pThis->surface);

	uint32_t formatCount;
	REQUIRE_VK_SUCCESS(
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			pThis->physicalDevice,
			pThis->surface,
			&formatCount,
			NULL)
		);

	VkSurfaceFormatKHR* paSurfaceFormats
		= SAFE_ALLOCATE_ARRAY(VkSurfaceFormatKHR, formatCount);

	REQUIRE_VK_SUCCESS(
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			pThis->physicalDevice,
			pThis->surface,
			&formatCount,
			paSurfaceFormats)
		);

	if (formatCount == 1 && paSurfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
		assert(FALSE); // for now
		pThis->surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
	}
	else {
		pThis->surfaceFormat = paSurfaceFormats[0].format;
	}

	SAFE_FREE(paSurfaceFormats);
}

void VulkanRenderer_CreateSwapchain(
	VulkanRenderer* pThis,
	VkCommandBuffer setupCommandBuffer) {
	assert(pThis);
	assert(pThis->physicalDevice);
	assert(pThis->surface);
	assert(setupCommandBuffer);

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	REQUIRE_VK_SUCCESS(
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			pThis->physicalDevice,
			pThis->surface,
			&surfaceCapabilities)
	);

	uint32_t presentModeCount;
	REQUIRE_VK_SUCCESS(
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			pThis->physicalDevice,
			pThis->surface,
			&presentModeCount,
			NULL)
	);

	VkPresentModeKHR* paPresentModes
		= SAFE_ALLOCATE_ARRAY(VkPresentModeKHR, presentModeCount);
	REQUIRE_VK_SUCCESS(
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			pThis->physicalDevice,
			pThis->surface,
			&presentModeCount,
			paPresentModes)
	);

	VkExtent2D swapChainExtent;
	// if width or height are -1, they both are -1
	if (surfaceCapabilities.currentExtent.width == (uint32_t)-1) {
		// set to our defined width and height
		swapChainExtent.width = pThis->width;
		swapChainExtent.height = pThis->height;
	}
	else {
		swapChainExtent = surfaceCapabilities.currentExtent;
	}
	
	// Mailbox mode is the fastest, immediate is usually availalbe, and fifo is our fallback
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (uint32_t i = 0; i < presentModeCount; i++) {
		if (paPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
		if (paPresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	// figure out the number of images needed
	uint32_t desiredNumberOfSwapChainImages = surfaceCapabilities.minImageCount + 1;
	if (
		surfaceCapabilities.maxImageCount > 0
		&& desiredNumberOfSwapChainImages > surfaceCapabilities.maxImageCount) {

		// for the case of we want more than the gpu will give us
		desiredNumberOfSwapChainImages = surfaceCapabilities.maxImageCount;
	}

	// I'm not quite sure what's going on here, but I can guess
	// still haven't found the documentation for anything KHR
	VkSurfaceTransformFlagBitsKHR preTransform;
	if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else {
		preTransform = surfaceCapabilities.currentTransform;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = { 0 };
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = NULL;
	swapChainCreateInfo.flags = 0;
	swapChainCreateInfo.surface = pThis->surface;
	swapChainCreateInfo.minImageCount = desiredNumberOfSwapChainImages;
	swapChainCreateInfo.imageFormat = pThis->surfaceFormat;
	swapChainCreateInfo.imageExtent.width = swapChainExtent.width;
	swapChainCreateInfo.imageExtent.height = swapChainExtent.height;
	swapChainCreateInfo.preTransform = preTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.presentMode = swapchainPresentMode;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	swapChainCreateInfo.imageUsage
		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.queueFamilyIndexCount = 0;
	swapChainCreateInfo.pQueueFamilyIndices = NULL;

	REQUIRE_VK_SUCCESS(
		vkCreateSwapchainKHR(
			pThis->device,
			&swapChainCreateInfo,
			NULL,
			&pThis->swapChain)
	);

	REQUIRE_VK_SUCCESS(
		vkGetSwapchainImagesKHR(
			pThis->device,
			pThis->swapChain,
			&pThis->swapChainImageCount,
			NULL)
	);

	VkImage* paSwapChainImages = SAFE_ALLOCATE_ARRAY(VkImage, pThis->swapChainImageCount);
	REQUIRE_VK_SUCCESS(
		vkGetSwapchainImagesKHR(
			pThis->device,
			pThis->swapChain,
			&pThis->swapChainImageCount,
			paSwapChainImages)
	);

	pThis->paSwapChainBuffers = SAFE_ALLOCATE_ARRAY(
		SwapChainBuffer,
		pThis->swapChainImageCount);
	for (uint32_t i = 0; i < pThis->swapChainImageCount; i++) {
		SwapChainBuffer swapChainBuffer = { 0 };

		VkImageViewCreateInfo colorImageViewCreate = { 0 };
		colorImageViewCreate.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorImageViewCreate.pNext = NULL;
		colorImageViewCreate.flags = 0;

		colorImageViewCreate.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorImageViewCreate.format = pThis->surfaceFormat;
		colorImageViewCreate.components.r = VK_COMPONENT_SWIZZLE_R;
		colorImageViewCreate.components.g = VK_COMPONENT_SWIZZLE_G;
		colorImageViewCreate.components.b = VK_COMPONENT_SWIZZLE_B;
		colorImageViewCreate.components.a = VK_COMPONENT_SWIZZLE_A;
		colorImageViewCreate.subresourceRange.baseMipLevel = 0;
		colorImageViewCreate.subresourceRange.levelCount = 1;
		colorImageViewCreate.subresourceRange.baseArrayLayer = 0;
		colorImageViewCreate.subresourceRange.layerCount = 1;
		colorImageViewCreate.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VulkanRenderer_ChangeImageLayout(
			pThis,
			swapChainBuffer.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			setupCommandBuffer);
		swapChainBuffer.image = paSwapChainImages[i];
		colorImageViewCreate.image = swapChainBuffer.image;

		REQUIRE_VK_SUCCESS(
			vkCreateImageView(
				pThis->device,
				&colorImageViewCreate,
				NULL,
				&swapChainBuffer.view)
		);

		pThis->paSwapChainBuffers[i] = swapChainBuffer;
	}
	pThis->currentBuffer = 0;

	SAFE_FREE(paSwapChainImages);
	SAFE_FREE(paPresentModes);
}

void VulkanRenderer_CreateShaders(VulkanRenderer* pThis) {
	assert(pThis);
	assert(pThis->pShaderManager);
	assert(pThis->device);

	ShaderCode vertexShaderCode = ShaderManager_GetVertexShader(
		pThis->pShaderManager,
		"main");
	VkShaderModuleCreateInfo vertexShaderCreateInfo = { 0 };
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderCreateInfo.pNext = NULL;
	vertexShaderCreateInfo.flags = 0;
	vertexShaderCreateInfo.codeSize = vertexShaderCode.codeSize;
	vertexShaderCreateInfo.pCode = vertexShaderCode.pCode;
	REQUIRE_VK_SUCCESS(
		vkCreateShaderModule(
			pThis->device,
			&vertexShaderCreateInfo,
			NULL,
			&pThis->vertexShader)
	);

	ShaderCode fragmentShaderCode = ShaderManager_GetFragmentShader(
		pThis->pShaderManager,
		"main");
	VkShaderModuleCreateInfo fragmentShaderCreateInfo = { 0 };
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderCreateInfo.pNext = NULL;
	fragmentShaderCreateInfo.flags = 0;
	fragmentShaderCreateInfo.codeSize = fragmentShaderCode.codeSize;
	fragmentShaderCreateInfo.pCode = fragmentShaderCode.pCode;
	REQUIRE_VK_SUCCESS(
		vkCreateShaderModule(
			pThis->device,
			&fragmentShaderCreateInfo,
			NULL,
			&pThis->fragmentShader)
	);

	ShaderManager_CleanupShaderCode(vertexShaderCode);
	ShaderManager_CleanupShaderCode(fragmentShaderCode);

	assert(pThis->vertexShader);
	assert(pThis->fragmentShader);

	// TODO: destroy vertex shader
	// TODO: destroy fragment shader
}

void VulkanRenderer_CreatePipelines(VulkanRenderer* pThis) {
	assert(pThis);
	assert(pThis->device);

	VkPipelineShaderStageCreateInfo aShaderStageCreateInfo[2] = { 0 };
	aShaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	aShaderStageCreateInfo[0].pNext = NULL;
	aShaderStageCreateInfo[0].flags = 0;
	aShaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	aShaderStageCreateInfo[0].module = pThis->vertexShader;
	aShaderStageCreateInfo[0].pName = "main";
	aShaderStageCreateInfo[0].pSpecializationInfo = NULL;
	aShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	aShaderStageCreateInfo[1].pNext = NULL;
	aShaderStageCreateInfo[1].flags = 0;
	aShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_VERTEX_BIT;
	aShaderStageCreateInfo[1].module = pThis->fragmentShader;
	aShaderStageCreateInfo[1].pName = "main";
	aShaderStageCreateInfo[1].pSpecializationInfo = NULL;

	// create the input binding
	VkVertexInputBindingDescription inputBindingDescriptions[1] = { 0 };
	inputBindingDescriptions[0].binding = 0;
	inputBindingDescriptions[0].stride = sizeof(Vertex);
	inputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// create vertex attribute descriptions
	VkVertexInputAttributeDescription inputAttributeDescriptions[2] = { 0 };

	// position attribute
	inputAttributeDescriptions[0].location = 0;
	inputAttributeDescriptions[0].binding = 0;
	inputAttributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	inputAttributeDescriptions[0].offset = offsetof(Vertex, position);

	// color attribute
	inputAttributeDescriptions[1].location = 1;
	inputAttributeDescriptions[1].binding = 0;
	inputAttributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	inputAttributeDescriptions[1].offset = offsetof(Vertex, color);

	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = { 0 };
	vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateInfo.pNext = NULL;
	vertexInputStateInfo.flags = 0;
	vertexInputStateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateInfo.pVertexBindingDescriptions = inputBindingDescriptions;
	vertexInputStateInfo.vertexAttributeDescriptionCount = 2;
	vertexInputStateInfo.pVertexAttributeDescriptions = inputAttributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { 0 };
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.pNext = NULL;
	inputAssemblyState.flags = 0;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyState.primitiveRestartEnable = VK_FALSE;

	VkViewport aViewports[1] = { 0 };
	aViewports[0].x = 0;
	aViewports[0].y = 0;
	aViewports[0].width = (float)pThis->width;
	aViewports[0].height = (float)pThis->height;
	aViewports[0].minDepth = 0.f;
	aViewports[0].maxDepth = 1.f;

	VkRect2D aScissors[1] = { 0 };
	aScissors[0].extent.width = pThis->width;
	aScissors[0].extent.height = pThis->height;
	aScissors[0].offset.x = 0;
	aScissors[0].offset.y = 0;

	VkPipelineViewportStateCreateInfo viewportCreateInfo = { 0 };
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.pNext = NULL;
	viewportCreateInfo.flags = 0;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = aViewports;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = aScissors;

	VkPipelineRasterizationStateCreateInfo rasterizationState = { 0 };
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.pNext = NULL;
	rasterizationState.flags = 0;
	rasterizationState.depthClampEnable = VK_TRUE;
	rasterizationState.rasterizerDiscardEnable = VK_TRUE;
	rasterizationState.polygonMode = VK_POLYGON_MODE_POINT;
	rasterizationState.cullMode = VK_CULL_MODE_NONE; // TODO: actually do front-facing polys
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.depthBiasConstantFactor = 0;
	rasterizationState.depthBiasClamp = 0;
	rasterizationState.depthBiasSlopeFactor = 0;
	rasterizationState.lineWidth = 1;

	VkPipelineMultisampleStateCreateInfo multisampleState;
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.pNext = NULL;
	multisampleState.flags = 0;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.sampleShadingEnable = VK_FALSE;
	multisampleState.minSampleShading = 0;
	multisampleState.pSampleMask = NULL;
	multisampleState.alphaToCoverageEnable = VK_FALSE;
	multisampleState.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilState = { 0 };
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.pNext = NULL;
	depthStencilState.flags = 0;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.stencilTestEnable = VK_FALSE;
	depthStencilState.front.failOp = VK_STENCIL_OP_KEEP;
	depthStencilState.front.passOp = VK_STENCIL_OP_KEEP;
	depthStencilState.front.depthFailOp = VK_STENCIL_OP_KEEP;
	depthStencilState.front.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.depthFailOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilState.minDepthBounds = 0;
	depthStencilState.maxDepthBounds = 0;

	VkPipelineColorBlendAttachmentState aColorBlendAttachments[1] = { 0 };
	aColorBlendAttachments[0].blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendState = { 0 };
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.pNext = NULL;
	colorBlendState.flags = 0;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = aColorBlendAttachments;

	// Descriptor Sets/Bindings: uniforms can be in sets and bindings:
	// layout(set=0, binding=0) uniform blah{};
	VkDescriptorSetLayoutBinding aLayoutBindings0[1] = { 0 };
	aLayoutBindings0[0].binding = 0;
	aLayoutBindings0[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	aLayoutBindings0[0].descriptorCount = 1;
	aLayoutBindings0[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	aLayoutBindings0[0].pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo aDescriptorSetCreateInfos[1] = { 0 };
	aDescriptorSetCreateInfos[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	aDescriptorSetCreateInfos[0].pNext = NULL;
	aDescriptorSetCreateInfos[0].flags = 0;
	aDescriptorSetCreateInfos[0].bindingCount = 1;
	aDescriptorSetCreateInfos[0].pBindings = aLayoutBindings0;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout aSetLayouts[1] = { 0 };
	REQUIRE_VK_SUCCESS(
		vkCreateDescriptorSetLayout(
			pThis->device,
			aDescriptorSetCreateInfos,
			NULL,
			&descriptorSetLayout)
		);

	VkPipelineLayoutCreateInfo pipelineLayoutCreate = { 0 };
	pipelineLayoutCreate.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreate.pNext = NULL;
	pipelineLayoutCreate.flags = 0;
	pipelineLayoutCreate.setLayoutCount = 1;
	pipelineLayoutCreate.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutCreate.pushConstantRangeCount = 0;
	pipelineLayoutCreate.pPushConstantRanges = NULL;

	VkPipelineLayout pipelineLayout;
	REQUIRE_VK_SUCCESS(
		vkCreatePipelineLayout(
			pThis->device,
			&pipelineLayoutCreate,
			NULL,
			&pipelineLayout)
		);

	VkAttachmentDescription aRenderPassAttachments[2] = { 0 };

	// cbuffer
	aRenderPassAttachments[0].flags = 0;
	aRenderPassAttachments[0].format = pThis->surfaceFormat;
	aRenderPassAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	aRenderPassAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	aRenderPassAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	aRenderPassAttachments[0].stencilLoadOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aRenderPassAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aRenderPassAttachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	aRenderPassAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// zbuffer
	aRenderPassAttachments[1].flags = 0;
	aRenderPassAttachments[1].format = pThis->depthBufferFormat;
	aRenderPassAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	aRenderPassAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	aRenderPassAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	aRenderPassAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	aRenderPassAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	aRenderPassAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	aRenderPassAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference aColorAttachmentReferences[1] = { 0 };
	aColorAttachmentReferences[0].attachment = 0;
	aColorAttachmentReferences[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference aDepthAttachmentReferences[1] = { 0 };
	aDepthAttachmentReferences[0].attachment = 1;
	aDepthAttachmentReferences[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription aSubpasses[1] = { 0 };
	aSubpasses[0].flags = 0;
	aSubpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	aSubpasses[0].inputAttachmentCount = 0;
	aSubpasses[0].pInputAttachments = NULL;
	aSubpasses[0].colorAttachmentCount = 1;
	aSubpasses[0].pColorAttachments = aColorAttachmentReferences;
	aSubpasses[0].pResolveAttachments = NULL;
	aSubpasses[0].pDepthStencilAttachment = aDepthAttachmentReferences;
	aSubpasses[0].preserveAttachmentCount = 0;
	aSubpasses[0].pPreserveAttachments = NULL;

	VkRenderPassCreateInfo renderPassCreate = { 0 };
	renderPassCreate.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreate.pNext = NULL;
	renderPassCreate.flags = 0;
	renderPassCreate.attachmentCount = 2; // cbuffer and zbuffer
	renderPassCreate.pAttachments = aRenderPassAttachments;
	renderPassCreate.subpassCount = 1;
	renderPassCreate.pSubpasses = aSubpasses;
	renderPassCreate.dependencyCount = 0;
	renderPassCreate.pDependencies = NULL;

	// TODO: this can be entirely seperate from the pipeline create!
	VkRenderPass renderPass;
	REQUIRE_VK_SUCCESS(
		vkCreateRenderPass(
			pThis->device,
			&renderPassCreate,
			NULL,
			&renderPass)
		);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { 0 };
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext = NULL;
	pipelineCreateInfo.flags = 0;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = aShaderStageCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputStateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pTessellationState = NULL;
	pipelineCreateInfo.pViewportState = &viewportCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDynamicState = NULL;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = 0;
	pipelineCreateInfo.basePipelineIndex = 0;

	VkPipeline graphicsPipeline;
	REQUIRE_VK_SUCCESS(
		vkCreateGraphicsPipelines(
			pThis->device,
			VK_NULL_HANDLE,
			1,
			&pipelineCreateInfo,
			VK_NULL_HANDLE,
			&graphicsPipeline)
		);

	// TODO: destroy graphics pipeline
	// TODO: destroy pipeline layout
	// TODO: destroy render pass
}

void VulkanRenderer_FreeSurface(VulkanRenderer* pThis) {
	vkDestroySurfaceKHR(pThis->instance, pThis->surface, NULL);
	pThis->surface = NULL;
}

void VulkanRenderer_FreeSwapchain(VulkanRenderer* pThis) {
	// TODO: see if anything in paSwapChainBuffers needs to be explicitly destroyed
	SAFE_FREE(pThis->paSwapChainBuffers);
	pThis->swapChainImageCount = 0;
	vkDestroySwapchainKHR(pThis->device, pThis->swapChain, NULL);
	pThis->swapChain = NULL;
}

VkCommandBuffer VulkanRenderer_SetupCommandBuffer(VulkanRenderer* pThis) {
	assert(pThis);
	assert(pThis->commandPool);
	assert(pThis->device);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { 0 };
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = NULL;
	commandBufferAllocateInfo.commandPool = pThis->commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	REQUIRE_VK_SUCCESS(
		vkAllocateCommandBuffers(
			pThis->device,
			&commandBufferAllocateInfo,
			&commandBuffer)
	);

	return commandBuffer;
}

void VulkanRenderer_DestroyCommandBuffer(
	VulkanRenderer* pThis,
	VkCommandBuffer commandBuffer) {
	assert(pThis);
	assert(pThis->device);
	assert(pThis->commandPool);

	vkFreeCommandBuffers(
		pThis->device,
		pThis->commandPool,
		1,
		&commandBuffer);
}

VulkanRenderer_BeginCommandBuffer(VkCommandBuffer commandBuffer) {
	// put in setup commands
	VkCommandBufferBeginInfo beginInfo = { 0 };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = NULL;
	beginInfo.flags = 0;
	REQUIRE_VK_SUCCESS(vkBeginCommandBuffer(commandBuffer, &beginInfo));
}

VulkanRenderer_EndCommandBuffer(VkCommandBuffer commandBuffer) {
	REQUIRE_VK_SUCCESS(vkEndCommandBuffer(commandBuffer));
}

// TODO: may be optimal to also specify the command buffer that will depend on this?
void VulkanRenderer_ChangeImageLayout(
	VulkanRenderer* pThis,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkCommandBuffer setupCommandBuffer) {
	assert(setupCommandBuffer);
	assert(pThis->mainQueue);

	VkImageMemoryBarrier imageMemoryBarrier = { 0 };
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = NULL;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = 0;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange.aspectMask = aspectMask;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.layerCount = 1;

	if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}

	if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		// ensures that anything that was copying from this image was completed
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	}

	if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		// ensures any copy or cpu writes to the image are flushed
		imageMemoryBarrier.srcAccessMask
			= VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	}

	if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	}

	if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	}

	VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	vkCmdPipelineBarrier(
		setupCommandBuffer,
		srcStages,
		dstStages,
		0,
		0,
		NULL,
		0,
		NULL,
		1,
		&imageMemoryBarrier);
}

const VkFormat kaDepthFormats[] = {
	VK_FORMAT_D32_SFLOAT_S8_UINT,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM
};

VkFormat SelectDepthFormat(VkPhysicalDevice physicalDevice) {
	for (int i = 0; i < sizeof(kaDepthFormats) / sizeof(VkFormat); i++) {
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(
			physicalDevice,
			kaDepthFormats[i],
			&formatProperties);
		if (formatProperties.optimalTilingFeatures
			& VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {

			return kaDepthFormats[i];
		}
	}

	assert(FALSE);
	return VK_FORMAT_D16_UNORM;
}

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
