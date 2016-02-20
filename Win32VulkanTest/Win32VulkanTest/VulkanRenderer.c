#include "stdafx.h"

#include "VulkanRenderer.h"

#include "ShaderManager.h"

#define STRINGIFY(s) STR(s)
#define STR(s) #s
#define PRINT_VK_RESULT(result) case result: printf( STRINGIFY(result) "\n"); break;

typedef struct vertex_t {
	float position [3];
	float color [3];
} Vertex;

struct vulkan_renderer_t {
	float width;
	float height;

	VkInstance instance;
	VkDevice device;

	uint32_t queueFamilyIndex;

	// TODO: do I need this?
	VkQueue mainQueue;
	VkCommandPool commandPool;

	VkCommandBuffer setupCommandBuffer;

	ShaderManager* pShaderManager;
};

void VulkanRenderer_CreateCommandPool(VulkanRenderer* pThis);
void VulkanRenderer_CreateSetupCommandBuffer(VulkanRenderer* pThis);
void VulkanRenderer_FreeSetupCommandBuffer(VulkanRenderer* pThis);

BOOL DeviceTypeIsSuperior(VkPhysicalDeviceType newType, VkPhysicalDeviceType oldType);

// order the devices by the kind we'd prefer to run on
VkPhysicalDeviceType aDevicePrecidents[] = {
	VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
	VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
	VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
	VK_PHYSICAL_DEVICE_TYPE_CPU,
	VK_PHYSICAL_DEVICE_TYPE_OTHER,
};

VulkanRenderer* VulkanRenderer_Create(float width, float height) {
	VulkanRenderer* pVulkanRenderer = (VulkanRenderer*)malloc(sizeof(VulkanRenderer));
	memset(pVulkanRenderer, 0, sizeof(VulkanRenderer));

	pVulkanRenderer->width = width;
	pVulkanRenderer->height = height;

	pVulkanRenderer->pShaderManager = ShaderManager_Create(
		"Resources/Shaders",
		".vert.spv",
		".frag.spv");

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
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateInstance(&createInfo, NULL, &pVulkanRenderer->instance)
	);

	// get all the physical devices
	// first find the numer of devices
	uint32_t physicalDeviceCount;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkEnumeratePhysicalDevices(
			pVulkanRenderer->instance,
			&physicalDeviceCount,
			NULL)
	);

	// then get the devices
	VkPhysicalDevice* paPhysicalDevices = (VkPhysicalDevice*)malloc(
		sizeof(VkPhysicalDevice) * physicalDeviceCount);
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkEnumeratePhysicalDevices(
			pVulkanRenderer->instance,
			&physicalDeviceCount,
			paPhysicalDevices)
	);

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
			if (paQueueFamilyProperties[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
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

	// make a logical device
	float queuePriorities[1] = { 0.5f };
	VkDeviceQueueCreateInfo deviceQueues[1] = {0};
	deviceQueues[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueues[0].pNext = NULL;
	deviceQueues[0].flags = 0;
	deviceQueues[0].queueFamilyIndex = chosenQueueIndex;
	deviceQueues[0].queueCount = 1;
	deviceQueues[0].pQueuePriorities = queuePriorities;

	VkDeviceCreateInfo deviceCreateInfo = { 0 };
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = NULL;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = deviceQueues;
	deviceCreateInfo.enabledLayerCount = 0; // don't turn any layers on for now
	deviceCreateInfo.ppEnabledLayerNames = NULL;
	deviceCreateInfo.enabledExtensionCount = 0; // no extensions
	deviceCreateInfo.ppEnabledExtensionNames = NULL;
	deviceCreateInfo.pEnabledFeatures = NULL; // no special features for now
	VK_EXECUTE_REQUIRE_SUCCESS(
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

	VulkanRenderer_CreateCommandPool(pVulkanRenderer);
	VulkanRenderer_CreateSetupCommandBuffer(pVulkanRenderer);

	return pVulkanRenderer;
}

void VulkanRenderer_Render(VulkanRenderer* pThis) {
	assert(pThis);
}

void VulkanRenderer_Destroy(VulkanRenderer* pThis) {
	assert(pThis);

	ShaderManager_Destroy(pThis->pShaderManager);
	pThis->pShaderManager = NULL;

	VulkanRenderer_FreeSetupCommandBuffer(pThis);
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

	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateCommandPool(
			pThis->device,
			&createInfo,
			NULL,
			&pThis->commandPool)
	);
}

void VulkanRenderer_CreateSetupCommandBuffer(VulkanRenderer* pThis) {
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = { 0 };
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = NULL;
	commandBufferAllocateInfo.commandPool = pThis->commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	VK_EXECUTE_REQUIRE_SUCCESS(
		vkAllocateCommandBuffers(
			pThis->device,
			&commandBufferAllocateInfo,
			&pThis->setupCommandBuffer)
	);

	// put in setup commands
	VkCommandBufferBeginInfo beginInfo = { 0 };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = NULL;
	beginInfo.flags = 0;
	VK_EXECUTE_REQUIRE_SUCCESS(
		vkBeginCommandBuffer(pThis->setupCommandBuffer, &beginInfo));

	VkViewport viewport = { 0 };
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = pThis->width;
	viewport.height = pThis->height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	// shader stuff
	ShaderCode vertexShaderCode = ShaderManager_GetVertexShader(
		pThis->pShaderManager,
		"main");
	VkShaderModuleCreateInfo vertexShaderCreateInfo = { 0 };
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderCreateInfo.pNext = NULL;
	vertexShaderCreateInfo.flags = 0;
	vertexShaderCreateInfo.codeSize = vertexShaderCode.codeSize;
	vertexShaderCreateInfo.pCode = vertexShaderCode.pCode;
	VkShaderModule vertexShaderModule;
	VK_REQUIRE_SUCCESS(
		vkCreateShaderModule(
			pThis->device,
			&vertexShaderCreateInfo,
			NULL,
			&vertexShaderModule)
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
	VkShaderModule fragmentShaderModule;
	VK_REQUIRE_SUCCESS(
		vkCreateShaderModule(
			pThis->device,
			&fragmentShaderCreateInfo,
			NULL,
			&fragmentShaderModule)
		);

	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2] = { 0 };
	shaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[0].pNext = NULL;
	shaderStageCreateInfo[0].flags = 0;
	shaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageCreateInfo[0].module = vertexShaderModule;
	shaderStageCreateInfo[0].pName = "main";
	shaderStageCreateInfo[0].pSpecializationInfo = NULL;
	shaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[1].pNext = NULL;
	shaderStageCreateInfo[1].flags = 0;
	shaderStageCreateInfo[1].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageCreateInfo[1].module = fragmentShaderModule;
	shaderStageCreateInfo[1].pName = "main";
	shaderStageCreateInfo[1].pSpecializationInfo = NULL;

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

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { 0 };
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext = NULL;
	pipelineCreateInfo.flags = 0;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStageCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputStateInfo;
	//pipelineCreateInfo.pInputAssemblyState = 

	VK_EXECUTE_REQUIRE_SUCCESS(
		vkCreateGraphicsPipelines(
			pThis->device,
			VK_NULL_HANDLE,
			0,
			)
	);

	ShaderManager_CleanupShaderCode(vertexShaderCode);

	// TODO: destroy graphics pipeline
	// TODO: destroy vertex shader
	// TODO: destroy fragment shader

	VK_EXECUTE_REQUIRE_SUCCESS(vkEndCommandBuffer(pThis->setupCommandBuffer));
}

void VulkanRenderer_FreeSetupCommandBuffer(VulkanRenderer* pThis) {
	if (pThis->setupCommandBuffer) {
		vkFreeCommandBuffers(
			pThis->device,
			pThis->commandPool,
			1,
			&pThis->setupCommandBuffer);
		pThis->setupCommandBuffer = NULL;
	}
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
